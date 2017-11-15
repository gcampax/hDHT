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

Node* RTreeHelper::handleOverflow(Node* node, std::shared_ptr<NodeEntry> entry,
                                  std::vector<Node*>& siblings) {
    std::vector<std::shared_ptr<NodeEntry>> entries;
    for (const auto& e : node->getEntries()) {
        entries.push_back(e);
    }
    siblings = node->getCooperatingSiblings();
    for (Node* sibling : siblings) {
        for (const auto& e : sibling->getEntries()) {
            entries.push_back(e);
        }
        sibling->clearEntries();
    }
    entries.push_back(entry);

    for (Node* sibling : siblings) {
        if (sibling->hasCapacity()) {
            RTreeHelper::distributeEntries(entries, siblings);
            return nullptr;
        }
    }
    Node* new_node = new Node();
    Node* prevSibling = node->getPrevSibling();
    new_node->setPrevSibling(prevSibling);
    if (prevSibling != nullptr) {
        prevSibling->setNextSibling(new_node);
    }
    new_node->setNextSibling(node);
    node->setPrevSibling(new_node);
    siblings.push_back(new_node); // TODO(insert this in the correct position)
    RTreeHelper::distributeEntries(entries, siblings);
    return new_node;
}

Node* RTreeHelper::adjustTree(Node* root, Node* leaf, Node* new_leaf,
                              std::vector<Node*>& siblings) {
    return nullptr;
}

void RTreeHelper::distributeEntries(
                        std::vector<std::shared_ptr<NodeEntry>>& entries,
                        std::vector<Node*>& siblings) {

}

} // namespace libhdht
