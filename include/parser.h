#pragma once

#include <cassert>
#include <filesystem>
#include <map>

#include "sydney.h"
#include "util.h"

namespace syd_parser {

struct Parser {
    md::Note note;  // need to initialize note before we initialize the builder
    flat_tree::Builder<md::Node> builder;

    Parser() : note{}, builder(note.g) {}
};

std::map<std::filesystem::path, md::Note> parse(const Args &sa);
}  // namespace syd_parser
