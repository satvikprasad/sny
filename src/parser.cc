#include "parser.h"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "md4c.h"
#include "sydney.h"

namespace syd_parser {
// md4c emits normalised whitespace, line breaks and attribute substitutions
// from its own string literals rather than from the source buffer, so a
// pointer only yields a valid offset when it lies inside that buffer.
inline bool in_source(const std::string &src, const MD_CHAR *p, MD_SIZE n) {
    const char *base = src.data();
    return p >= base && p + n <= base + src.size();
}

inline str::Slice slice_of(const std::string &src, const MD_CHAR *p,
                           MD_SIZE n) {
    return str::Slice{.off = static_cast<uint32_t>(p - src.data()), .len = n};
}

inline md::Node node_from_detail(MD_BLOCKTYPE type, void *detail) {
    switch (type) {
        case MD_BLOCK_DOC:
            return md::Node{.kind = md::NodeKind::Doc};
        case MD_BLOCK_H: {
            MD_BLOCK_H_DETAIL *d = static_cast<MD_BLOCK_H_DETAIL *>(detail);
            return md::Node{.kind = md::NodeKind::Heading,
                            .aux = static_cast<uint8_t>(d->level)};
        }
        case MD_BLOCK_QUOTE:
            return md::Node{.kind = md::NodeKind::Quote};
        case MD_BLOCK_UL:
            return md::Node{.kind = md::NodeKind::List};
        case MD_BLOCK_OL:
            return md::Node{.kind = md::NodeKind::OrderedList};
        case MD_BLOCK_LI:
            return md::Node{.kind = md::NodeKind::Item};
        case MD_BLOCK_P:
            return md::Node{.kind = md::NodeKind::Para};
        default:
            std::cout << "WARNING: Unsupported block type " << type << "\n";
            return md::Node{.kind = md::NodeKind::Para};
    }
}

inline md::Node node_from_detail(MD_SPANTYPE type, void *detail,
                                 const std::string &src) {
    switch (type) {
        case MD_SPAN_EM:
            return md::Node{.kind = md::NodeKind::Em};
        case MD_SPAN_STRONG:
            return md::Node{.kind = md::NodeKind::Strong};
        case MD_SPAN_CODE:
            return md::Node{.kind = md::NodeKind::Code};
        case MD_SPAN_WIKILINK: {
            MD_SPAN_WIKILINK_DETAIL *d =
                static_cast<MD_SPAN_WIKILINK_DETAIL *>(detail);

            md::Node node{.kind = md::NodeKind::Link};

            if (in_source(src, d->target.text, d->target.size)) {
                node.text = slice_of(src, d->target.text, d->target.size);
            }

            return node;
        }
        default:
            std::cout << "WARNING: Unsupported span type " << type << "\n";
            return md::Node{.kind = md::NodeKind::Em};
    }
}

inline MD_PARSER get_parser() {
    auto text = [](MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,
                   void *userdata) {
        Parser *state = static_cast<Parser *>(userdata);
        std::vector<md::Node> &nodes = state->note.g.nodes;
        std::vector<uint32_t> &end = state->note.g.end;

        if (type == MD_TEXT_NULLCHAR) {
            return 0;
        }

        const std::string &src = state->note.source;

        md::Node node{};

        if (type == MD_TEXT_BR) {
            node.kind = md::NodeKind::Break;
        } else if (type == MD_TEXT_SOFTBR || !in_source(src, text, size)) {
            node.kind = md::NodeKind::Space;
        } else {
            node.kind = md::NodeKind::Text;
            node.text = slice_of(src, text, size);
        }

        nodes.push_back(node);
        end.push_back(nodes.size());

        return 0;
    };

    return MD_PARSER{
        .flags = MD_FLAG_WIKILINKS,
        .enter_block =
            [](MD_BLOCKTYPE type, void *detail, void *userdata) {
                Parser *state =
                    static_cast<Parser *>(userdata);
                state->builder.enter(node_from_detail(type, detail));
                return 0;
            },
        .leave_block =
            [](MD_BLOCKTYPE type, void *detail, void *userdata) {
                Parser *state =
                    static_cast<Parser *>(userdata);
                state->builder.leave();
                return 0;
            },
        .enter_span =
            [](MD_SPANTYPE type, void *detail, void *userdata) {
                Parser *state =
                    static_cast<Parser *>(userdata);
                state->builder.enter(
                    node_from_detail(type, detail, state->note.source));
                return 0;
            },
        .leave_span =
            [](MD_SPANTYPE type, void *detail, void *userdata) {
                Parser *state =
                    static_cast<Parser *>(userdata);
                state->builder.leave();
                return 0;
            },
        .text = text};
}

std::map<std::filesystem::path, md::Note> parse(const Args &sa) {
    std::map<std::filesystem::path, md::Note> notes{};

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

        Parser state{};  // state to track construction of AST

        md::Note &note = state.note;

        note.source = buffer.str();
        note.g.nodes = std::vector<md::Node>(1);  // sentinel first node
        note.g.end = std::vector<uint32_t>(1);

        MD_PARSER parser = get_parser();
        if (md_parse(note.source.c_str(), note.source.size(), &parser,
                     &state)) {
            std::cout << "ERROR: failed to parse " << p << "\n";
        }

        notes[p] = note;
    }

    return notes;
}
}  // namespace syd_parser
