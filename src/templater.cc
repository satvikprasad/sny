#include "templater.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

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

inline std::string
build_stmt (const std::string &chunk)
{
  const bool has_aux = chunk.find ("(aux)") != std::string::npos;
  const bool has_text = chunk.find ("{{}}") != std::string::npos;

  std::string args, text_arg, aux_arg;

  if (has_text && has_aux)
    {
      args = ", text, aux";
      text_arg = "{0}";
      aux_arg = "{1}";
    }
  else if (has_aux)
    {
      args = ", aux";
      aux_arg = "{0}";
    }
  else if (has_text)
    {
      args = ", text";
      text_arg = "{0}";
    }

  const std::string lit = build_literal (chunk, text_arg, aux_arg);

  return has_text || has_aux
             ? "buf += std::format(\"" + lit + "\"" + args + ");"
             : "buf += \"" + lit + "\";";
}

inline std::vector<std::string>
split_lines (const std::string &body)
{
  std::vector<std::string> lines;
  size_t start = 0;

  while (start < body.size ())
    {
      const size_t nl = body.find ('\n', start);

      if (nl == std::string::npos)
        {
          lines.push_back (body.substr (start));
          break;
        }

      lines.push_back (body.substr (start, nl - start + 1));
      start = nl + 1;
    }

  return lines;
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

struct Split
{
  std::string header, footer, indent;
  bool footer_indented;
};

inline bool
all_blank (const std::string &s, size_t from, size_t to)
{
  for (size_t i = from; i < to; ++i)
    if (s[i] != ' ' && s[i] != '\t')
      return false;

  return true;
}

inline Split
split_body (const std::string &body)
{
  Split split;
  split.footer_indented = false;

  const size_t children = body.find ("()");

  if (children == std::string::npos)
    {
      const size_t text_end = body.find ("{{}}");

      if (text_end == std::string::npos)
        split.header = body;
      else
        {
          split.header = body.substr (0, text_end + 4);
          split.footer = body.substr (text_end + 4);
        }

      return split;
    }

  const size_t nl = body.rfind ('\n', children);
  const size_t line_start = nl == std::string::npos ? 0 : nl + 1;

  const size_t after = children + 2;
  const size_t eol = body.find ('\n', after);
  const size_t line_end = eol == std::string::npos ? body.size () : eol;

  if (all_blank (body, line_start, children)
      && all_blank (body, after, line_end))
    {
      split.indent = body.substr (line_start, children - line_start);
      split.header = body.substr (0, line_start);
      split.footer = eol == std::string::npos ? "" : body.substr (eol + 1);
      split.footer_indented = true;

      return split;
    }

  split.header = body.substr (0, children);
  split.footer = body.substr (after);

  return split;
}

inline std::string
build_indented (const std::string &chunk, bool indent_first)
{
  std::string code;

  for (const std::string &line : split_lines (chunk))
    {
      if (indent_first && line != "\n")
        code += "\t\ttmpl_indent(depth, buf);\n";

      code += "\t\t" + build_stmt (line) + "\n";
      indent_first = true;
    }

  return code;
}

inline void
add_case (std::string &cases, const std::string &label,
          const std::string &chunk, bool indent_first)
{
  cases += "\tcase " + label + ":\n";

  if (!chunk.empty ())
    cases += build_indented (chunk, indent_first);

  cases += "\t\tbreak;\n";
}

inline void
put_dispatch (std::ofstream &out, const std::string &name,
              const std::string &cases)
{
  out << "inline void tmpl_put_" << name
      << "([[maybe_unused]] const NodeKind kind,\n"
      << "\t\t[[maybe_unused]] const std::string &text,\n"
      << "\t\t[[maybe_unused]] const uint32_t aux,\n"
      << "\t\t[[maybe_unused]] const uint32_t depth, std::string &buf) {\n"
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

  const std::regex head_pattern (R"(^([A-Za-z_][\w:]*)\s*->\s*(.*)$)");

  std::string header_cases, footer_cases, step_cases;
  char indent_char = '\t';

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
      std::string inlined = matches[2];

      while (!inlined.empty ()
             && (inlined.back () == ' ' || inlined.back () == '\t'))
        inlined.pop_back ();

      const bool is_inline = !inlined.empty ();
      const std::string body = is_inline ? inlined : read_body (tmpl_file);

      if (!label.starts_with ("NodeKind::"))
        {
          std::cerr << tmpl << ": unknown label `" << label
                    << "`, expected NodeKind::*\n";
          return 1;
        }

      const Split split = split_body (body);

      if (!split.indent.empty ())
        {
          if (indent_char != '\t' && indent_char != split.indent[0])
            {
              std::cerr << tmpl << ": " << label
                        << " indents () with a different character than an"
                           " earlier block\n";
              return 1;
            }

          indent_char = split.indent[0];
        }

      add_case (header_cases, label, split.header, !is_inline);
      add_case (footer_cases, label, split.footer,
                !is_inline && split.footer_indented);

      step_cases += "\tcase " + label + ":\n\t\treturn "
                    + std::to_string (split.indent.size ()) + ";\n";
    }

  out_file << "#pragma once\n\n"
           << "#include <cassert>\n"
           << "#include <cstdint>\n"
           << "#include <format>\n"
           << "#include <string>\n\n"
           << "#include \"sydney.h\"\n\n";

  out_file << "inline void tmpl_indent(const uint32_t depth, std::string "
              "&buf) {\n"
           << "\tbuf.append(depth, '"
           << (indent_char == '\t' ? "\\t" : " ") << "');\n"
           << "}\n\n";

  out_file << "inline uint32_t tmpl_indent_step(const NodeKind kind) {\n"
           << "\tswitch (kind) {\n"
           << step_cases
           << "\tdefault:\n\t\tassert(false && \"unhandled NodeKind\");\n"
           << "\t\treturn 0;\n"
           << "\t}\n}\n\n";

  put_dispatch (out_file, "header", header_cases);
  put_dispatch (out_file, "footer", footer_cases);

  return 0;
}
