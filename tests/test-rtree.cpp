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

#include "../lib/rtree/rtree.hpp"

#include <cassert>

using namespace libhdht::rtree;

static void test_search() {
    RTree rtree(32 /* max_dimension */);
    float data = 1000;
    rtree.insert(std::make_pair<uint64_t, uint64_t>(1, 1), &data);
    rtree.insert(std::make_pair<uint64_t, uint64_t>(0, 1), &data);
    rtree.insert(std::make_pair<uint64_t, uint64_t>(2, 0), &data);
    rtree.insert(std::make_pair<uint64_t, uint64_t>(3, 3), &data);
    Rectangle rectangle(std::make_pair<uint64_t, uint64_t>(0, 0),
                        std::make_pair<uint64_t, uint64_t>(2, 2));
    std::vector<std::shared_ptr<LeafEntry>> results = rtree.search(rectangle);
    assert(results.size() == 3);
    for (const auto& result : results) {
        assert(*static_cast<float*>(result->get_data()) == data);
    }
}

int main() {
    test_search();
}
