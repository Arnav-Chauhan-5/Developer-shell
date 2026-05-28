// ─────────────────────────────────────────────────────────────
//  CommandRegistry Implementation
// ─────────────────────────────────────────────────────────────

#include "registry.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace devshell {

void CommandRegistry::registerCommand(std::unique_ptr<Command> cmd) {
  if (!cmd)
    return;

  // Extract name before moving ownership into the map
  std::string name = cmd->getName();
  commands_[std::move(name)] = std::move(cmd);
}

std::string
CommandRegistry::dispatch(const std::vector<std::string> &tokens) const {
  if (tokens.empty()) {
    return "";
  }

  const auto &command_name = tokens[0];
  auto it = commands_.find(command_name);

  // ── Built-in command found → execute it directly ──
  if (it != commands_.end()) {
    return it->second->execute(tokens);
  }

  // ── Not a built-in → forward to the OS via _popen ──

  // Reconstruct the full command string from tokens
  std::string full_command;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (i > 0)
      full_command += ' ';
    full_command += tokens[i];
  }

  // Redirect stderr into stdout so we capture everything
  full_command += " 2>&1";

  FILE *pipe = _popen(full_command.c_str(), "r");
  if (!pipe) {
    return "devshell: failed to start process: " + full_command;
  }

  // Read the pipe output into a string buffer
  std::string output;
  std::array<char, 256> buffer{};

  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    output += buffer.data();
  }

  _pclose(pipe);

  // Trim a single trailing newline for cleaner display
  if (!output.empty() && output.back() == '\n') {
    output.pop_back();
  }

  return output;
}

bool CommandRegistry::hasCommand(const std::string &name) const {
  return commands_.contains(name);
}

const std::unordered_map<std::string, std::unique_ptr<Command>> &
CommandRegistry::commands() const {
  return commands_;
}

std::vector<std::string> CommandRegistry::getCommandNames() const {
  std::vector<std::string> names;
  names.reserve(commands_.size());
  for (const auto& [name, _] : commands_) {
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

} // namespace devshell
