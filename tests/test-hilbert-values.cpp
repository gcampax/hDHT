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


#include "../lib/hilbert-values.hpp"

#include <cstdint>
#include <cassert>
#include <iostream>

using namespace libhdht::hilbert_values;

static void forward(int n, int x, int y)
{
    std::cout << "(" << x << ", " << y << ") = " << xy2d(n, x, y) << std::endl;
}

static void reverse(int n, int d)
{
    int x, y;
    d2xy(n, d, x, y);
    std::cout << "p(" << d << ") = (" << x << ", " << y << ")" << std::endl;
}

int main()
{
    std::cout << "N = 4" << std::endl;
    forward (4, 0, 0);
    std::cout << "(0, 1) = " << xy2d(4, 0, 1) << std::endl;
    std::cout << "(1, 0) = " << xy2d(4, 1, 0) << std::endl;

    std::cout << "(1, 1) = " << xy2d(4, 1, 1) << std::endl;
    std::cout << "(2, 2) = " << xy2d(4, 2, 2) << std::endl;
    std::cout << "(3, 3) = " << xy2d(4, 3, 3) << std::endl;
    reverse(4, 13);

    std::cout << "N = 2" << std::endl;
    std::cout << "(0, 0) = " << xy2d(2, 0, 0) << std::endl;
    std::cout << "(0, 1) = " << xy2d(2, 0, 1) << std::endl;
    std::cout << "(1, 0) = " << xy2d(2, 1, 0) << std::endl;
    std::cout << "(1, 1) = " << xy2d(2, 1, 1) << std::endl;

    std::cout << "N = 8" << std::endl;
    std::cout << "(0, 0) = " << xy2d(8, 0, 0) << std::endl;
    std::cout << "(0, 1) = " << xy2d(8, 0, 1) << std::endl;
    std::cout << "(1, 0) = " << xy2d(8, 1, 0) << std::endl;
    reverse(8, 0);
    reverse(8, 1);
    reverse(8, 3);
    reverse(8, 7);
    forward (8, 1, 2);
}
