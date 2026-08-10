#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include "meta/template.h"
#include "parser.h"
#include "sydney.h"

namespace sp = syd_parser;

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
        std::cout << "ERROR: [input_dir] should exist, got " << root_dir
                  << ".\n";
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

// Rewrites a link target into a url relative to the emitted page. The output
// tree mirrors the note tree, so a relative target already points at the right
// place once its extension is swapped; anything with a scheme is left alone.
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
        // wiki-link targets name a note rather than a file
        path += ".html";
    }

    return path + fragment;
}

void syd_put(const std::map<std::filesystem::path, md::Note> &notes,
             const Args &sa) {
    for (auto &[p, note] : notes) {
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

        const std::vector<uint32_t> &end = note.g.end;

        // determines if node i is an ancestor of node j
        const auto is_child = [&](uint32_t j, uint32_t i) {
            return j >= i && j < end[i];
        };

        const auto node_text = [&](const md::Node &n) -> std::string {
            if (n.kind == md::NodeKind::Doc) {
                return rel.string();
            }

            if (n.kind == md::NodeKind::Link) {
                return resolve_href(n.text.from_src(src));
            }

            return n.text.from_src(src);
        };

        uint32_t depth = 0;
        note.g.preorder(
            [&](const md::Node &curr) {
                meta::tmpl_put_header(curr.kind, node_text(curr), curr.aux,
                                      depth, buf);
                depth += meta::tmpl_indent_step(curr.kind);
            },
            [&](const md::Node &close) {
                depth -= meta::tmpl_indent_step(close.kind);
                meta::tmpl_put_footer(close.kind, node_text(close), close.aux,
                                      depth, buf);
            });

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

    std::map<std::filesystem::path, md::Note> notes = sp::parse(sa);
    std::cout << "INFO: finished parsing, putting to "
              << std::string(sa.out_dir) << "\n";

    syd_put(notes, sa);

    return 0;
}
