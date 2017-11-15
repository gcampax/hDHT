#include "libhdht/rtree/rtree-helper.hpp"

#include <memory>
#include <vector>

#include "libhdht/rtree/node-entry.hpp"
#include "libhdht/rtree/rectangle.hpp"

namespace libhdht {

std::vector<NodeEntry> RTreeHelper::search(std::shared_ptr<Rectangle> query,
                                           Node* root) {
    std::vector<NodeEntry> results;
    return results;
}

void RTreeHelper::adjustTree(std::vector<Node*> siblings) {
    
}

Node* RTreeHelper::handleOverflow(Node* n, std::shared_ptr<Rectangle> r) {
    return nullptr;
}

} // namespace libhdht
