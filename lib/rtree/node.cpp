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

#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

const uint64_t kDefaultHilbertValue = 0;
const uint64_t kLeafCapacity = 5;
const uint64_t kInternalCapacity = 5;

Node::Node() {

}

Node::~Node() {

}

bool Node::isLeaf() const {
    return leaf_;
}

std::shared_ptr<Rectangle> Node::getMBR() {
    return mbr_;
}

Node::HilbertValue Node::getLHV() {
    return lhv_;
}

void Node::adjustMBR() {
    if (entries_.size() == 0) {
        mbr_.reset(new Rectangle());
        return;
    }
    std::shared_ptr<Rectangle> current_mbr = entries_[0]->getMBR();
    const Point& current_mbr_upper = current_mbr->getUpper();
    const Point& current_mbr_lower = current_mbr->getLower();

    Point new_mbr_upper(current_mbr_upper);
    Point new_mbr_lower(current_mbr_lower);

    for (const auto& entry : entries_) {
        std::shared_ptr<Rectangle> mbr = entry->getMBR();
        const Point& upper = mbr->getUpper();
        const Point& lower = mbr->getLower();

        new_mbr_upper.first = std::max(upper.first, new_mbr_upper.first);
        new_mbr_upper.second = std::max(upper.second, new_mbr_upper.second);

        new_mbr_lower.first = std::min(lower.first, new_mbr_lower.first);
        new_mbr_lower.second = std::min(lower.second, new_mbr_lower.second);
    }

    mbr_.reset(new Rectangle(new_mbr_upper, new_mbr_lower));
}

void Node::adjustLHV() {
    if (entries_.size() == 0) {
        lhv_ = kDefaultHilbertValue;
    }

    const HilbertValue current_lhv = entries_[0]->getLHV();
    HilbertValue new_lhv = current_lhv;

    for (const auto& entry : entries_) {
        new_lhv = std::max(new_lhv, entry->getLHV());
    }

    lhv_ = new_lhv;
}

const std::vector<std::shared_ptr<NodeEntry>> Node::getEntries() const {
    return entries_;
}

Node* Node::getParent() {
    return parent_;
}

Node* Node::getPrevSibling() {
    return prev_sibling_;
}

Node* Node::getNextSibling() {
    return next_sibling_;
}

void Node::setPrevSibling(Node* node) {
    this->prev_sibling_ = node;
}

void Node::setNextSibling(Node* node) {
    this->next_sibling_ = node;
}

std::vector<Node*> Node::getCooperatingSiblings() {
    std::vector<Node*> cooperating_siblings;

    cooperating_siblings.push_back(this->prev_sibling_);
    cooperating_siblings.push_back(this);
    cooperating_siblings.push_back(this->next_sibling_);

    return cooperating_siblings;
}

void Node::clearEntries() {
    entries_.clear();
}

void Node::insertLeafEntry(std::shared_ptr<NodeEntry> entry) {
    // TODO(keshav2): Assert node is leaf
    // TODO(keshav2): Assert node has capacity
    const HilbertValue entry_lhv = entry->getLHV();
    for (auto it = entries_.begin(); it != entries_.end(); it++) {
        if (entry_lhv < (*it)->getLHV()) {
            entries_.insert(it, entry);
            return;
        }
    }
}

void Node::insertInternalEntry(std::shared_ptr<NodeEntry> entry) {
    // TODO(keshav2): Assert node is internal
    // TODO(keshav2): Assert node has capacity
    // TODO(keshav2): Implement insert
}

Node* Node::findNextNode(HilbertValue hv) {
    return nullptr;
}

bool Node::hasCapacity() const {
    if (leaf_ && entries_.size() < kLeafCapacity) {
        return true;
    } else if (!leaf_ && entries_.size() < kInternalCapacity) {
        return true;
    } else {
        return false;
    }
}

}

} // namespace libhdht
