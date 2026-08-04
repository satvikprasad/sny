#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stack>

#include "md4c.h"
#include "sydney.h"

std::optional<Args> parse_args(char argc, char **argv) {
  auto disp_help = [&]() {
    std::cout << "Usage: sydney [input_dir] -o [output_dir (default: "
                 "./static/)]\n";
  };

  if (argc <= 1) {
    disp_help();
    return std::nullopt;
  }

  char *root_dir = argv[1];

  Args sa;
  sa.root_dir = std::filesystem::path(root_dir);

  if (!std::filesystem::exists(sa.root_dir)) {
    std::cout << "ERROR: [input_dir] should exist, got " << root_dir << ".\n";
    return std::nullopt;
  }

  if (argc != 4) {
    sa.out_dir = std::filesystem::current_path() / "static/";
  } else {
    char *out_dir = argv[3];
    sa.out_dir = std::filesystem::path(out_dir);
  }

  if (!std::filesystem::exists(sa.out_dir)) {
    // create directory of it does not exist
    std::filesystem::create_directory(sa.out_dir);
  }

  return sa;
}

Node node_from_detail(MD_BLOCKTYPE type, void *detail) {
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
    case MD_BLOCK_P:
      return Node{.kind = NodeKind::Para};
    default:
      std::cout << "WARNING: Unsupported block type " << type << "\n";
      return Node{.kind = NodeKind::Para};
  }
}

void syd_append_header(const Node &node, std::string &buf,
                       const std::string &src) {
  auto emit_text = [&]() { buf.append(&src[node.text.off], node.text.len); };

  switch (node.kind) {
    case NodeKind::Doc:
      buf.append("<body>\n");
      return;
    case NodeKind::Para:
      buf.append("<p>");
      emit_text();
      return;
    case NodeKind::Heading:
      buf.append(std::format("<h{}>\n", node.aux));
      emit_text();
      return;
    default:
      std::cout << "WARNING: Unsupported NodeKind "
                << static_cast<int>(node.kind) << "\n";
      buf.append("<p>\n");
      return;
  }
}

void syd_append_footer(const Node &node, std::string &buf) {
  switch (node.kind) {
    case NodeKind::Doc:
      buf.append("</body>\n");
      return;
    case NodeKind::Para:
      buf.append("</p>\n");
      return;
    case NodeKind::Heading:
      buf.append(std::format("</h{}>\n", node.aux));
      return;
    default:
      std::cout << "WARNING: Unsupported NodeKind "
                << static_cast<int>(node.kind) << "\n";
      buf.append("</p>\n");
      return;
  }
}

int syd_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
  NoteParserState *state = static_cast<NoteParserState *>(userdata);
  std::vector<Node> &nodes = state->note.doc.nodes;
  std::vector<uint32_t> &end = state->note.doc.end;

  state->note.doc.end[state->prev] = nodes.size();
  state->stk.push(nodes.size());

  nodes.push_back(node_from_detail(type, detail));
  end.push_back(0);

  state->prev = 0;

  return 0;
}

int syd_leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
  NoteParserState *state = static_cast<NoteParserState *>(userdata);

  state->prev = state->stk.top();
  state->stk.pop();

  return 0;
}

int syd_enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
  std::cout << "entered block\n";
  return 0;
}

int syd_leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
  std::cout << "entered block\n";
  return 0;
}

int syd_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size,
             void *userdata) {
  NoteParserState *state = static_cast<NoteParserState *>(userdata);
  uint32_t curr_idx = state->stk.top();

  Doc &d = state->note.doc;
  d.nodes[curr_idx].text = Slice{
      .off = static_cast<uint32_t>(text - state->note.source.data()),
      .len = size,
  };

  return 0;
}

std::map<std::filesystem::path, Note> syd_parse(const Args &sa) {
  std::map<std::filesystem::path, Note> notes{};

  for (const auto &entry : std::filesystem::directory_iterator(sa.root_dir)) {
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

    NoteParserState state{.stk = std::stack<uint32_t>(),
                          .note = {}};  // state to track construction of AST

    state.stk.push(0);
    Note &note = state.note;

    note.source = buffer.str();
    note.doc.nodes = std::vector<Node>(1);  // sentinel first node
    note.doc.end = std::vector<uint32_t>(1);

    struct MD_PARSER parser = {.enter_block = &syd_enter_block,
                               .leave_block = &syd_leave_block,
                               .enter_span = &syd_enter_span,
                               .leave_span = &syd_leave_span,
                               .text = &syd_text};

    if (md_parse(note.source.c_str(), note.source.size(), &parser, &state)) {
      std::cout << "ERROR: failed to parse " << p << "\n";
    }

    // replace pointers to sentinel with correct value
    for (uint32_t i = 1; i < note.doc.end.size(); ++i) {
      if (note.doc.end[i] == 0) note.doc.end[i] = note.doc.nodes.size();
    }

    notes[p] = note;
  }

  return notes;
}

void syd_put(const std::map<std::filesystem::path, Note> &notes,
             const Args &sa) {
  for (auto &[p, note] : notes) {
    auto rel = std::filesystem::relative(p, sa.root_dir);
    auto base = (sa.out_dir / rel).replace_extension(".html");

    std::ofstream file = std::ofstream(base);
    const std::string &src = note.source;
    std::string buf;
    buf.reserve(64 * 1024);

    const std::vector<uint32_t> &end = note.doc.end;

    // determines if node i is an ancestor of node j
    const auto is_child = [&](uint32_t j, uint32_t i) {
      return j >= i && j < end[i];
    };

    buf.append(
        std::format("<html><head><title>{}</title></head>", std::string(rel)));

    std::stack<uint32_t> closes{};
    for (uint32_t i = 1; i < note.doc.nodes.size(); ++i) {
      syd_append_header(note.doc.nodes[i], buf, src);
      closes.push(i);

      while (!closes.empty() && !is_child(i + 1, closes.top())) {
        uint32_t j = closes.top();
        closes.pop();

        syd_append_footer(note.doc.nodes[j], buf);
      }
    }

    buf.append("</html>");

    file << buf;
  }
}

int main(int argc, char **argv) {
  std::optional<Args> s = parse_args(argc, argv);
  if (s == std::nullopt) {
    return -1;
  }

  Args sa = s.value();
  std::cout << "INFO: using " << sa.root_dir << " as root directory.\n";

  std::map<std::filesystem::path, Note> notes = syd_parse(sa);
  std::cout << "INFO: finished parsing, putting to " << std::string(sa.out_dir)
            << "\n";

  syd_put(notes, sa);

  return 0;
}
