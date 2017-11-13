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

#include "rtree/hilbert-value.hpp"
#include "rtree/rectangle.hpp"

namespace libhdht {

// A data entry for an RTree node. Can either belong to a leaf node
// or an internal node.
class NodeEntry {
  public:
    virtual ~NodeEntry();
    virtual std::shared_ptr<HilbertValue> getMBR()=0;
    virtual std::shared_ptr<Rectangle> getLHV()=0;
    virtual bool isLeafEntry()=0;
};

} // namespace libhdht


