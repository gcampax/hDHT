#include "libhdht/rtree/rtree-helper.hpp"

#include <memory>
#include <vector>

#include "libhdht/rtree/node-entry.hpp"
#include "libhdht/rtree/internal-entry.hpp"
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

Node* RTreeHelper::adjustTree(Node* root, Node* node, Node* new_node,
                              std::vector<Node*>& siblings) {
    Node* new_parent = nullptr;
    std::vector<Node*> local_siblings;
    std::vector<Node*> new_siblings;
    bool done = false;
    while (!done) {
        Node* parent = node->getParent();
        if (parent == nullptr) {
            done = true;

            if (new_node != nullptr) {
              std::shared_ptr<InternalEntry> node_entry(
                  new InternalEntry(node));
              std::shared_ptr<InternalEntry> new_node_entry(
                  new InternalEntry(new_node));
              root = new Node();
              root->insertInternalEntry(node_entry);
              root->insertInternalEntry(new_node_entry);
            }
        } else {
            if (new_node != nullptr) {
                std::shared_ptr<InternalEntry> new_node_entry(
                    new InternalEntry(new_node));
                if (parent->hasCapacity()) {
                    parent->insertInternalEntry(new_node_entry);
                    parent->adjustLHV();
                    parent->adjustMBR();
                    new_siblings.push_back(parent);
                } else {
                    new_parent = RTreeHelper::handleOverflow(parent,
                                                             new_node_entry,
                                                             new_siblings);
                }
            } else {
                new_siblings.push_back(parent);
            }

            for (auto& sibling : local_siblings) {
                sibling->getParent()->adjustLHV();
                sibling->getParent()->adjustMBR();
            }

            local_siblings.clear();
            for (const auto& sibling : new_siblings) {
                local_siblings.push_back(sibling);
            }

            node = parent;
            new_node = new_parent;
        }
    }
    return root;
}

void RTreeHelper::distributeEntries(
                        std::vector<std::shared_ptr<NodeEntry>>& entries,
                        std::vector<Node*>& siblings) {

}

} // namespace libhdht
