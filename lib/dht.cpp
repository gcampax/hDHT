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
#include "hilbert-values.hpp"

#include "rtree/rtree.hpp"

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
    auto it = m_ranges.upper_bound(node);
    it--;

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

    log(LOG_DEBUG, "Adding remote range %s from peer %s", range.to_string().c_str(),
        proxy->get_address().to_string().c_str());
    auto it = m_ranges.lower_bound(range.from());

    const NodeIDRange* current_range = &it->second->get_range();
    assert(current_range->from() == it->first);

    if (range == *current_range) {
        if (it->second->is_local())
            return false;

        RemoteServerNode *server = static_cast<RemoteServerNode*>(it->second);
        server->set_proxy(proxy);
        return true;
    }

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
            if (copy == m_ranges.end())
                break;
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
        log(LOG_DEBUG, "Found existing covering range %s", current_range->to_string().c_str());

        // we need to split current_range
        assert(current_range->contains(range));
        // if the two masks are equal, then current_range and range are the same
        // because current_range.from() <= range.from() and there are no holes
        assert(current_range->mask() < range.mask());
        ServerNode *current_node = it->second;

        if (current_node->is_local()) {
            log(LOG_DEBUG, "The current range is local, refusing to overwrite");
            return false;
        }

        for (uint8_t i = current_range->mask(); i < range.mask(); i++) {
            assert(!current_range->from().bit_at(i));

            log(LOG_DEBUG, "Splitting range %s at bit %u", current_range->to_string().c_str(), i);
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
        log(LOG_DEBUG, "Replacing remote range");
        RemoteServerNode *new_node = new RemoteServerNode(range, proxy);
        delete current_node;
        it->second = new_node;
    }

    return true;
}

