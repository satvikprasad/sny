#include "templater.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

inline std::string
build_literal (const std::string &body, const std::string &text_arg,
               const std::string &aux_arg)
{
  const bool as_format = !text_arg.empty () || !aux_arg.empty ();
  std::string lit;

  for (size_t i = 0; i < body.size ();)
    {
      if (!aux_arg.empty () && body.compare (i, 5, "(aux)") == 0)
        {
          lit += aux_arg;
          i += 5;
          continue;
        }

      if (!text_arg.empty () && body.compare (i, 4, "{{}}") == 0)
        {
          lit += text_arg;
          i += 4;
          continue;
        }

      switch (body[i])
        {
        case '{':
          lit += as_format ? "{{" : "{";
          break;
        case '}':
          lit += as_format ? "}}" : "}";
          break;
        case '"':
          lit += "\\\"";
          break;
        case '\\':
          lit += "\\\\";
          break;
        case '\n':
          lit += "\\n";
          break;
        case '\t':
          lit += "\\t";
          break;
        default:
          lit += body[i];
        }

      ++i;
    }

  return lit;
}

struct Put
{
  std::string params, stmt;
};

inline Put
build_put (const std::string &body)
{
  const bool has_aux = body.find ("(aux)") != std::string::npos;
  const bool has_text = body.find ("{{}}") != std::string::npos;

  std::string params, args, text_arg, aux_arg;

  if (has_text && has_aux)
    {
      params = "const std::string &text, const uint32_t aux, std::string &buf";
      args = ", text, aux";
      text_arg = "{0}";
      aux_arg = "{1}";
    }
  else if (has_aux)
    {
      params = "const uint32_t aux, std::string &buf";
      args = ", aux";
      aux_arg = "{0}";
    }
  else if (has_text)
    {
      params = "const std::string &text, std::string &buf";
      args = ", text";
      text_arg = "{0}";
    }
  else
    {
      params = "std::string &buf";
    }

  const std::string lit = build_literal (body, text_arg, aux_arg);

  Put put;
  put.params = params;
  put.stmt = has_text || has_aux
                 ? "buf += std::format(\"" + lit + "\"" + args + ");"
                 : "buf += \"" + lit + "\";";

  return put;
}

inline std::string
read_body (std::ifstream &in)
{
  std::string body, line;

  while (std::getline (in, line) && !line.empty ())
    {
      body += line;
      body += '\n';
    }

  return body;
}

inline void
split_body (const std::string &body, std::string &header, std::string &footer)
{
  const size_t children = body.find ("()");

  if (children != std::string::npos)
    {
      header = body.substr (0, children);
      footer = body.substr (children + 2);
      return;
    }

  const size_t text_end = body.find ("{{}}");

  if (text_end == std::string::npos)
    {
      header = body;
      footer.clear ();
      return;
    }

  header = body.substr (0, text_end + 4);
  footer = body.substr (text_end + 4);
}

inline void
add_case (std::string &cases, const std::string &label,
          const std::string &body)
{
  if (body.empty ())
    return;

  cases += "\tcase " + label + ":\n\t\t" + build_put (body).stmt
           + "\n\t\tbreak;\n";
}

inline void
put_dispatch (std::ofstream &out, const std::string &name,
              const std::string &cases)
{
  out << "inline void tmpl_put_" << name
      << "([[maybe_unused]] const NodeKind kind,\n"
      << "\t\t[[maybe_unused]] const std::string &text,\n"
      << "\t\t[[maybe_unused]] const uint32_t aux, std::string &buf) {\n"
      << "\tswitch (kind) {\n"
      << cases << "\tdefault:\n\t\tassert(false && \"unhandled NodeKind\");\n"
      << "\t\tbreak;\n"
      << "\t}\n}\n\n";
}

int
main (int argc, char **argv)
{
  assert (argc == 2);

  std::filesystem::path out_dir = std::filesystem::path ("src/meta/");
  if (!std::filesystem::exists (out_dir))
    {
      std::filesystem::create_directory (out_dir);
    }

  std::filesystem::path out = out_dir / "template.h";
  std::filesystem::path tmpl = std::filesystem::path (argv[1]) / "tmpl.syd";
  assert (std::filesystem::exists (tmpl));

  std::ofstream out_file = std::ofstream (out);
  std::ifstream tmpl_file = std::ifstream (tmpl);
  std::string line;

  const std::regex head_pattern (R"(^([A-Za-z_][\w:]*)\s*->\s*$)");

  out_file << "#pragma once\n\n"
           << "#include <cassert>\n"
           << "#include <cstdint>\n"
           << "#include <format>\n"
           << "#include <string>\n\n"
           << "#include \"sydney.h\"\n\n";

  std::string header_cases, footer_cases;

  while (std::getline (tmpl_file, line))
    {
      if (line.empty ())
        continue;

      std::smatch matches;
      if (!std::regex_match (line, matches, head_pattern))
        {
          std::cerr << tmpl << ": expected a `label ->` header, got: " << line
                    << "\n";
          return 1;
        }

      const std::string label = matches[1];
      const std::string body = read_body (tmpl_file);

      if (label.starts_with ("NodeKind::"))
        {
          std::string header, footer;
          split_body (body, header, footer);

          add_case (header_cases, label, header);
          add_case (footer_cases, label, footer);
        }
      else if (label == "preamble" || label == "postamble")
        {
          const Put put = build_put (body);

          out_file << "inline void tmpl_put_" << label << "(" << put.params
                   << ") {\n\t" << put.stmt << "\n}\n\n";
        }
      else
        {
          std::cerr << tmpl << ": unknown label `" << label
                    << "`, expected NodeKind::*, preamble or postamble\n";
          return 1;
        }
    }

  put_dispatch (out_file, "header", header_cases);
  put_dispatch (out_file, "footer", footer_cases);

  return 0;
}
