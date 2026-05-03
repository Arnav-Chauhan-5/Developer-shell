#ifndef DEVSHELL_PARSER_H
#define DEVSHELL_PARSER_H

// ─────────────────────────────────────────────────────────────
//  Tokenizer — Splits raw input into argv-style token vectors.
//
//  Handles:
//    • Unquoted tokens split on whitespace
//    • Double-quoted strings  ("hello world" → single token)
//    • Single-quoted strings  ('hello world' → single token)
//    • Escaped characters inside double quotes  (\" → literal ")
//    • Backslash-space in unquoted context      (hello\ world → single token)
// ─────────────────────────────────────────────────────────────

#include <string>
#include <vector>

namespace devshell {

/// Tokenize a raw input line into a vector of argument strings.
/// Returns an empty vector if the input is blank or only whitespace.
[[nodiscard]]
std::vector<std::string> tokenize(const std::string& input);

} // namespace devshell

#endif // DEVSHELL_PARSER_H
