#pragma once

#include <cassert>
#include <functional>
#include <stack>
#include <vector>

namespace flat_tree {
template <typename T>
struct FlatTree {
    std::vector<T> nodes;
    std::vector<uint32_t> end;  // ending indices of subtrees

    inline bool is_child(uint32_t child, uint32_t ancestor) const {
        return child >= ancestor && child < end[ancestor];
    }

    inline void preorder(std::function<void(const T &)> enter,
                         std::function<void(const T &)> exit) const {
        assert(nodes.size() == end.size());

        std::stack<uint32_t> closes{};

        for (uint32_t i = 1; i < nodes.size(); ++i) {
            const T &curr = nodes[i];

            enter(curr);
            closes.push(i);

            while (!closes.empty() && !is_child(i + 1, closes.top())) {
                uint32_t j = closes.top();
                const T &close = nodes[j];

                exit(close);

                closes.pop();
            }
        }
    }
};

template <typename T>
struct Builder {
    FlatTree<T> &tree;
    std::stack<uint32_t> stk;

    inline void enter(const T &&t) {
        stk.push(tree.nodes.size());
        tree.nodes.push_back(t);
        tree.end.push_back(0);
    }

    inline void leave() {
        uint32_t curr_idx = stk.top();
        stk.pop();
        tree.end[curr_idx] = tree.nodes.size();
    }

    Builder<T>(FlatTree<T> &t) : tree{t}, stk{} { stk.push(0); }
};
}  // namespace flat_tree

namespace str {
struct Slice {
    uint32_t off, len;

    std::string from_src(const std::string &src) const {
        return std::string(&src[off], len);
    }
};
}  // namespace str

namespace md {
enum class NodeKind : uint8_t {
    Doc,
    Para,
    Heading,
    Quote,
    Em,
    Strong,
    Code,
    Text,
    Break,
    Space,
    List,
    OrderedList,
    Item,
    Link
};

struct Node {
    uint8_t aux;
    NodeKind kind;

    str::Slice text;
};

struct Note {
    std::string source;
    flat_tree::FlatTree<Node> g;
};
}  // namespace md
