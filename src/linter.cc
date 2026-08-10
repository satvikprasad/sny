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
      if (node.kind != md::NodeKind::IntLink) continue;
	  if (node.text.to_str(n.source).starts_with("http")) continue;

      std::cout << "INFO: (" << i << ") encountered backlink to "
                << node.text.to_str(n.source) << "\n";

      auto p = root_dir / std::filesystem::path(n.rel_path).parent_path() /
               node.text.to_str(n.source);
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
