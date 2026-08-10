#pragma once

#include <filesystem>
#include <map>

#include "util.h"

struct Universe {
  std::vector<md::Note> notes{};
  std::map<std::filesystem::path, uint32_t> path_mapping{};
};

struct Args {
  std::filesystem::path root_dir, out_dir;
};
