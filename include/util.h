#pragma once

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace sparse_graph {
template <typename T>
struct Edge {
  T u, v;

  Edge<T>(T a, T b) : u(a), v(b) {}

  auto operator<=>(const Edge<T> &) const = default;
};

inline void build_csr(size_t n, const std::vector<Edge<uint32_t>> &edges,
                      bool reverse, std::vector<uint32_t> &offsets,
                      std::vector<uint32_t> &adjacency) {
  offsets.assign(n + 1, 0);

  for (auto [u, v] : edges) {
    assert(u < n && v < n);
    offsets[(reverse ? v : u) + 1]++;
  }

  for (size_t i = 0; i < n; ++i) offsets[i + 1] += offsets[i];

  adjacency.resize(offsets[n]);

  std::vector<uint32_t> cursor(offsets.begin(), offsets.end() - 1);

  for (auto [u, v] : edges) {
    adjacency[cursor[reverse ? v : u]++] = reverse ? u : v;
  }
}

template <typename T>
struct SparseGraph {
  std::vector<uint32_t> out_offsets, out_adjacency;
  std::vector<uint32_t> in_offsets, in_adjacency;

  SparseGraph<T>(const std::vector<T> &backing,
                 const std::vector<Edge<uint32_t>> &edges) {
    build_csr(backing.size(), edges, false, out_offsets, out_adjacency);
    build_csr(backing.size(), edges, true, in_offsets, in_adjacency);
  }

  SparseGraph<T>() {}
};
};  // namespace sparse_graph

namespace flat_tree {
template <typename T>
struct FlatTree {
  std::vector<T> nodes;
  std::vector<uint32_t> end_idx;  // ending indices of subtrees

  FlatTree<T>() : nodes{}, end_idx{} {};

  inline bool is_child(uint32_t child, uint32_t ancestor) const {
    return child >= ancestor && child < end_idx[ancestor];
  }

  inline void preorder(std::function<void(const T &)> enter,
                       std::function<void(const T &)> exit) const {
    assert(nodes.size() == end_idx.size());

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

  T &operator[](uint32_t index) { return nodes[index]; }

  auto begin() { return nodes.begin(); }
  auto end() { return nodes.end(); }

  auto begin() const { return nodes.begin(); }
  auto end() const { return nodes.end(); }
};

template <typename T>
struct Builder {
  FlatTree<T> &tree;
  std::stack<uint32_t> stk;

  T &top() { return tree[stk.top()]; }

  uint32_t top_idx() const { return stk.top(); }

  inline size_t size() const { return tree.nodes.size(); }

  inline void truncate(size_t n) {
    tree.nodes.resize(n);
    tree.end_idx.resize(n);
  }

  inline void abandon() {
    uint32_t curr_idx = stk.top();
    stk.pop();

    truncate(curr_idx);
  }

  inline size_t enter(const T &&t) {
    stk.push(tree.nodes.size());
    tree.nodes.push_back(t);
    tree.end_idx.push_back(0);

    return tree.nodes.size();
  }

  inline size_t leave() {
    uint32_t curr_idx = stk.top();
    stk.pop();
    tree.end_idx[curr_idx] = tree.nodes.size();

    return curr_idx;
  }

  Builder<T>(FlatTree<T> &t) : tree{t}, stk{} { stk.push(0); }
};
}  // namespace flat_tree

namespace str {
struct Slice {
  uint32_t off, len;

  Slice(const std::string &src, const char *p, size_t n)
      : off(static_cast<uint32_t>(p - src.data())), len(n) {}

  Slice() : off{}, len{} {}

  std::string to_str(const std::string &src) const {
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
  IntLink,
  ExtLink,
  Block,
  Excerpt,
};

struct Node {
  uint8_t aux;
  NodeKind kind;

  str::Slice text;
};

struct Note {
  std::string source, rel_path;

  flat_tree::FlatTree<Node> g;

  std::map<std::string, uint32_t> tags; // maps tag ->  node idx
};
}  // namespace md
