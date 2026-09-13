#include "emitter.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <stack>
#include <vector>

#include "meta/template.h"
#include "sydney.h"
#include "util.h"

namespace syd_emitter {
std::string resolve_href(const std::string &href) {
  if (href.empty() || href.starts_with("#") || href.starts_with("//") ||
      href.starts_with("mailto:") || href.find("://") != std::string::npos) {
    return href;
  }

  std::string path = href;
  std::string fragment;

  if (size_t hash = path.find('#'); hash != std::string::npos) {
    fragment = path.substr(hash);
    path.resize(hash);
  }

  if (path.ends_with(".md")) {
    path.resize(path.size() - 3);
    path += ".html";
  } else if (!std::filesystem::path(path).has_extension()) {
    path += ".html";
  }

  return path + fragment;
}

std::string html_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());

  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += c;
    }
  }

  return out;
}

std::string json_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());

  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out += std::format("\\u{:04x}", static_cast<unsigned>(c));
        } else {
          out += c;
        }
    }
  }

  return out;
}

void put_kgraph(const Universe &uv, const Args &sa) {
  std::vector<std::filesystem::path> by_id(uv.notes.size());
  for (const auto &[p, idx] : uv.path_mapping) by_id[idx] = p;

  std::filesystem::path out = sa.out_dir / "kgraph.json";
  std::cout << "INFO: putting knowledge graph " << std::string(out) << "\n";

  std::ofstream file(out);

  file << "{\n  \"nodes\": [\n";

  for (uint32_t i = 0; i < by_id.size(); ++i) {
    auto rel = std::filesystem::relative(by_id[i], sa.root_dir);
    auto url = std::filesystem::path(rel).replace_extension(".html");

    file << "    {\"id\": " << i << ", \"path\": \""
         << json_escape(rel.generic_string()) << "\", \"url\": \""
         << json_escape(url.generic_string()) << "\"}"
         << (i + 1 < by_id.size() ? "," : "") << "\n";
  }

  file << "  ],\n  \"links\": [\n";

  const auto &offsets = uv.kgraph.out_offsets;
  const auto &adjacency = uv.kgraph.out_adjacency;

  bool first = true;
  for (uint32_t u = 0; u + 1 < offsets.size(); ++u) {
    for (uint32_t k = offsets[u]; k < offsets[u + 1]; ++k) {
      if (!first) file << ",\n";

      file << "    [" << u << ", " << adjacency[k] << "]";
      first = false;
    }
  }

  file << (first ? "" : "\n") << "  ]\n}\n";
}

struct Excerpt {
  std::filesystem::path path;
  std::string tag;
};

Excerpt parse_excerpt(const std::string &body) {
  Excerpt ex;

  ex.path = md::excerpt_path(body);

  size_t at = body.find('@');

  if (at != std::string::npos) {
    size_t open = body.find('"', at);
    size_t close = open == std::string::npos
                       ? std::string::npos
                       : body.find('"', open + 1);

    if (close != std::string::npos) {
      ex.tag = body.substr(open + 1, close - open - 1);
    }
  }

  return ex;
}

