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

#include <cstdint>
#include <vector>

namespace libhdht {

namespace rtree {

typedef std::pair<uint64_t, uint64_t> Point;

// An 2-dimensional rectangle
class Rectangle {
  public:
    Rectangle();
    Rectangle(const Point& upper, const Point& lower);
    ~Rectangle() {}
    Point getCenter() const;
    const Point& getLower() const
    {
        return lower_;
    }
    const Point& getUpper() const
    {
        return upper_;
    }
    bool intersects(const Rectangle& other) const;
    bool contains(const Rectangle& other) const;

  private:
    Point upper_;
    Point lower_;
};

}

} // namespace libhdht

