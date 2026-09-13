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

  // md4c emits a code block's newlines and indentation from its own static
  // buffers, so the block is recovered as one span of the source instead: the
  // gaps between the in-source runs are exactly that whitespace.
  bool in_code = false;
  uint32_t code_beg = 0, code_end = 0;

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
