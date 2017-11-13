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

#include "libhdht/rtree/rectangle.hpp"

#include <vector>

namespace libhdht {

Rectangle::Rectangle(std::vector<int> upper, std::vector<int> lower)
    : upper_(upper), lower_(lower) {

}

Rectangle::~Rectangle() {

}

std::vector<int> Rectangle::getCenter() {
    std::vector<int> center;
    return center;
}
    
const std::vector<int> Rectangle::getLower() const {
    return lower_;
}

const std::vector<int> Rectangle::getUpper() const {
    return upper_;
}
 
bool Rectangle::intersects(const Rectangle& other) {
    return false;
}

bool Rectangle::contains(const Rectangle& other) {
    return false;
}

} // namespace libhdht


