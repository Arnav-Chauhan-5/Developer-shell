// ─────────────────────────────────────────────────────────────
//  Tokenizer Implementation
//
//  A hand-written state-machine lexer. No regex, no exceptions —
//  pure pointer arithmetic and branch logic. This is the kind of
//  low-level parsing you'd find in a real shell like bash or zsh.
// ─────────────────────────────────────────────────────────────

#include "parser.h"

#include <stdexcept>

namespace devshell {

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current_token;

    enum class State {
        Normal,       // outside any token
        InToken,      // inside an unquoted token
        InDouble,     // inside "..."
        InSingle,     // inside '...'
    };

    State state = State::Normal;
    const std::size_t len = input.size();

    for (std::size_t i = 0; i < len; ++i) {
        const char c = input[i];

        switch (state) {
            // ── Outside any token ───────────────────────────
            case State::Normal:
                if (c == ' ' || c == '\t') {
                    // Skip leading whitespace
                    continue;
                } else if (c == '"') {
                    state = State::InDouble;
                } else if (c == '\'') {
                    state = State::InSingle;
                } else if (c == '\\' && i + 1 < len) {
                    // Escaped char starts a new token
                    current_token += input[++i];
                    state = State::InToken;
                } else {
                    current_token += c;
                    state = State::InToken;
                }
                break;

            // ── Inside an unquoted token ────────────────────
            case State::InToken:
                if (c == ' ' || c == '\t') {
                    // Whitespace ends the current token
                    tokens.push_back(std::move(current_token));
                    current_token.clear();
                    state = State::Normal;
                } else if (c == '"') {
                    // Transition into double-quoted segment
                    // (continues the same token: e.g.  abc"def" → abcdef)
                    state = State::InDouble;
                } else if (c == '\'') {
                    state = State::InSingle;
                } else if (c == '\\' && i + 1 < len) {
                    current_token += input[++i];
                } else {
                    current_token += c;
                }
                break;

            // ── Inside double quotes ────────────────────────
            case State::InDouble:
                if (c == '"') {
                    // Closing quote — return to unquoted token mode
                    // (the token itself may continue if no space follows)
                    state = State::InToken;
                } else if (c == '\\' && i + 1 < len) {
                    // Only \" and \\ are special inside double quotes
                    const char next = input[i + 1];
                    if (next == '"' || next == '\\') {
                        current_token += next;
                        ++i;
                    } else {
                        current_token += c; // literal backslash
                    }
                } else {
                    current_token += c;
                }
                break;

            // ── Inside single quotes (no escaping) ──────────
            case State::InSingle:
                if (c == '\'') {
                    state = State::InToken;
                } else {
                    // Everything is literal inside single quotes
                    current_token += c;
                }
                break;
        }
    }

    // Flush any remaining token
    if (!current_token.empty() || state == State::InDouble || state == State::InSingle) {
        tokens.push_back(std::move(current_token));
    }

    return tokens;
}

} // namespace devshell
