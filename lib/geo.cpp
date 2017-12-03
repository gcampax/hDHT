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

#include "libhdht-private.hpp"

#include <cmath>
#include <sstream>
#include <iomanip>

namespace libhdht {

static inline double
to_radians(double deg) {
    return deg * M_PI / 180.0;
}

void
GeoPoint2D::canonicalize()
{
    longitude = fmod(longitude + 180.0, 360.0) - 180.0;
    latitude = latitude > 90.0 ? 90.0 : (latitude < -90.0 ? -90.0 : latitude);
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

std::string
GeoPoint2D::to_string() const
{
    std::ostringstream ostr;
    ostr << std::setprecision(5) << "lat: " << abs(latitude) <<
        (latitude > 0 ? " north" : " south") << ", long: " << abs(longitude) <<
        (longitude > 0 ? " east" : " west");
    return ostr.str();
}

static uint64_t longitude_to_fixpoint(double longitude)
{
    uint64_t mask = ((1ULL << 52)-1);

    assert(longitude >= -180 && longitude <= 180);

    // Longitude is easy because -180 == +180, so we can divide by 360.0
    // without losing precision
    // then we extract the mantissa and we're done
    // (this works because of IEEE754 double representation)

    if (longitude == 180.0)
        longitude = -180.0;
    longitude = 1.0 + (longitude + 180.) / 360.0;

    // For a double number between 1 and 2 (1 included 2 excluded)
    // the mantissa is exactly is its fixpoint representation
    assert(longitude >= 1.0 && longitude < 2.0);

    uint64_t longitude_bits;
    static_assert(sizeof(longitude) == sizeof(longitude_bits));
    memcpy(&longitude_bits, &longitude, sizeof(longitude_bits));
    return (longitude_bits & mask) << (64 - 52);
}

static uint64_t latitude_to_fixpoint(double latitude)
{
    uint64_t mask = ((1ULL << 52)-1);

    assert(latitude >= -90 && latitude <= 90);

    // Latitude is the annoying case
    // -90 != 90 (they're at the opposite sides of Earth)
    // so we cannot do the same bit trick as longitude

    latitude = (latitude + 90.) / (180.0);
    assert(latitude >= 0.0 && latitude <= 1.0);

    return uint64_t(floor(latitude * mask)) << (64-52);
}

std::pair<uint64_t, uint64_t>
GeoPoint2D::to_fixed_point() const
{
    assert(longitude_to_fixpoint(0) == 1ULL<<63);
    assert(longitude_to_fixpoint(-180) == 0);
    assert(longitude_to_fixpoint(180) == 0);
    assert(latitude_to_fixpoint(-90) == 0);
    // note that the last 3 nibbles stay zero because
    // doubles don't have enough precision
    assert(latitude_to_fixpoint(90) == 0xfffffffffffff000);

    return std::make_pair(latitude_to_fixpoint(latitude),
                          longitude_to_fixpoint(longitude));
}

}
