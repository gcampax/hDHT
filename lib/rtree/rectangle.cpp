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

#include "rectangle.hpp"

#include <cstdint>
#include <vector>

namespace libhdht {

namespace rtree {

Rectangle::Rectangle()
    : upper_(std::pair<uint64_t, uint64_t>(0, 0)),
      lower_(std::pair<uint64_t, uint64_t>(0, 0)) {

}

Rectangle::Rectangle(const std::pair<uint64_t, uint64_t>& upper,
                     const std::pair<uint64_t, uint64_t>& lower)
    : upper_(upper), lower_(lower) {

}

std::pair<uint64_t, uint64_t> Rectangle::getCenter() const {
    return std::make_pair((upper_.first + lower_.first) / 2,
                          (upper_.second + lower_.second) / 2);
}

bool Rectangle::intersects(const Rectangle& other) const {
    if (lower_.first > other.upper_.first ||
        other.lower_.first > upper_.first)
        return false;
    if (lower_.second > other.upper_.second ||
        other.lower_.second > upper_.second)
        return false;
    return true;
}

bool Rectangle::contains(const Rectangle& other) const {
    if (upper_.first < other.upper_.first ||
        lower_.first > other.lower_.first)
        return false;
    if (upper_.second < other.upper_.second ||
        lower_.second > other.lower_.second)
        return false;
    return true;
}

bool
Rectangle::contains(const Point &pt) const
{
    return contains(Rectangle(pt, pt));
}

Rectangle
Rectangle::intersection(const Rectangle &one, const Rectangle &two)
{
    if (!one.intersects(two))
        return Rectangle();

    auto upper = std::make_pair(
        std::min(one.upper_.first, two.upper_.first),
        std::min(one.upper_.second, two.upper_.second)
    );
    auto lower = std::make_pair(
        std::max(one.lower_.first, two.lower_.first),
        std::max(one.lower_.second, two.lower_.second)
    );
    return Rectangle(upper, lower);
}

}

} // namespace libhdht


