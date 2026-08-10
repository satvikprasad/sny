#pragma once

#include <cstdint>
#include <filesystem>
#include <stack>

enum class NodeKind : uint8_t {
    Doc,
    Para,
    Heading,
    Quote,
    Em,
    Strong,
    Code,
    Text,
    Break,
    Space,
    List,
    OrderedList,
    Item
};

struct Slice {
    uint32_t off, len;

    std::string from_src(const std::string &src) const {
        return std::string(&src[off], len);
    }
};

struct Node {
    uint8_t aux;

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
    std::stack<uint32_t> stk;
    Note note;
};

struct Args {
    std::filesystem::path root_dir, out_dir;
};
