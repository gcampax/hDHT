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

#include "libhdht/rtree/node.hpp"
#include "libhdht/rtree/node-entry.hpp"
#include "libhdht/rtree/rectangle.hpp"

namespace libhdht {

class RTree {
  public:
    RTree();
    ~RTree();
    void insert(std::shared_ptr<Rectangle> r);
    std::vector<NodeEntry> search(std::shared_ptr<Rectangle> query);
    
  private:
    Node* chooseLeaf(std::shared_ptr<HilbertValue> hv);
    Node* handleOverflow(Node* leaf);
    Node* root_;
};

} // namespace libhdht
