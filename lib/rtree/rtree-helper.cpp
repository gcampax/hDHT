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

std::vector<std::shared_ptr<LeafEntry>> RTreeHelper::search(
                                std::shared_ptr<Rectangle> query, Node* root) {
    std::vector<std::shared_ptr<LeafEntry>> results;
    if (root->isLeaf()) {
        for (std::shared_ptr<NodeEntry> entry : root->getEntries()) {
            assert(entry->isLeafEntry());
            if (entry->getMBR()->intersects(*query)) {
                results.push_back(std::static_pointer_cast<LeafEntry>(entry));
            }
        }
    } else {
        for (std::shared_ptr<NodeEntry> entry : root->getEntries()) {
            if (entry->getMBR()->intersects(*query)) {
                assert(!entry->isLeafEntry());
                std::vector<std::shared_ptr<LeafEntry>> temp = search(query,
                               std::static_pointer_cast<InternalEntry> (entry)->getNode());
                results.reserve(results.size() + temp.size());
                results.insert(results.end(), temp.begin(), temp.end());
            }
        }
    }
    return results;
}

Node* RTreeHelper::handleOverflow(Node* node, std::shared_ptr<NodeEntry> entry,
                                  std::vector<Node*>& siblings) {
    std::vector<std::shared_ptr<NodeEntry>> entries;
    for (const auto& e : node->getEntries()) {
        entries.push_back(e);
    }
    siblings = node->getCooperatingSiblings();
    for (Node* sibling : siblings) {
        for (const auto& e : sibling->getEntries()) {
            entries.push_back(e);
        }
        sibling->clearEntries();
    }
    entries.push_back(entry);

    for (Node* sibling : siblings) {
        if (sibling->hasCapacity()) {
            RTreeHelper::distributeEntries(entries, siblings);
            return nullptr;
        }
    }
    Node* new_node = new Node();
    Node* prevSibling = node->getPrevSibling();
    new_node->setPrevSibling(prevSibling);
    if (prevSibling != nullptr) {
        prevSibling->setNextSibling(new_node);
    }
    new_node->setNextSibling(node);
    node->setPrevSibling(new_node);
    siblings.push_back(new_node); // TODO(insert this in the correct position)
    RTreeHelper::distributeEntries(entries, siblings);
    return new_node;
}

Node* RTreeHelper::adjustTree(Node* root, Node* node, Node* new_node,
                              std::vector<Node*>& siblings) {
    Node* new_parent = nullptr;
    std::vector<Node*> local_siblings;
    std::vector<Node*> new_siblings;
    bool done = false;
    while (!done) {
        Node* parent = node->getParent();
        if (parent == nullptr) {
            done = true;

            if (new_node != nullptr) {
              std::shared_ptr<InternalEntry> node_entry = std::make_shared<InternalEntry>(node);
              std::shared_ptr<InternalEntry> new_node_entry = std::make_shared<InternalEntry>(new_node);
              root = new Node();
              root->insertInternalEntry(node_entry);
              root->insertInternalEntry(new_node_entry);
            }
        } else {
            if (new_node != nullptr) {
                std::shared_ptr<InternalEntry> new_node_entry =
                    std::make_shared<InternalEntry>(new_node);
                if (parent->hasCapacity()) {
                    parent->insertInternalEntry(new_node_entry);
                    parent->adjustLHV();
                    parent->adjustMBR();
                    new_siblings.push_back(parent);
                } else {
                    new_parent = RTreeHelper::handleOverflow(parent,
                                                             new_node_entry,
                                                             new_siblings);
                }
            } else {
                new_siblings.push_back(parent);
            }

            for (auto& sibling : local_siblings) {
                sibling->getParent()->adjustLHV();
                sibling->getParent()->adjustMBR();
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

void RTreeHelper::distributeEntries(
                        std::vector<std::shared_ptr<NodeEntry>>& entries,
                        std::vector<Node*>& siblings) {
    int entries_per_node = (int) (std::ceil(
          static_cast<float> (entries.size()) / siblings.size()));
    
    uint32_t entry_idx = 0;
    for (const auto& sibling : siblings) {
        for (int i = 0; i < entries_per_node && entry_idx < entries.size();
             i++) {
            if (entries[entry_idx]->isLeafEntry()) {
                sibling->insertLeafEntry(entries[entry_idx++]);
            } else {
                sibling->insertInternalEntry(entries[entry_idx++]);
            }
        }
        sibling->adjustLHV();
        sibling->adjustMBR();
    }
}

}

} // namespace libhdht
