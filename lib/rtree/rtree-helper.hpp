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
    static std::vector<std::shared_ptr<LeafEntry>> search(
                                 std::shared_ptr<Rectangle> query, Node* root);

    static Node* adjustTree(Node* root, Node* leaf, Node* new_leaf,
                            std::vector<Node*>& siblings);

    static Node* handleOverflow(Node* node, std::shared_ptr<NodeEntry> entry,
                                std::vector<Node*>& siblings);

    static void distributeEntries(
                        std::vector<std::shared_ptr<NodeEntry>>& entries,
                        std::vector<Node*>& siblings);

    template<typename Callback>
    static void foreach_entry(Node* root, const Callback& callback)
    {
        if (root->isLeaf()) {
            for (const auto& entry : root->getEntries()) {
                assert(entry->isLeafEntry());
                callback(std::static_pointer_cast<LeafEntry>(entry));
            }
        } else {
            for (const auto& entry : root->getEntries()) {
                assert(!entry->isLeafEntry());
                foreach_entry(std::static_pointer_cast<InternalEntry>(entry)->getNode(), callback);
            }
        }
    }
};

}

} // namespace libhdht
