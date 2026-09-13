#include <filesystem>
#include <iostream>
#include <optional>

#include "emitter.h"
#include "linter.h"
#include "parser.h"
#include "sydney.h"

namespace sp = syd_parser;
namespace sl = syd_linter;
namespace se = syd_emitter;

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
  sa.root_dir = std::filesystem::path(root_dir).lexically_normal();

  if (!std::filesystem::exists(sa.root_dir)) {
    std::cout << "ERROR: [input_dir] should exist, got " << root_dir << ".\n";
    return std::nullopt;
  }

  if (argc != 4) {
    sa.out_dir = std::filesystem::current_path() / "static/";
  } else {
    char *out_dir = argv[3];
    sa.out_dir = std::filesystem::path(out_dir).lexically_normal();
  }

  if (!std::filesystem::exists(sa.out_dir)) {
    std::filesystem::create_directory(sa.out_dir);
  }

  return sa;
}

int main(int argc, char **argv) {
  std::optional<Args> s = parse_args(argc, argv);
  if (s == std::nullopt) {
    return -1;
  }

  Args sa = s.value();
  std::cout << "INFO: using " << sa.root_dir << " as root directory.\n";

  Universe uv{};
  sp::parse(sa, uv);
  sl::lint(uv, sa.root_dir);
  se::put(uv, sa);

  return 0;
}
