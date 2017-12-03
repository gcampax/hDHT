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

#pragma once

#include "libhdht/rtree/hilbert-value.hpp"
#include "libhdht/rtree/node-entry.hpp"
#include "libhdht/rtree/rectangle.hpp"

#include <memory>

namespace libhdht {

// An RTree node
class Node {
  public:
    Node();
    ~Node();
    bool isLeaf() const;
    std::shared_ptr<Rectangle> getMBR();
    std::shared_ptr<HilbertValue> getLHV();
    void adjustMBR();
    void adjustLHV();
    const std::vector<std::shared_ptr<NodeEntry>> getEntries() const;
    void insertLeafEntry(std::shared_ptr<NodeEntry> entry);
    void insertInternalEntry(std::shared_ptr<NodeEntry> entry);
    Node* getParent();
    Node* getPrevSibling();
    Node* getNextSibling();
    void setPrevSibling(Node* node);
    void setNextSibling(Node* node);
    std::vector<Node*> getCooperatingSiblings();
    void clearEntries();
    Node* findNextNode(std::shared_ptr<HilbertValue> hv);
    bool hasCapacity() const;

  private:
    Node* prev_sibling_;
    Node* next_sibling_;
    std::shared_ptr<Rectangle> mbr_;
    std::shared_ptr<HilbertValue> lhv_;
    bool leaf_;
    std::vector<std::shared_ptr<NodeEntry>> entries_;
};

} // namespace libhdht
