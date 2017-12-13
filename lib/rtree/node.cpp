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

#include "node.hpp"

#include <memory>

#include "internal-entry.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

Node::Node() {
    prev_sibling_ = nullptr;
    next_sibling_ = nullptr;
}

Node::~Node() {

}

bool Node::is_leaf() const {
    return leaf_;
}

void Node::set_leaf(bool status) {
    leaf_ = status;
}

std::shared_ptr<Rectangle> Node::get_mbr() {
    return mbr_;
}

Node::HilbertValue Node::get_lhv() {
    return lhv_;
}

void Node::adjust_mbr() {
    if (entries_.size() == 0) {
        mbr_.reset(new Rectangle());
        return;
    }
    std::shared_ptr<Rectangle> current_mbr = entries_[0]->get_mbr();
    const Point& current_mbr_upper = current_mbr->get_upper();
    const Point& current_mbr_lower = current_mbr->get_lower();

    Point new_mbr_upper(current_mbr_upper);
    Point new_mbr_lower(current_mbr_lower);

    for (const auto& entry : entries_) {
        std::shared_ptr<Rectangle> mbr = entry->get_mbr();
        const Point& upper = mbr->get_upper();
        const Point& lower = mbr->get_lower();

        new_mbr_upper.first = std::max(upper.first, new_mbr_upper.first);
        new_mbr_upper.second = std::max(upper.second, new_mbr_upper.second);

        new_mbr_lower.first = std::min(lower.first, new_mbr_lower.first);
        new_mbr_lower.second = std::min(lower.second, new_mbr_lower.second);
    }

    mbr_.reset(new Rectangle(new_mbr_upper, new_mbr_lower));
}

void Node::adjust_lhv() {
    if (entries_.size() == 0) {
        lhv_ = kDefaultHilbertValue;
        return;
    }

    const HilbertValue current_lhv = entries_[0]->get_lhv();
    HilbertValue new_lhv = current_lhv;

    for (const auto& entry : entries_) {
        new_lhv = std::max(new_lhv, entry->get_lhv());
    }

    lhv_ = new_lhv;
}

std::vector<std::shared_ptr<NodeEntry>> Node::get_entries() const {
    return entries_;
}

Node* Node::get_parent() const {
    return parent_;
}

Node* Node::get_prev_sibling() const {
    return prev_sibling_;
}

Node* Node::get_next_sibling() const {
    return next_sibling_;
}

void Node::set_parent(Node* node) {
    this->parent_ = node;
}

void Node::set_prev_sibling(Node* node) {
    this->prev_sibling_ = node;
}

void Node::set_next_sibling(Node* node) {
    this->next_sibling_ = node;
}

std::vector<Node*> Node::get_cooperating_siblings() {
    std::vector<Node*> cooperating_siblings;

    if (this->prev_sibling_ != nullptr) {
        cooperating_siblings.push_back(this->prev_sibling_);
    }
    cooperating_siblings.push_back(this);
    if (this->next_sibling_ != nullptr) {
        cooperating_siblings.push_back(this->next_sibling_);
    }

    return cooperating_siblings;
}

void Node::clear_entries() {
    entries_.clear();
}

void Node::insert_leaf_entry(std::shared_ptr<NodeEntry> entry) {
    // TODO(keshav2): Assert node is leaf
    // TODO(keshav2): Assert node has capacity
    const HilbertValue entry_lhv = entry->get_lhv();
    auto it = entries_.begin();
    for (; it != entries_.end(); it++) {
        if (entry_lhv < (*it)->get_lhv()) {
            entries_.insert(it, entry);
            return;
        }
    }
    entries_.insert(it, entry);
}

void Node::insert_internal_entry(std::shared_ptr<NodeEntry> entry) {
    // TODO(keshav2): Assert node is internal
    // TODO(keshav2): Assert node has capacity
    const HilbertValue entry_lhv = entry->get_lhv();
    auto it = entries_.begin();
    bool inserted = false;
    for (; it != entries_.end(); it++) {
        if (entry_lhv < (*it)->get_lhv()) {
            it = entries_.insert(it, entry);
            inserted = true;
            break;
        }
    }
    if (!inserted) {
        it = entries_.insert(it, entry);
    }
    Node* node = std::dynamic_pointer_cast<InternalEntry>(*it)->get_node();;

    // Set parent
    node->set_parent(this);

    // Set previous sibling
    if (it != entries_.begin()) {
        auto prev_it = it;
        prev_it--;
    node->set_prev_sibling(std::dynamic_pointer_cast<InternalEntry>(*prev_it)->get_node());
    } else {
        node->set_prev_sibling(nullptr);
    }

    // Set next sibling
    auto end = entries_.end();
    end--;
    if (it != end) {
        auto next_it = it;
        next_it++;
        node->set_next_sibling(std::dynamic_pointer_cast<InternalEntry>(*next_it)->get_node());
    } else {
        node->set_next_sibling(nullptr);
    }
}

bool Node::has_capacity() const {
    return entries_.size() < kMaxCapacity;
}

}

} // namespace libhdht
