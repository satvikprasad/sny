#pragma once

#include <filesystem>

struct Args {
    std::filesystem::path root_dir, out_dir;
};
