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

#include "node-entry.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

// A data entry for an RTree leaf node.
class LeafEntry : public NodeEntry {
  public:
    // LeafEntry constructor
    LeafEntry(const Point& pt, HilbertValue lhv, void* data);

    // LeafEntry destructor
    ~LeafEntry() {};

    // Get the maximum bounding rectangle (MBR) of this entry
    std::shared_ptr<Rectangle> get_mbr() override;

    // Get the largest Hilbert value (LHV) of this entry
    HilbertValue get_lhv() override;

    // Returns true if this entry is a LeafEntry
    bool is_leaf_entry() override;

    // Returns the data associated with this entry
    void *get_data() const
    {
        return m_data;
    }

  private:
    std::shared_ptr<Rectangle> mbr_;
    HilbertValue lhv_;
    void *m_data;
};

}

} // namespace libhdht
