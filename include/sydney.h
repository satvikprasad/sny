#pragma once

#include <filesystem>
#include <map>

#include "util.h"

struct Universe {
  std::vector<md::Note> notes{};
  std::map<std::filesystem::path, uint32_t> path_mapping{};

  sparse_graph::SparseGraph<md::Note> kgraph;
};

struct Args {
  std::filesystem::path root_dir, out_dir;
};
