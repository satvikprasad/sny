#include "parser.h"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "md4c.h"
#include "sydney.h"

namespace syd_parser {
inline Node node_from_detail(MD_BLOCKTYPE type, void *detail) {
    switch (type) {
        case MD_BLOCK_DOC:
            return Node{.kind = NodeKind::Doc};
        case MD_BLOCK_H: {
            MD_BLOCK_H_DETAIL *d = static_cast<MD_BLOCK_H_DETAIL *>(detail);
            return Node{.kind = NodeKind::Heading,
                        .aux = static_cast<uint8_t>(d->level)};
        }
        case MD_BLOCK_QUOTE:
            return Node{.kind = NodeKind::Quote};
        case MD_BLOCK_UL:
            return Node{.kind = NodeKind::List};
        case MD_BLOCK_OL:
            return Node{.kind = NodeKind::OrderedList};
        case MD_BLOCK_LI:
            return Node{.kind = NodeKind::Item};
        case MD_BLOCK_P:
            return Node{.kind = NodeKind::Para};
        default:
            std::cout << "WARNING: Unsupported block type " << type << "\n";
            return Node{.kind = NodeKind::Para};
    }
}

inline Node node_from_detail(MD_SPANTYPE type, void *detail) {
    switch (type) {
        case MD_SPAN_EM:
            return Node{.kind = NodeKind::Em};
        case MD_SPAN_STRONG:
            return Node{.kind = NodeKind::Strong};
        case MD_SPAN_CODE:
            return Node{.kind = NodeKind::Code};
        default:
            std::cout << "WARNING: Unsupported span type " << type << "\n";
            return Node{.kind = NodeKind::Em};
    }
}
int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    NoteParserState *state = static_cast<NoteParserState *>(userdata);
    std::vector<Node> &nodes = state->note.doc.nodes;
    std::vector<uint32_t> &end = state->note.doc.end;

    state->stk.push(nodes.size());

    nodes.push_back(node_from_detail(type, detail));
    end.push_back(0);

    return 0;
}

int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    NoteParserState *state = static_cast<NoteParserState *>(userdata);

    uint32_t curr_idx = state->stk.top();
    state->stk.pop();

    state->note.doc.end[curr_idx] = state->note.doc.nodes.size();

    return 0;
}

int enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    NoteParserState *state = static_cast<NoteParserState *>(userdata);
    std::vector<Node> &nodes = state->note.doc.nodes;
    std::vector<uint32_t> &end = state->note.doc.end;

    state->stk.push(nodes.size());

    nodes.push_back(node_from_detail(type, detail));
    end.push_back(0);

    return 0;
}

int leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    NoteParserState *state = static_cast<NoteParserState *>(userdata);

    uint32_t curr_idx = state->stk.top();
    state->stk.pop();

    state->note.doc.end[curr_idx] = state->note.doc.nodes.size();

    return 0;
}

int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    NoteParserState *state = static_cast<NoteParserState *>(userdata);
    std::vector<Node> &nodes = state->note.doc.nodes;
    std::vector<uint32_t> &end = state->note.doc.end;

    if (type == MD_TEXT_NULLCHAR) {
        return 0;
    }

    // md4c emits normalised whitespace, line breaks and the null replacement
    // from its own string literals rather than from the source buffer, so the
    // pointer only yields a valid offset when it lies inside that buffer.
    const char *base = state->note.source.data();
    const bool in_source =
        text >= base && text + size <= base + state->note.source.size();

    Node node{};

    if (type == MD_TEXT_BR) {
        node.kind = NodeKind::Break;
    } else if (type == MD_TEXT_SOFTBR || !in_source) {
        node.kind = NodeKind::Space;
    } else {
        node.kind = NodeKind::Text;
        node.text = Slice{
            .off = static_cast<uint32_t>(text - base),
            .len = size,
        };
    }

    nodes.push_back(node);
    end.push_back(nodes.size());

    return 0;
}

std::map<std::filesystem::path, Note> parse(const Args &sa) {
    std::map<std::filesystem::path, Note> notes{};

    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(sa.root_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::cout << "INFO: reading file " << entry << ".\n";
        std::filesystem::path p = entry.path();

        std::ifstream file(p);
        if (!file.is_open()) {
            std::cout << "ERROR: could not open file " << entry.path() << ".\n";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        NoteParserState state{
            .stk = std::stack<uint32_t>(),
            .note = {}};  // state to track construction of AST

        state.stk.push(0);
        Note &note = state.note;

        note.source = buffer.str();
        note.doc.nodes = std::vector<Node>(1);  // sentinel first node
        note.doc.end = std::vector<uint32_t>(1);

        struct MD_PARSER parser = {.enter_block = &enter_block,
                                   .leave_block = &leave_block,
                                   .enter_span = &enter_span,
                                   .leave_span = &leave_span,
                                   .text = &text};

        if (md_parse(note.source.c_str(), note.source.size(), &parser,
                     &state)) {
            std::cout << "ERROR: failed to parse " << p << "\n";
        }

        notes[p] = note;
    }

    return notes;
}
}  // namespace parser
