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

#include <memory>
#include <vector>
#include <cassert>

#include "node.hpp"
#include "node-entry.hpp"
#include "leaf-entry.hpp"
#include "internal-entry.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

class RTreeHelper {
  public:
    // Chooses the appropriate leaf node of the R-Tree rooted at <root> to insert <hv_to_insert> into
    static Node* choose_leaf(Node* root, Node::HilbertValue hv_to_insert);

    // Returns a list of LeafEntry pointers in the tree rooted at <root> that are contained with <query>
    static std::vector<std::shared_ptr<LeafEntry>> search(const Rectangle& query, Node* root);

    // Rebalances tree rooted at <root> after insertion
    static Node* adjust_tree(Node* root, Node* leaf, Node* new_leaf, std::vector<Node*>& siblings);

    // Takes care of the case where adding a new entry to a node increases its capacity beyond the limit
    static Node* handle_overflow(Node* node, std::shared_ptr<NodeEntry> entry, std::vector<Node*>& siblings);

    // Re-distributes <entries> among the Nodes in <siblings>
    static void distribute_entries(std::vector<std::shared_ptr<NodeEntry>>& entries, std::vector<Node*>& siblings);

    // Inserts <entry> into <entries> in increasing order of LHV
    static void insert_entry(std::vector<std::shared_ptr<NodeEntry>>& entries, const std::shared_ptr<NodeEntry>& entry);

    template<typename Callback>
    static void foreach_entry(Node* root, const Callback& callback)
    {
        if (root->is_leaf()) {
            for (const auto& entry : root->get_entries()) {
                assert(entry->is_leaf_entry());
                callback(std::static_pointer_cast<LeafEntry>(entry));
            }
        } else {
            for (const auto& entry : root->get_entries()) {
                assert(!entry->is_leaf_entry());
                foreach_entry(std::static_pointer_cast<InternalEntry>(entry)->get_node(), callback);
            }
        }
    }
};

}

} // namespace libhdht

