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
    bool is_leaf() const;

    // Returns the maximum bounding rectangle (MBR) of the entries rooted at this Node
    std::shared_ptr<Rectangle> get_MBR();

    // Returns the largest Hilbert value (LHV) of the entries rooted at this Node
    HilbertValue get_LHV();

    // Returns the list of entries stored at this Node
    std::vector<std::shared_ptr<NodeEntry>> get_entries() const;

    // Returns a pointer to this Node's parent
    Node* get_parent() const;

    // Returns a pointer to this Node's previous sibling
    Node* get_prev_sibling() const;

    // Returns a pointer to this Node's next sibling
    Node* get_next_sibling() const;

    // Sets the Node to a leaf node if <status> is true
    void set_leaf(bool status);

    // Returns a list of nodes to assist with the overflow handling procedure
    std::vector<Node*> get_cooperating_siblings();

    // Returns true if the Node has less than kMaxCapacity entries
    bool has_capacity() const;

    // ----------//
    // Modifiers //
    // ----------//

    // Adds <entry> to this Node, assuming this is a leaf node
    void insert_leaf_entry(std::shared_ptr<NodeEntry> entry);

    // Adds <entry> to this Node, assuming this is an internal node
    void insert_internal_entry(std::shared_ptr<NodeEntry> entry);

    // Sets the parent pointer of this Node
    void set_parent(Node* node);

    // Sets the previous sibling pointer of this Node
    void set_prev_sibling(Node* node);

    // Sets the next sibling pointer of this node
    void set_next_sibling(Node* node);

    // Clears all entries stored at this Node
    void clear_entries();

    // Recomputes the MBR for this Node
    void adjust_MBR();

    // Recomputes the LHV for this Node
    void adjust_LHV();

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
