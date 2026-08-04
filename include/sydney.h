#pragma once

#include <cstdint>
#include <filesystem>
#include <stack>

enum class NodeKind : uint8_t { Doc, Para, Heading, Quote };

struct Slice {
  uint32_t off, len;
};

struct Node {
  NodeKind kind;
  Slice text;
};

struct Doc {
  std::vector<Node> nodes;
  std::vector<uint32_t> end;
};

struct Note {
  std::string source;
  Doc doc;
};

struct NoteParserState {
  uint32_t prev;
  std::stack<uint32_t> stk;
  Note note;
};

struct Args {
  std::filesystem::path root_dir, out_dir;
};
