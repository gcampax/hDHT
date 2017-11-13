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

// NOTE: this is NOT "pragma once"
// this file is included multiple times, defining different values
// for the macros, to put the protocol definition in various places

// Note: you cannot have requests with no parameters (due to how the
// preprocessors expands __VA_ARGS__ macros, it would create a dangling comma)

begin_class(Master)
    request(void, register_server, NodeID, std::shared_ptr<ServerProxy>)
    request(void, register_client, double, double)
end_class

begin_class(Server)
end_class
