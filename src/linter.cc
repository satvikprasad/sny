#include "linter.h"

#include <filesystem>
#include <iostream>
#include <vector>

#include "sydney.h"
#include "util.h"

namespace syd_linter {
void lint(Universe &uv, const std::filesystem::path &root_dir) {
  std::vector<sparse_graph::Edge<uint32_t>> edges{};

  // build an edge list
  for (uint32_t i = 0; i < uv.notes.size(); ++i) {
    const auto &n = uv.notes[i];
    for (const auto &node : n.g) {
      if (node.kind != md::NodeKind::Link) continue;

      std::cout << "INFO: (" << i << ") encountered backlink to "
                << node.text.to_str(n.source) << "\n";

      auto p = root_dir / node.text.to_str(n.source);
      auto it = uv.path_mapping.find(p);

      if (it == uv.path_mapping.end()) {
        std::cout << "ERROR: could not resolve backlink to " << std::string(p)
                  << "\n";
        continue;
      }

      edges.push_back(sparse_graph::Edge<uint32_t>(i, it->second));
    }
  }

  sparse_graph::SparseGraph<md::Note> sg(uv.notes, edges);
}
}  // namespace syd_linter
