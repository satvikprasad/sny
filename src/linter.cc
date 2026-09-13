#include "linter.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#include "sydney.h"
#include "util.h"

namespace syd_linter {
void lint(Universe &uv, const std::filesystem::path &root_dir) {
  std::cout << "INFO: linting universe...\n";

  std::vector<sparse_graph::Edge<uint32_t>> edges{};

  for (uint32_t i = 0; i < uv.notes.size(); ++i) {
    const auto &n = uv.notes[i];
    for (const auto &node : n.g) {
      const bool is_link = node.kind == md::NodeKind::IntLink;
      const bool is_excerpt = node.kind == md::NodeKind::Excerpt;

      if (!is_link && !is_excerpt) continue;

      const std::string body = node.text.to_str(n.source);

      if (body.starts_with("http")) continue;

      const std::string target = is_excerpt ? md::excerpt_path(body) : body;

      std::cout << "INFO: (" << i << ") encountered "
                << (is_excerpt ? "excerpt" : "backlink") << " to " << target
                << "\n";

      auto p = (root_dir / std::filesystem::path(n.rel_path).parent_path() /
                target)
                   .lexically_normal();
      auto it = uv.path_mapping.find(p);

      if (it == uv.path_mapping.end()) {
        std::cout << "ERROR: could not resolve backlink to " << std::string(p)
                  << "\n";
        continue;
      }

      edges.push_back(sparse_graph::Edge<uint32_t>(i, it->second));
    }
  }

  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

  sparse_graph::SparseGraph<md::Note> sg(uv.notes, edges);

  uv.kgraph = std::move(sg);
}
}  // namespace syd_linter
