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

#include "rtree-helper.hpp"

namespace libhdht {

namespace rtree {

class RTree {
  public:
    typedef uint64_t HilbertValue;

    // Rtree constructor
    // max_dimension: the maximum size in either dimension
    RTree(uint64_t max_dimension) : m_max_dimension(max_dimension), m_size(0), root_(nullptr) {}

    // RTree destructor
    ~RTree() {
        delete root_;
    }

    // RTree copy and assign
    RTree(const RTree&) = delete;
    RTree& operator=(const RTree&) = delete;
    RTree(RTree&& from) : m_max_dimension(from.m_max_dimension), m_size(from.m_size), root_(from.root_) {
        from.root_ = nullptr;
    }
    RTree& operator=(RTree&& from) {
        delete root_;
        m_max_dimension = from.m_max_dimension;
        m_size = from.m_size;
        root_ = from.root_;
        from.root_ = nullptr;
        from.m_size = 0;
        return *this;
    }
    void swap(RTree& with)
    {
        std::swap(m_size, with.m_size);
        std::swap(m_max_dimension, with.m_max_dimension);
        std::swap(root_, with.root_);
    }

    // Return the size of this RTree
    size_t size() const
    {
        return m_size;
    }

    // Insert <r>:<data> into this RTree
    void insert(const Point& r, void* data);
    std::vector<std::shared_ptr<LeafEntry>> search(const Rectangle& query) const
    {
        return RTreeHelper::search(query, root_);
    }

    template<typename Callback>
    void foreach_entry(const Callback& callback) const
    {
        if (root_ == nullptr)
            return;
        RTreeHelper::foreach_entry(root_, callback);
    }

  private:
    HilbertValue hilbert_value_for_point(const std::pair<uint64_t, uint64_t>& pt) const;

    uint64_t m_max_dimension;
    size_t m_size; // the number of elements
    Node* chooseLeaf(Node::HilbertValue hv);
    Node* handleOverflow(Node* leaf);
    Node* root_;

};

}

} // namespace libhdht
