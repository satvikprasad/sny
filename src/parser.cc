#include "parser.h"

#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

#include "md4c.h"
#include "sydney.h"
#include "util.h"

namespace syd_parser {
// md4c emits normalised whitespace, line breaks and attribute substitutions
// from its own string literals rather than from the source buffer, so a
// pointer only yields a valid offset when it lies inside that buffer.
inline bool in_source(const std::string &src, const MD_CHAR *p, MD_SIZE n) {
  const char *base = src.data();
  return p >= base && p + n <= base + src.size();
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
    case MD_BLOCK_OL: {
      MD_BLOCK_OL_DETAIL *d = static_cast<MD_BLOCK_OL_DETAIL *>(detail);
      return md::Node{.kind = md::NodeKind::OrderedList,
                      .aux = static_cast<uint8_t>(d->start)};
    }
    case MD_BLOCK_LI:
      return md::Node{.kind = md::NodeKind::Item};
    case MD_BLOCK_P:
      return md::Node{.kind = md::NodeKind::Para};
    case MD_BLOCK_BLOCK:
      return md::Node{.kind = md::NodeKind::Block};
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

      md::Node node{.kind = md::NodeKind::IntLink};

      if (in_source(src, d->target.text, d->target.size)) {
        node.text = str::Slice(src, d->target.text, d->target.size);
      }

      return node;
    }
    case MD_SPAN_LATEXMATH:
      return md::Node{.kind = md::NodeKind::Math};
    case MD_SPAN_LATEXMATH_DISPLAY:
      return md::Node{.kind = md::NodeKind::MathBlock};
    case MD_SPAN_EXCERPT: {
      MD_SPAN_EXCERPT_DETAIL *d = static_cast<MD_SPAN_EXCERPT_DETAIL *>(detail);

      md::Node node{.kind = md::NodeKind::Excerpt};

      const MD_CHAR *beg = d->path.text;
      const MD_CHAR *end = d->tag.size > 0 ? d->tag.text + d->tag.size + 1
                                           : d->path.text + d->path.size;

      if (in_source(src, beg, end - beg)) {
        node.text = str::Slice(src, beg, end - beg);
      }

      return node;
    }
    case MD_SPAN_A: {
      MD_SPAN_A_DETAIL *d = static_cast<MD_SPAN_A_DETAIL *>(detail);

      md::Node node{.kind = md::NodeKind::ExtLink};

      if (in_source(src, d->href.text, d->href.size)) {
        node.text = str::Slice(src, d->href.text, d->href.size);
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

    if (type == MD_TEXT_NULLCHAR) {
      return 0;
    }

    std::string t(text, size);

    const std::string &src = state->note.source;

    md::Node node{};

    if (type == MD_TEXT_BR) {
      node.kind = md::NodeKind::Break;
    } else if (type == MD_TEXT_SOFTBR || !in_source(src, text, size)) {
      node.kind = md::NodeKind::Space;
    } else {
      node.kind = md::NodeKind::Text;
      node.text = str::Slice(src, text, size);
    }

    state->builder.enter(std::move(node));
    state->builder.leave();

    return 0;
  };

  return MD_PARSER{
      .flags = MD_FLAG_WIKILINKS | MD_FLAG_BLOCKS | MD_FLAG_EXCERPTS |
               MD_FLAG_LATEXMATHSPANS,
      .enter_block =
          [](MD_BLOCKTYPE type, void *detail, void *userdata) {
            Parser *parser = static_cast<Parser *>(userdata);
            uint32_t pushed =
                parser->builder.enter(node_from_detail(type, detail)) - 1;

            if (type == MD_BLOCK_BLOCK) {
              MD_BLOCK_BLOCK_DETAIL *d =
                  static_cast<MD_BLOCK_BLOCK_DETAIL *>(detail);

              if (d->name.size > 0) {
                parser->note.tags[std::string(d->name.text, d->name.size)] =
                    pushed;
              }
            }

            return 0;
          },
      .leave_block =
          [](MD_BLOCKTYPE type, void *detail, void *userdata) {
            Parser *parser = static_cast<Parser *>(userdata);
            parser->builder.leave();

            return 0;
          },
      .enter_span =
          [](MD_SPANTYPE type, void *detail, void *userdata) {
            Parser *parser = static_cast<Parser *>(userdata);
            parser->builder.enter(
                node_from_detail(type, detail, parser->note.source));
            return 0;
          },
      .leave_span =
          [](MD_SPANTYPE type, void *detail, void *userdata) {
            Parser *parser = static_cast<Parser *>(userdata);
            parser->builder.leave();
            return 0;
          },
      .text = text};
}

void parse(const Args &sa, Universe &uv) {
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

    uv.path_mapping[p] = uv.notes.size();
    uv.notes.push_back(md::Note{.rel_path = p.lexically_relative(sa.root_dir)});

    Parser parser(uv.notes.back());  // state to track construction of AST
    if (!parser.parse(buffer, get_parser())) {
      std::cout << "ERROR: failed to parse " << p << "\n";
    }
  }
}
}  // namespace syd_parser
