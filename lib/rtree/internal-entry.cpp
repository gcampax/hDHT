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

#include "internal-entry.hpp"

#include <memory>

#include "node-entry.hpp"
#include "node.hpp"
#include "rectangle.hpp"

namespace libhdht {

namespace rtree {

InternalEntry::InternalEntry(Node* node) : node_(node) {

}

InternalEntry::~InternalEntry() {

}

std::shared_ptr<Rectangle> InternalEntry::getMBR() {
    return node_->getMBR();
}

NodeEntry::HilbertValue InternalEntry::getLHV() {
    return node_->getLHV();
}

Node* InternalEntry::getNode() {
    return node_;
}

bool InternalEntry::isLeafEntry() {
    return false;
}

}

} // namespace libhdht


