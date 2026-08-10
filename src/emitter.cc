#include "emitter.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
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

void put_note(const std::filesystem::path &p, const md::Note &note,
              const Args &sa) {
  auto rel = std::filesystem::relative(p, sa.root_dir);
  std::cout << "INFO: putting note " << std::string(rel) << "\n";

  auto base = (sa.out_dir / rel).replace_extension(".html");
  auto parent = base.parent_path();

  if (!std::filesystem::exists(parent)) {
    std::filesystem::create_directories(parent);
  }

  std::ofstream file = std::ofstream(base);
  const std::string &src = note.source;
  std::string buf;
  buf.reserve(64 * 1024);

  const auto node_text = [&](const md::Node &n) -> std::string {
    if (n.kind == md::NodeKind::Doc) {
      return rel.string();
    }

    if (n.kind == md::NodeKind::IntLink || n.kind == md::NodeKind::ExtLink) {
      return resolve_href(n.text.to_str(src));
    }

    return n.text.to_str(src);
  };

  uint32_t depth = 0;
  note.g.preorder(
      [&](const md::Node &curr) {
        meta::tmpl_put_header(curr.kind, node_text(curr), curr.aux, depth, buf);
        depth += meta::tmpl_indent_step(curr.kind);
      },
      [&](const md::Node &close) {
        depth -= meta::tmpl_indent_step(close.kind);
        meta::tmpl_put_footer(close.kind, node_text(close), close.aux, depth,
                              buf);
      });

  file << buf;
}

void put(const Universe &uv, const Args &sa) {
  for (const auto &[p, note_idx] : uv.path_mapping) {
    put_note(p, uv.notes[note_idx], sa);
  }

  put_kgraph(uv, sa);
}
}  // namespace syd_emitter
