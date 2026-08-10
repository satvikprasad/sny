#include "linter.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <vector>

#include "sydney.h"
#include "util.h"

namespace syd_linter {
void lint(Universe &uv, const std::filesystem::path &root_dir) {
  std::set<std::pair<uint32_t, uint32_t>> edges{};

  for (int i = 0; i < uv.notes.size(); ++i) {
    const auto &n = uv.notes[i];
    for (const auto &node : n.g) {
      if (node.kind != md::NodeKind::Link) continue;

      std::cout << "INFO: (" << i << ") encountered backlink to "
                << node.text.to_str(n.source) << "\n";

      auto p = root_dir / node.text.to_str(n.source);
      if (!std::filesystem::exists(p)) {
        std::cout << "ERROR: could not resolve backlink to " << std::string(p)
                  << "\n";
      }

      uint32_t j = uv.path_mapping[p];

      edges.insert({i, j});
    }
  }
}
}  // namespace syd_linter
