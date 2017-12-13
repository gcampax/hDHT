/*
  libhdht: a library for Hilbert-curve based Distributed Hash Tables
  Copyright 2017 Keshav Santhanam <santhanam.keshav@gmail.com>
                 Giovanni Campagna <gcampagn@cs.stanford.edu>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 3
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "rtree-helper.hpp"

#include <cmath>
#include <memory>
#include <vector>
#include <cassert>

#include "node-entry.hpp"
#include "internal-entry.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

Node* RTreeHelper::choose_leaf(Node* root, Node::HilbertValue hv_to_insert) {
    if (root == nullptr) {
        return nullptr;
    }

    if (root->is_leaf()) {
        return root;
    }

    auto it = root->get_entries().begin();
    for (; it != root->get_entries().end(); it++) {
        const auto hv = (*it)->get_LHV();
        if (hv > hv_to_insert) {
            Node* node = std::dynamic_pointer_cast<InternalEntry>(*it)->get_node();
            return RTreeHelper::choose_leaf(node, hv_to_insert);
        }
    }

    Node* node = std::dynamic_pointer_cast<InternalEntry>(*--it)->get_node();
    return RTreeHelper::choose_leaf(node, hv_to_insert);
}

std::vector<std::shared_ptr<LeafEntry>> RTreeHelper::search(const Rectangle& query, Node* root) {
    std::vector<std::shared_ptr<LeafEntry>> results;
    if (root == nullptr) {
        return results;
    }
    if (root->is_leaf()) {
        for (std::shared_ptr<NodeEntry> entry : root->get_entries()) {
            assert(entry->is_leaf_entry());
            if (entry->get_MBR()->intersects(query)) {
                results.push_back(std::static_pointer_cast<LeafEntry>(entry));
            }
        }
    } else {
        for (std::shared_ptr<NodeEntry> entry : root->get_entries()) {
            if (entry->get_MBR()->intersects(query)) {
                assert(!entry->is_leaf_entry());
                std::vector<std::shared_ptr<LeafEntry>> temp = search(query, std::static_pointer_cast<InternalEntry> (entry)->get_node());
                results.reserve(results.size() + temp.size());
                results.insert(results.end(), temp.begin(), temp.end());
            }
        }
    }
    return results;
}

Node* RTreeHelper::handle_overflow(Node* node, std::shared_ptr<NodeEntry> entry, std::vector<Node*>& siblings) {
    std::vector<std::shared_ptr<NodeEntry>> entries;
    std::vector<Node*>::iterator node_it;
    Node* new_node = nullptr;

    RTreeHelper::insert_entry(entries, entry);
    siblings = node->get_cooperating_siblings();
    for (auto it = siblings.begin(); it != siblings.end(); it++) {
        Node* sibling = *it;
        if (sibling == nullptr) continue;
        const std::vector<std::shared_ptr<NodeEntry>>& sibling_entries = sibling->get_entries();
        for (auto e : sibling_entries) {
            RTreeHelper::insert_entry(entries, e);
        }
        sibling->clear_entries();
        if (sibling == node) {
            node_it = it;
        }
    }

    size_t total_capacity = siblings.size() * kMaxCapacity;
    if (entries.size() >= total_capacity) {
        // We need a new node because there is no capacity in the existing nodes
        new_node = new Node();
        new_node->set_leaf(entry->is_leaf_entry());

        // Adjust the sibling pointers as follows:
        // node_prev_sibling -> node
        // |-> node_prev_sibling -> new_node -> node
        Node* node_prev_sibling = node->get_prev_sibling();
        if (node_prev_sibling != nullptr) {
            node_prev_sibling->set_next_sibling(new_node);
        }
        new_node->set_prev_sibling(node->get_prev_sibling());
        node->set_prev_sibling(new_node);
        new_node->set_next_sibling(node);

        // Insert the new node where the original node was
        siblings.insert(node_it, new_node);
    }

    // Redistribute the entries
    RTreeHelper::distribute_entries(entries, siblings);

    return new_node;
}

Node* RTreeHelper::adjust_tree(Node* root, Node* node, Node* new_node, std::vector<Node*>& siblings) {
    Node* new_parent = nullptr;
    std::vector<Node*> local_siblings;
    std::vector<Node*> new_siblings;
    bool done = false;
    while (!done) {
        Node* parent = node->get_parent();
        if (parent == nullptr) {
            done = true;

            if (new_node != nullptr) {
                std::shared_ptr<InternalEntry> node_entry = std::make_shared<InternalEntry>(node);
                std::shared_ptr<InternalEntry> new_node_entry = std::make_shared<InternalEntry>(new_node);
                root = new Node();
                root->set_leaf(false);
                root->insert_internal_entry(node_entry);
                root->insert_internal_entry(new_node_entry);
            }
        } else {
            if (new_node != nullptr) {
                std::shared_ptr<InternalEntry> new_node_entry = std::make_shared<InternalEntry>(new_node);
                if (parent->has_capacity()) {
                    parent->insert_internal_entry(new_node_entry);
                    parent->adjust_LHV();
                    parent->adjust_MBR();
                    new_siblings.push_back(parent);
                } else {
                    new_parent = RTreeHelper::handle_overflow(parent, new_node_entry, new_siblings);
                }
            } else {
                new_siblings.push_back(parent);
            }

            for (auto& sibling : local_siblings) {
                sibling->get_parent()->adjust_LHV();
                sibling->get_parent()->adjust_MBR();
            }

            local_siblings.clear();
            for (const auto& sibling : new_siblings) {
                local_siblings.push_back(sibling);
            }

            node = parent;
            new_node = new_parent;
        }
    }
    return root;
}

void RTreeHelper::distribute_entries(std::vector<std::shared_ptr<NodeEntry>>& entries, std::vector<Node*>& siblings) {
    int entries_per_node = (int) (std::ceil(static_cast<float> (entries.size()) / siblings.size()));

    uint32_t entry_idx = 0;
    for (const auto& sibling : siblings) {
        for (int i = 0; i < entries_per_node && entry_idx < entries.size(); i++) {
            if (entries[entry_idx]->is_leaf_entry()) {
                sibling->insert_leaf_entry(entries[entry_idx++]);
            } else {
                sibling->insert_internal_entry(entries[entry_idx++]);
            }
        }
        sibling->adjust_LHV();
        sibling->adjust_MBR();
    }
}

void RTreeHelper::insert_entry(std::vector<std::shared_ptr<NodeEntry>>& entries,
                               const std::shared_ptr<NodeEntry>& entry) {
    auto it = entries.begin();
    for (; it != entries.end(); it++) {
        if ((*it)->get_LHV() > entry->get_LHV()) {
            entries.insert(it, entry);
            return;
        }
    }
    entries.insert(it, entry);
}

}

} // namespace libhdht
