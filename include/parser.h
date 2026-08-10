#pragma once

#include <md4c.h>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>

#include "sydney.h"
#include "util.h"

namespace syd_parser {

struct Parser {
  md::Note &note;  // need to initialize note before we initialize the builder
  flat_tree::Builder<md::Node> builder;

  Parser(md::Note &n) : note{n}, builder(note.g) {}

  inline bool parse(const std::stringstream &buffer,
                    const MD_PARSER &md_parser) {
    note.source = buffer.str();
    note.g.nodes.push_back(md::Node{});  // sentinel first node
    note.g.end_idx.push_back(1);

    if (md_parse(note.source.c_str(), note.source.size(), &md_parser, this)) {
      return false;
    }

    return true;
  }
};

void parse(const Args &sa, Universe &uv);
}  // namespace syd_parser
