#pragma once

#include <filesystem>
#include <map>

#include "sydney.h"

namespace syd_parser {
std::map<std::filesystem::path, Note> parse(const Args &sa);
}
