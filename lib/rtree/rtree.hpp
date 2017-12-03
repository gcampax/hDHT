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

#include "node.hpp"
#include "node-entry.hpp"
#include "leaf-entry.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

class RTree {
  public:
    typedef uint64_t HilbertValue;

    // N: the maximum size in either dimension
    RTree(uint64_t N);
    ~RTree();
    
    void insert(const Point& r, void* data);
    std::vector<std::shared_ptr<LeafEntry>> search(std::shared_ptr<Rectangle> query);

  private:
    HilbertValue hilbert_value_for_point(const std::pair<uint64_t, uint64_t>& pt) const;

    uint64_t m_N;
    Node* chooseLeaf(Node::HilbertValue hv);
    Node* handleOverflow(Node* leaf);
    Node* root_;
};

}

} // namespace libhdht
