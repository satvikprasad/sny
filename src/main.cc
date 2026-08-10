#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stack>

#include "meta/template.h"
#include "parser.h"
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

void syd_put(const std::map<std::filesystem::path, Note> &notes,
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

        const std::vector<uint32_t> &end = note.doc.end;

        // determines if node i is an ancestor of node j
        const auto is_child = [&](uint32_t j, uint32_t i) {
            return j >= i && j < end[i];
        };

        const auto node_text = [&](const Node &n) {
            return n.kind == NodeKind::Doc ? rel.string()
                                           : n.text.from_src(src);
        };

        std::stack<uint32_t> closes{};
        uint32_t depth = 0;

        for (uint32_t i = 1; i < note.doc.nodes.size(); ++i) {
            const Node &curr = note.doc.nodes[i];

            tmpl_put_header(curr.kind, node_text(curr), curr.aux, depth, buf);
            depth += tmpl_indent_step(curr.kind);

            closes.push(i);
            while (!closes.empty() && !is_child(i + 1, closes.top())) {
                uint32_t j = closes.top();
                const Node &close = note.doc.nodes[j];

                depth -= tmpl_indent_step(close.kind);
                tmpl_put_footer(close.kind, node_text(close), close.aux, depth,
                                buf);

                closes.pop();
            }
        }

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

    std::map<std::filesystem::path, Note> notes = syd_parser::parse(sa);
    std::cout << "INFO: finished parsing, putting to "
              << std::string(sa.out_dir) << "\n";

    syd_put(notes, sa);

    return 0;
}
