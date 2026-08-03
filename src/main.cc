#include <iostream>
#include <filesystem>
#include <optional>
#include <fstream>
#include <vector>
#include <sstream>

#include "md4c.h"

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
	std::optional<SArgs> s = parse_args(argc, argv);
	if (s == std::nullopt) {
		return -1;
	}

	SArgs sa = s.value();
	std::cout << "INFO: using " << sa.root_dir << " as root directory.\n";

	for (const auto& entry : std::filesystem::directory_iterator(sa.root_dir)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		std::cout << "INFO: reading file " << entry << ".\n";
		
		std::ifstream file(entry.path());
		if (!file.is_open()) {
			std::cout << "ERROR: could not open file " << entry.path() << ".\n";
		}

		std::stringstream buffer;
		buffer << file.rdbuf();

		std::string source = buffer.str();

		struct MD_PARSER parser = {
		};

		md_parse(source.c_str(), source.size(), &parser, nullptr);
	}

	return 0;
}
