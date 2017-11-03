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

#include <libhdht/libhdht.hpp>

#include <cmath>

namespace libhdht {

static inline double
to_radians(double deg) {
    return deg * M_PI / 180.0;
}

double
GeoPoint2D::distance(GeoPoint2D& one, GeoPoint2D& two)
{
    const double R = 6371000; // meters
    double lat1 = one.latitude;
    double lat2 = two.latitude;
    double lon1 = one.longitude;
    double lon2 = two.longitude;

    // formula courtesy of http://www.movable-type.co.uk/scripts/latlong.html
    double phi1 = to_radians(lat1);
    double phi2 = to_radians(lat2);
    double deltaphi = to_radians(lat2-lat1);
    double deltalambda = to_radians(lon2-lon1);

    double x = std::sin(deltaphi/2) * std::sin(deltaphi/2) +
            std::cos(phi1) * std::cos(phi2) *
            std::sin(deltalambda/2) * std::sin(deltalambda/2);
    double c = 2 * std::atan2(std::sqrt(x), std::sqrt(1-x));

    return R * c;
}

}