void put_note(const std::filesystem::path &p, const Universe &uv,
              const md::Note &note, const Args &sa) {
  auto rel = std::filesystem::relative(p, sa.root_dir);
  std::cout << "INFO: putting note " << std::string(rel) << "\n";

  auto base = (sa.out_dir / rel).replace_extension(".html");
  auto parent = base.parent_path();

  if (!std::filesystem::exists(parent)) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream file = std::ofstream(base);
  std::string buf;
  buf.reserve(64 * 1024);

  uint32_t depth = 0;

  const auto node_text = [&](const md::Note &n, const md::Node &node) {
    if (node.kind == md::NodeKind::Doc) {
      return rel.string();
    }

    if (node.kind == md::NodeKind::IntLink ||
        node.kind == md::NodeKind::ExtLink ||
        node.kind == md::NodeKind::Image ||
        node.kind == md::NodeKind::Video) {
      return resolve_href(node.text.to_str(n.source));
    }

    if (node.kind == md::NodeKind::Excerpt) {
      return resolve_href(parse_excerpt(node.text.to_str(n.source)).path.string());
    }

    if (node.kind == md::NodeKind::CodeBlock) {
      const std::string lang = node.text.to_str(n.source);
      return html_escape(lang.empty() ? "plaintext" : lang);
    }

    return html_escape(node.text.to_str(n.source));
  };

  std::stack<uint32_t> closes{};

  const auto drain = [&](const md::Note &n, uint32_t i) {
    while (!closes.empty() && !n.g.is_child(i + 1, closes.top())) {
      const md::Node &close = n.g.nodes[closes.top()];

      depth -= meta::tmpl_indent_step(close.kind);
      meta::tmpl_put_footer(close.kind, node_text(n, close), close.aux, depth,
                            buf);

      closes.pop();
    }
  };

  const auto emit = [&](const md::Note &n, uint32_t i) {
    const md::Node &curr = n.g.nodes[i];

    meta::tmpl_put_header(curr.kind, node_text(n, curr), curr.aux, depth, buf);
    depth += meta::tmpl_indent_step(curr.kind);

    closes.push(i);
    drain(n, i);
  };

  // Embedding is capped at depth 1, so the target range is walked by a plain
  // inner loop that skips any excerpts it finds rather than recursing.
  const auto embed = [&](const md::Note &n, uint32_t begin, uint32_t end) {
    std::stack<uint32_t> outer;
    closes.swap(outer);

    for (uint32_t i = begin; i < end; ++i) {
      const md::Node &curr = n.g.nodes[i];

      if (curr.kind == md::NodeKind::Excerpt) {
        const std::string body = curr.text.to_str(n.source);
        const md::NodeKind kind = md::NodeKind::ExcerptNested;

        meta::tmpl_put_header(kind, body, curr.aux, depth, buf);
        meta::tmpl_put_footer(kind, body, curr.aux, depth, buf);

        drain(n, i);
        continue;
      }

      emit(n, i);
    }

    closes.swap(outer);
  };

  for (uint32_t i = 1; i < note.g.nodes.size(); ++i) {
    const md::Node &curr = note.g.nodes[i];

    if (curr.kind != md::NodeKind::Excerpt) {
      emit(note, i);
      continue;
    }

    const Excerpt ex = parse_excerpt(curr.text.to_str(note.source));
    const auto target = uv.path_mapping.find(
        (sa.root_dir / rel.parent_path() / ex.path).lexically_normal());

    meta::tmpl_put_header(curr.kind, node_text(note, curr), curr.aux, depth,
                          buf);
    depth += meta::tmpl_indent_step(curr.kind);

    if (target == uv.path_mapping.end()) {
      std::cout << "ERROR: excerpt could not resolve " << std::string(ex.path)
                << "\n";
    } else {
      const md::Note &from = uv.notes[target->second];

      if (ex.tag.empty()) {
        embed(from, 2, from.g.end_idx[1]);
      } else if (const auto at = from.tags.find(ex.tag); at == from.tags.end()) {
        std::cout << "ERROR: excerpt could not resolve tag " << ex.tag << " in "
                  << std::string(ex.path) << "\n";
      } else {
        embed(from, at->second, from.g.end_idx[at->second]);
      }
    }

    depth -= meta::tmpl_indent_step(curr.kind);
    meta::tmpl_put_footer(curr.kind, node_text(note, curr), curr.aux, depth,
                          buf);

    drain(note, i);
  }


  file << buf;
}

void put_assets(const Args &sa) {
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(sa.root_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const std::filesystem::path &p = entry.path();

    if (p.extension() == ".md") {
      continue;
    }

    auto rel = std::filesystem::relative(p, sa.root_dir);
    auto out = sa.out_dir / rel;

    std::cout << "INFO: copying asset " << std::string(rel) << "\n";

    std::filesystem::create_directories(out.parent_path());
    std::filesystem::copy_file(p, out,
                               std::filesystem::copy_options::update_existing);
  }
}

void put(const Universe &uv, const Args &sa) {
  for (const auto &[p, note_idx] : uv.path_mapping) {
    put_note(p, uv, uv.notes[note_idx], sa);
  }

  put_assets(sa);
  put_kgraph(uv, sa);
}
}  // namespace syd_emitter
