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
    Point& getLower()
    {
        return lower_;
    }
    Point& getUpper()
    {
        return upper_;
    }
    Point getCorner(const int corner[2]) const
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

    bool intersects(const Rectangle& other) const;
    static Rectangle intersection(const Rectangle& one, const Rectangle& two);
    bool contains(const Rectangle& other) const;
    bool contains(const Point& pt) const;

  private:
    Point upper_;
    Point lower_;
};

}

} // namespace libhdht

