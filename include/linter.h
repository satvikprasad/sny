#pragma once

#include "sydney.h"

namespace syd_linter {
void lint(Universe &uv, const std::filesystem::path &root_dir);
}  // namespace syd_linter
