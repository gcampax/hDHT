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

#include "leaf-entry.hpp"

#include <memory>

#include "node-entry.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

LeafEntry::LeafEntry(const Point& pt, HilbertValue lhv, void* data) : lhv_(lhv), m_data(data) {
    mbr_ = std::make_shared<Rectangle>(pt, pt);
}

std::shared_ptr<Rectangle> LeafEntry::getMBR() {
    return mbr_;
}

NodeEntry::HilbertValue LeafEntry::getLHV() {
    return lhv_;
}

bool LeafEntry::isLeafEntry() {
    return true;
}

}

} // namespace libhdht
