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

#define LEAF_CAPACITY 5
#define INTERNAL_CAPACITY 5

namespace libhdht {

namespace rtree {

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

}

void Node::adjustLHV() {

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

}

void Node::insertLeafEntry(std::shared_ptr<NodeEntry> entry) {

}

void Node::insertInternalEntry(std::shared_ptr<NodeEntry> entry) {

}

Node* Node::findNextNode(HilbertValue hv) {
    return nullptr;
}

bool Node::hasCapacity() const {
    if (leaf_ && entries_.size() < LEAF_CAPACITY) {
        return true;
    } else if (!leaf_ && entries_.size() < INTERNAL_CAPACITY) {
        return true;
    } else {
        return false;
    }
}

}

} // namespace libhdht
