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
#include "node.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

// A data entry for an internal RTree node.
class InternalEntry : public NodeEntry {
  public:
    InternalEntry(Node* node);
    ~InternalEntry();
    std::shared_ptr<Rectangle> getMBR() override;
    HilbertValue getLHV() override;
    Node* getNode();
    bool isLeafEntry() override;
  private:
    Node* node_;
};

} // namespace rtree

} // namespace libhdht

