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

namespace libhdht {

Table::Table(uint8_t resolution) : m_resolution(resolution)
{
    // there is more than one client
    assert(resolution > 0);
    // we can represent nodes in that many bytes
    assert(resolution <= NodeID::size * 8);

    // Initially, the table contains a single range, which is everything
    // Everything is owned by some unknown third party
    NodeIDRange everything{};
    RemoteServerNode *new_node = new RemoteServerNode(everything, nullptr);
    try {
        m_ranges.insert(std::make_pair(everything.from(), new_node));
    } catch(const std::bad_alloc& e) {
        delete new_node;
        throw;
    }
}

Table::~Table()
{
    for (auto& iter : m_ranges)
        delete iter.second;

    for (auto& iter : m_clients)
        delete iter.second;
}

ServerNode*
Table::find_controlling_server(const NodeID& node) const
{
    auto it = m_ranges.lower_bound(node);

    // there should be no holes in the table
    assert(it->second->get_range().contains(node));
    return it->second;
}

bool
Table::add_remote_server_node(const NodeIDRange& range, std::shared_ptr<protocol::ServerProxy> proxy)
{
    // there are two possibilities here: either the range
    // is wholly contained in an existing range, or the range
    // wholly contains the existing node, and part of the successors
    // there are not other cases because ranges are power of two sized

    auto it = m_ranges.lower_bound(range.from());

    const NodeIDRange* current_range = &it->second->get_range();
    assert(current_range->from() == it->first);

    if (range.contains(*current_range)) {
        // first check if anything that overlaps range is local
        // if so, we reject this change - we're the only ones deciding when to give up our
        // clients
        auto copy = it;
        const NodeIDRange* check_range;
        do {
            if (copy->second->is_local())
                return false;

            copy++;
            check_range = &copy->second->get_range();
            assert(check_range->from() == copy->first);
        } while (range.contains(*check_range));

        // now do the actual merge
        RemoteServerNode *new_node = new RemoteServerNode(range, proxy);
        do {
            auto next = it;
            next ++;

            assert (!it->second->is_local());
            delete it->second;
            m_ranges.erase(it);
            it = next;
            if (it == m_ranges.end()) {
                current_range = nullptr;
                break;
            }
            current_range = &it->second->get_range();
            assert(current_range->from() == it->first);
        } while (range.contains(*current_range));

        // we either got to the end of the table, or got to the end of range, insert the new node and be done
        m_ranges.insert(std::make_pair(range.from(), new_node));
    } else {
        // we need to split current_range
        assert(current_range->contains(range));
        // if the two masks are equal, then current_range and range are the same
        // because current_range.from() <= range.from() and there are no holes
        assert(current_range->mask() < range.mask());
        ServerNode *current_node = it->second;

        if (current_node->is_local())
            return false;

        for (uint8_t i = current_range->mask(); i < range.mask(); i++) {
            assert(!current_range->from().bit_at(i));

            ServerNode* from_split = it->second->split();
            try {
                auto insert_result = m_ranges.insert(std::make_pair(from_split->get_range().from(), from_split));

                if (range.from().bit_at(i)) {
                    // keep the existing node at the bottom, go on and split the one
                    // at the top
                    current_node = from_split;
                    current_range = &from_split->get_range();
                    it = insert_result.first;
                } else {
                    // current_node, current_range and it stay the same
                }
            } catch(const std::bad_alloc& e) {
                delete from_split;
                throw;
            }
        }

        assert(current_range->mask() == range.mask());
        assert(current_range->from() == range.from());
        assert(!current_node->is_local());

        // replace the RemoteServerNode with a local one
        RemoteServerNode *new_node = new RemoteServerNode(range, proxy);
        delete current_node;
        it->second = new_node;
    }

    return true;
}

void
Table::add_local_server_node(const NodeIDRange& range)
{
    // there are two possibilities here: either the range
    // is wholly contained in an existing range, or the range
    // wholly contains the existing node, and part of the successors
    // there are not other cases because ranges are power of two sized

    auto it = m_ranges.lower_bound(range.from());

    const NodeIDRange* current_range = &it->second->get_range();
    assert(current_range->from() == it->first);

    if (range.contains(*current_range)) {
        LocalServerNode *new_node = new LocalServerNode(range);

        do {
            auto next = it;
            next ++;

            if (it->second->is_local()) {
                // adopt clients from the existing local node
                LocalServerNode *existing_node = static_cast<LocalServerNode*> (it->second);
                new_node->adopt_nodes(existing_node);
            }
            // else as part of onboarding we will receive RPCs to adopt the existing clients
            // from the old servers

            delete it->second;
            m_ranges.erase(it);
            it = next;
            if (it == m_ranges.end()) {
                current_range = nullptr;
                break;
            }
            current_range = &it->second->get_range();
            assert(current_range->from() == it->first);
        } while (range.contains(*current_range));

        // we either got to the end of the table, or got to the end of range, insert the new node and be done
        m_ranges.insert(std::make_pair(range.from(), new_node));
    } else {
        // we need to split current_range
        assert(current_range->contains(range));
        // if the two masks are equal, then current_range and range are the same
        // because current_range.from() <= range.from() and there are no holes
        assert(current_range->mask() < range.mask());
        ServerNode *current_node = it->second;

        for (uint8_t i = current_range->mask(); i < range.mask(); i++) {
            assert(!current_range->from().bit_at(i));

            ServerNode* from_split = it->second->split();
            auto insert_result = m_ranges.insert(std::make_pair(from_split->get_range().from(), from_split));

            if (range.from().bit_at(i)) {
                // keep the existing node at the bottom, go on and split the one
                // at the top
                current_node = from_split;
                current_range = &from_split->get_range();
                it = insert_result.first;
            } else {
                // current_node, current_range and it stay the same
            }
        }

        assert(current_range->mask() == range.mask());
        assert(current_range->from() == range.from());

        if (current_node->is_local()) {
            // nothing to do! LocalServerNode::split() already took care of putting
            // the clients in the right place
        } else {
            // replace the RemoteServerNode with a local one
            ServerNode *new_node = new LocalServerNode(range);
            delete current_node;
            it->second = new_node;
        }
    }
}

ClientNode*
Table::get_or_create_client_node(const NodeID &id)
{
    auto it = m_clients.find(id);
    if (it != m_clients.end())
        return it->second;

    ServerNode *server_node = find_controlling_server(id);
    if (!server_node->is_local())
        return nullptr; // not our problem

    LocalServerNode *local = static_cast<LocalServerNode*>(server_node);
    local->prepare_insert();
    ClientNode *new_node = new ClientNode(id);

    try {
        m_clients.insert(std::make_pair(id, new_node));
    } catch(const std::bad_alloc& e) {
        delete new_node;
        throw;
    }

    local->add_client(new_node);
    return new_node;
}

ServerNode *
Table::move_client(ClientNode* node, const GeoPoint2D & pt)
{
    ServerNode *existing = find_controlling_server(node->get_id());
    assert(existing->is_local());

    NodeID new_node_id = get_node_id_for_point(pt);
    if (new_node_id == node->get_id())
        return existing; // fast path, the node did not move enough to matter

    node->set_id(new_node_id);
    if (existing->get_range().contains(new_node_id))
        return existing; // also fast path, the node did not move enough

    ServerNode *new_server_node = find_controlling_server(new_node_id);
    assert(existing != new_server_node);

    if (new_server_node->is_local())
        static_cast<LocalServerNode*>(new_server_node)->add_client(node);
    static_cast<LocalServerNode*>(existing)->remove_client(node);

    return new_server_node;
}

void
Table::forget_client(ClientNode* node)
{
    m_clients.erase(node->get_id());
    delete node;
}

}
