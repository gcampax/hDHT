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

#include "rtree.hpp"

#include "leaf-entry.hpp"
#include "node.hpp"
#include "rtree-helper.hpp"
#include "../hilbert-values.hpp"

namespace libhdht {

namespace rtree {

RTree::HilbertValue RTree::hilbert_value_for_point(const Point& pt) const
{
    return hilbert_values::xy2d(m_max_dimension, pt.first, pt.second);
}

void RTree::insert(const Point& pt, void *data) {
    HilbertValue hv(hilbert_value_for_point (pt));
    std::shared_ptr<LeafEntry> entry = std::make_shared<LeafEntry>(pt, hv, data);
    std::vector<Node*> siblings;

    // Find the appropriate leaf node
    Node* leaf = RTreeHelper::choose_leaf(this->root_, hv);
    if (leaf == nullptr) {
        leaf = new Node();
        leaf->set_leaf(true);
        root_ = leaf;
    }

    // Insert r in a leaf node
    Node* new_leaf = nullptr;
    if (leaf->has_capacity()) {
        leaf->insert_leaf_entry(entry);
        leaf->adjust_mbr();
        leaf->adjust_lhv();
    } else {
        new_leaf = RTreeHelper::handle_overflow(leaf, entry, siblings);
    }

    // Propogate changes upward
    this->root_ = RTreeHelper::adjust_tree(this->root_, leaf, new_leaf, siblings);

    m_size++;
}

}

} // namespace libhdht
