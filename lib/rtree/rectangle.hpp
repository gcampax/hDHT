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
    // Default Rectangle constructor
    Rectangle();

    // Rectangle constructor
    Rectangle(const Point& upper, const Point& lower);

    // Rectangle destructor
    ~Rectangle() {}

    // Get the center of the Rectangle
    Point get_center() const;

    // Get the lower point of the Rectangle (the lower-left corner)
    const Point& get_lower() const
    {
        return lower_;
    }

    Point& get_lower()
    {
        return lower_;
    }

    // Get the upper point of the Rectangle (the upper-right corner)
    const Point& get_upper() const
    {
        return upper_;
    }

    Point& get_upper()
    {
        return upper_;
    }

    // Get a corner of the rectangle
    Point get_corner(const int corner[2]) const
    {
        uint64_t x, y;
        if (corner[0])
            x = upper_.first;
        else
            x = lower_.first;
        if (corner[1])
            y = upper_.second;
        else
            y = lower_.second;
        return std::make_pair(x, y);
    }

    // Returns true if <other> itersects with this Rectangle
    bool intersects(const Rectangle& other) const;

    // Returns the intersecting Rectangle between <one> and <two>
    static Rectangle intersection(const Rectangle& one, const Rectangle& two);

    // Returns true if <other> is contained entirely in this Rectangle
    bool contains(const Rectangle& other) const;

    // Returns true if <pt> is contained in this Rectangle
    bool contains(const Point& pt) const;

  private:
    Point upper_;
    Point lower_;
};

}

} // namespace libhdht

