#include <iostream>
#include <filesystem>
#include <optional>

#include "sydney.h"

std::optional<SArgs> parse_args(char argc, char **argv) {
	if (argc <= 1) {
		std::cout << "Usage: sydney [path_to_dir]\n";
		return std::nullopt;
	}

	char *root_dir = argv[1];

	SArgs sa;
	sa.root_dir = std::filesystem::path(root_dir);

	if (!std::filesystem::exists(sa.root_dir)) {
		std::cout << "ERROR: [path_to_dir] should exist, got " << root_dir << ".\n";
		return std::nullopt;
	}

	return sa;
}

int main(int argc, char **argv) {
	std::optional<SArgs> sa = parse_args(argc, argv);
	if (sa == std::nullopt) {
		return -1;
	}
	
	std::cout << "INFO: using " << sa.value().root_dir << " as root directory.\n";

	return 0;
}
