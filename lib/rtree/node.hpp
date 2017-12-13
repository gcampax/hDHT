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

#include "node-entry.hpp"
#include "rectangle.hpp"

#include <memory>

namespace libhdht {

namespace rtree {

const uint64_t kDefaultHilbertValue = 0;
const uint64_t kMaxCapacity = 5;

// An RTree node
class Node {
  public:
    typedef uint64_t HilbertValue;

    // Node constructor
    Node();

    // Node destructor
    ~Node();

    // ----------//
    // Accessors //
    // ----------//

    // Returns true if this Node is a leaf
    bool isLeaf() const;

    // Returns the maximum bounding rectangle (MBR) of the entries rooted at this Node
    std::shared_ptr<Rectangle> getMBR();

    // Returns the largest Hilbert value (LHV) of the entries rooted at this Node
    HilbertValue getLHV();

    // Returns the list of entries stored at this Node
    std::vector<std::shared_ptr<NodeEntry>> getEntries() const;

    // Returns a pointer to this Node's parent
    Node* getParent() const;

    // Returns a pointer to this Node's previous sibling
    Node* getPrevSibling() const;

    // Returns a pointer to this Node's next sibling
    Node* getNextSibling() const;

    // Sets the Node to a leaf node if <status> is true
    void setLeaf(bool status);

    // Returns a list of nodes to assist with the overflow handling procedure
    std::vector<Node*> getCooperatingSiblings();

    // Returns true if the Node has less than kMaxCapacity entries
    bool hasCapacity() const;

    // ----------//
    // Modifiers //
    // ----------//

    // Adds <entry> to this Node, assuming this is a leaf node
    void insertLeafEntry(std::shared_ptr<NodeEntry> entry);

    // Adds <entry> to this Node, assuming this is an internal node
    void insertInternalEntry(std::shared_ptr<NodeEntry> entry);

    // Sets the parent pointer of this Node
    void setParent(Node* node);

    // Sets the previous sibling pointer of this Node
    void setPrevSibling(Node* node);

    // Sets the next sibling pointer of this node
    void setNextSibling(Node* node);

    // Clears all entries stored at this Node
    void clearEntries();

    // Recomputes the MBR for this Node
    void adjustMBR();

    // Recomputes the LHV for this Node
    void adjustLHV();

  private:
    Node* parent_;
    Node* prev_sibling_;
    Node* next_sibling_;
    std::shared_ptr<Rectangle> mbr_;
    HilbertValue lhv_;
    bool leaf_;
    std::vector<std::shared_ptr<NodeEntry>> entries_;
};

}

} // namespace libhdht