void
Table::add_local_server_node(const NodeIDRange& range, LocalServerNode *previous)
{
    // there are two possibilities here: either the range
    // is wholly contained in an existing range, or the range
    // wholly contains the existing node, and part of the successors
    // there are not other cases because ranges are power of two sized

    log(LOG_DEBUG, "Adding local range %s", range.to_string().c_str());
    auto it = m_ranges.lower_bound(range.from());

    const NodeIDRange* current_range = &it->second->get_range();
    assert(current_range->from() == it->first);

    if (range.contains(*current_range)) {
        LocalServerNode *new_node;
        if (previous == nullptr)
            new_node = new LocalServerNode(range, m_resolution);
        else
            new_node = previous;

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
        log(LOG_DEBUG, "Found existing covering range %s", current_range->to_string().c_str());

        // if the two masks are equal, then current_range and range are the same
        // because current_range.from() <= range.from() and there are no holes
        assert(current_range->mask() < range.mask());
        ServerNode *current_node = it->second;

        for (uint8_t i = current_range->mask(); i < range.mask(); i++) {
            assert(!current_range->from().bit_at(i));

            log(LOG_DEBUG, "Splitting range %s at bit %u", current_range->to_string().c_str(), i);
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

        if (current_node->is_local()) {
            log(LOG_DEBUG, "Found existing local range");

            if (previous == nullptr) {
                // nothing to do! LocalServerNode::split() already took care of putting
                // the clients in the right place
            } else {
                // move over the client nodes from the server node we were supposed to
                // add to the current one
                static_cast<LocalServerNode*>(current_node)->adopt_nodes(previous);
                delete previous;
            }
        } else {
            log(LOG_DEBUG, "Replacing remote range");
            // replace the RemoteServerNode with a local one
            ServerNode *new_node;
            if (previous == nullptr)
                new_node = new LocalServerNode(range, m_resolution);
            else
                new_node = previous;
            delete current_node;
            it->second = new_node;
        }
    }
}

ClientNode*
Table::get_or_create_client_node(const NodeID& id, const GeoPoint2D& pt)
{
    if (!id.is_valid())
        return get_or_create_client_node(get_node_id_for_point(pt), pt);

    auto it = m_clients.find(id);
    if (it != m_clients.end())
        return it->second;

    ServerNode *server_node = find_controlling_server(id);
    if (!server_node->is_local())
        return nullptr; // not our problem

    LocalServerNode *local = static_cast<LocalServerNode*>(server_node);
    local->prepare_insert();
    ClientNode *new_node = new ClientNode(id, pt);

    try {
        m_clients.insert(std::make_pair(id, new_node));
    } catch(const std::bad_alloc& e) {
        delete new_node;
        throw;
    }

    local->add_client(new_node);
    return new_node;
}

ClientNode*
Table::get_existing_client_node(const NodeID &id)
{
    auto it = m_clients.find(id);
    if (it != m_clients.end())
        return it->second;
    else
        return nullptr;
}

ServerNode *
Table::move_client(ClientNode* node, const GeoPoint2D & pt)
{
    ServerNode *existing = find_controlling_server(node->get_id());
    assert(existing->is_local());

    node->set_coordinates(pt);

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

void
Table::forget_server(ServerNode* node)
{
    delete node;
}

void
Table::debug_dump_table() const
{
    log(LOG_DEBUG, "--- begin table dump ---");
    for (const auto& it : m_ranges) {
        const auto& range = it.second->get_range();
        assert(it.first == range.from());
        auto range_str = range.to_string();
        log(LOG_DEBUG, "Range %p: %s", it.second, range_str.c_str());

        if (it.second->is_local()) {
            auto local = static_cast<LocalServerNode*>(it.second);
            local->foreach_client([](ClientNode* client) {
                log(LOG_DEBUG, "Owns client %p", client);
            });
        }
    }

    for (const auto& it : m_clients) {
        const auto& node_id = it.second->get_id();
        assert(it.first == node_id);
        auto node_str = node_id.to_string();
        const auto& coord = it.second->get_coordinates();

        log(LOG_DEBUG, "Client %p at id %s (%g, %g)", it.second, node_str.c_str(), coord.latitude, coord.longitude);
        for (const auto& meta_it : it.second->get_all_metadata())
            log(LOG_DEBUG, "Meta: %s = %s", meta_it.first.c_str(), meta_it.second.c_str());
    }

    log(LOG_DEBUG, "--- end table dump ---");
}

const int LOAD_THRESHOLD = 5000;

void
Table::load_balance_with_peer(std::shared_ptr<protocol::ServerProxy> proxy, std::function<void(LoadBalanceAction, ServerNode*)> callback)
{
    for (auto it = m_ranges.begin(); it != m_ranges.end(); it++) {
        ServerNode *server = it->second;
        if (server->is_local()) {
            LocalServerNode *local_server = static_cast<LocalServerNode*>(server);

            // Load balancing algorithm: if the range is bigger than resolution/2 (log size)
            // we always split it
            // keep the first half, send the second half
            if (local_server->get_range().mask() < m_resolution/2) {
                LocalServerNode *from_split = local_server->split();
                RemoteServerNode *replacement = new RemoteServerNode(from_split->get_range(), proxy);

                auto insert_result = m_ranges.insert(std::make_pair(replacement->get_range().from(), replacement));
                it = insert_result.first;
                callback(LoadBalanceAction::InformPeer, local_server);
                callback(LoadBalanceAction::RelinquishRange, from_split);
            } else {
                // if not, we check the load on the range: if bigger than a threshold, we split the
                // node
                // we keep splitting the bigger node until we go below the threshold or the table
                // is roughly balanced, and then we shed the smaller one
                LocalServerNode *iter, *bigger, *smaller;
                iter = local_server;

                bool inform_server = true;
                while (iter->load() > LOAD_THRESHOLD) {
                    inform_server = false;
                    LocalServerNode *from_split = local_server->split();
                    if (iter->load() >= from_split->load()) {
                        bigger = iter;
                        smaller = from_split;
                    } else {
                        bigger = from_split;
                        smaller = iter;
                    }

                    if (bigger->load() <= 2*smaller->load() || bigger->load() <= LOAD_THRESHOLD) {
                        // table is roughly balanced
                        if (smaller == from_split) {
                            // replace the "would be" from split with a remote server node and relinquish
                            // the local one
                            RemoteServerNode *replacement = new RemoteServerNode(from_split->get_range(), proxy);

                            auto insert_result = m_ranges.insert(std::make_pair(replacement->get_range().from(), replacement));
                            it = insert_result.first;
                            callback(LoadBalanceAction::InformPeer, bigger);
                            callback(LoadBalanceAction::RelinquishRange, smaller);
                        } else {
                            // put the local node we just created from splitting in the table
                            auto insert_result = m_ranges.insert(std::make_pair(from_split->get_range().from(), from_split));
                            // and replace the node we're about to relinquish
                            RemoteServerNode *replacement = new RemoteServerNode(iter->get_range(), proxy);
                            it->second = replacement;
                            it = insert_result.first;
                            callback(LoadBalanceAction::InformPeer, bigger);
                            callback(LoadBalanceAction::RelinquishRange, smaller);
                        }
                    } else {
                        // must keep splitting
                        // put the local node we just created from splitting in the table
                        auto insert_result = m_ranges.insert(std::make_pair(from_split->get_range().from(), from_split));
                        if (bigger == from_split) {
                            it = insert_result.first;
                            // we'll keep splitting bigger, but smaller is before bigger and we're skipping
                            // it entirely, so we let the other peer know about it
                            callback(LoadBalanceAction::InformPeer, smaller);
                        } else {
                            // otherwise bigger is before smaller, we'll touch smaller when continue
                            // walking the table
                        }
                        iter = bigger;
                        assert(it->second == iter);
                    }
                }

                // if we never did any split, just inform the other peer about this
                if (inform_server)
                    callback(LoadBalanceAction::InformPeer, iter);
            }
        } else {
            // if the node is already remote, there is nothing we can do about it
            // when the peer learns about this node and registers with the correct server,
            // the other server will perform load balancing if needed
            callback(LoadBalanceAction::InformPeer, server);
        }
    }
}

static uint64_t point_to_hilbert(uint8_t resolution, const std::pair<uint64_t, uint64_t>& pt)
{
    uint64_t hilbert_size = (1ULL << (resolution/2));
    return hilbert_values::xy2d(hilbert_size, pt.first, pt.second);
}

static std::pair<uint64_t, uint64_t> hilbert_to_point(uint8_t resolution, uint64_t hilbert_value)
{
    uint64_t hilbert_size = (1ULL << (resolution/2));
    uint64_t x, y;
    hilbert_values::d2xy(hilbert_size, hilbert_value, x, y);
    return std::make_pair(x, y);
}

class SearchRequest
{
private:
    size_t m_n_waiting = 0;
    std::function<void(rpc::Error*, std::vector<NodeID>*)> m_callback;
    std::vector<NodeID> m_accumulated_results;
    bool m_callback_called = false;

public:
    SearchRequest(std::function<void(rpc::Error*, std::vector<NodeID>*)>&& callback) : m_callback(callback) {}

    void add_local(std::vector<NodeID>& local)
    {
        m_accumulated_results.swap(local);
    }

    void perform_request(RemoteServerNode *node, const rtree::Rectangle& rect, const std::pair<uint64_t, uint64_t>& hilbert_bounds)
    {
        auto proxy = node->get_proxy();

        m_n_waiting ++;
        proxy->invoke_forward_search_clients([this](rpc::Error *error, const std::vector<NodeID>& reply) {
            m_n_waiting --;

            if (error) {
                if (!m_callback_called) {
                    m_callback_called = true;
                    m_callback(error, nullptr);
                }
            } else {
                m_accumulated_results.insert(m_accumulated_results.end(), reply.begin(), reply.end());
            }

            if (m_n_waiting == 0) {
                if (!m_callback_called) {
                    m_callback_called = true;
                    m_callback(nullptr, &m_accumulated_results);
                }

                delete this;
            }
        }, rect.get_lower(), rect.get_upper(), hilbert_bounds);
    }
};

rtree::Rectangle
Table::get_rectangle_for_points(const GeoPoint2D &upper, const GeoPoint2D &lower) const
{
    auto rectangle = rtree::Rectangle(upper.to_fixed_point(), lower.to_fixed_point());

    uint64_t mask = ((1ULL << (m_resolution/2)) - 1) << (64 - (m_resolution/2));
    rectangle.get_upper().first &= mask;
    rectangle.get_upper().first >>= (64 - (m_resolution/2));
    rectangle.get_upper().second &= mask;
    rectangle.get_upper().second >>= (64 - (m_resolution/2));
    rectangle.get_lower().first &= mask;
    rectangle.get_lower().first >>= (64 - (m_resolution/2));
    rectangle.get_lower().second &= mask;
    rectangle.get_lower().second >>= (64 - (m_resolution/2));

    assert(rectangle.get_lower() <= rectangle.get_upper());

    return rectangle;
}

void
Table::search_clients(const rtree::Rectangle& rectangle, uint64_t min_hilbert_value, uint64_t max_hilbert_value, std::function<void(rpc::Error*, std::vector<NodeID>*)> callback) const
{
    static const int rectangle_corners[4][2] = { {0, 0}, {0, 1}, {1, 1}, {1, 0} };

    uint64_t hilbert_corners[4];
    for (int i = 0; i < 4; i++)
        hilbert_corners[i] = point_to_hilbert(m_resolution, rectangle.get_corner(rectangle_corners[i]));

    int last_corner = 0;

    std::vector<std::pair<RemoteServerNode*, std::pair<uint64_t, uint64_t>>> to_query;
    std::vector<NodeID> our_response;

    for (auto i = std::max(hilbert_corners[0], min_hilbert_value); i <= max_hilbert_value;) {
        auto current_point = hilbert_to_point(m_resolution, i);
        if (rectangle.contains(current_point)) {
            // search clients at the current point

            NodeID point_id(i, m_resolution);
            ServerNode *server = find_controlling_server(point_id);

            if (server->is_local()) {
                auto from_rtree = static_cast<LocalServerNode*>(server)->search(rectangle);
                for (const auto& rtree_entry : from_rtree)
                    our_response.push_back(static_cast<ClientNode*>(rtree_entry->get_data())->get_id());
            } else {
                auto pt_begin = server->get_range().from().to_hilbert_value(m_resolution);
                auto pt_end = server->get_range().to().to_hilbert_value(m_resolution);

                to_query.push_back(std::make_pair(static_cast<RemoteServerNode*>(server),
                    std::make_pair(pt_begin, pt_end)));
            }

            NodeID next_point = server->get_range().to();
            i = next_point.to_hilbert_value(m_resolution) + 1;
        } else {
            bool found = false;
            for (int j = last_corner + 1; j < 4; j++) {
                if (hilbert_corners[j] > i) {
                    i = hilbert_corners[j];
                    found = true;
                    break;
                }
            }
            if (!found)
                break;
        }
    }

    if (to_query.empty()) {
        callback(nullptr, &our_response);
    } else {
        SearchRequest *request = new SearchRequest(std::move(callback));
        request->add_local(our_response);
        for (auto& server : to_query)
            request->perform_request(server.first, rectangle, server.second);
    }
}

}
