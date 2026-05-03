// ─────────────────────────────────────────────────────────────
//  Built-in Commands Implementation
// ─────────────────────────────────────────────────────────────

#include "builtins.h"
#include "shell.h"

#include <algorithm>
#include <sstream>

namespace devshell {

// ═════════════════════════════════════════════════════════════
//  HelpCommand
// ═════════════════════════════════════════════════════════════

HelpCommand::HelpCommand(const CommandRegistry *registry)
    : registry_(registry) {}

std::string HelpCommand::execute(const std::vector<std::string> & /*args*/) {
  std::ostringstream oss;
  oss << "┌─────────────────────────────────────┐\n";
  oss << "│  " << kTitle << " — Available Commands  │\n";
  oss << "├─────────────────────────────────────┤\n";

  // Collect and sort command names for deterministic output
  std::vector<std::pair<std::string, std::string>> entries;
  entries.reserve(registry_->commands().size());
  for (const auto &[name, cmd] : registry_->commands()) {
    entries.emplace_back(name, cmd->getDescription());
  }
  std::sort(entries.begin(), entries.end());

  for (const auto &[name, desc] : entries) {
    oss << "│  " << name;
    // Pad to align descriptions
    const int pad = 12 - static_cast<int>(name.size());
    for (int i = 0; i < pad; ++i)
      oss << ' ';
    oss << desc << "\n";
  }

  oss << "└─────────────────────────────────────┘";
  return oss.str();
}

std::string HelpCommand::getName() const { return "help"; }
std::string HelpCommand::getDescription() const {
  return "Show available commands";
}

// ═════════════════════════════════════════════════════════════
//  ExitCommand
// ═════════════════════════════════════════════════════════════

std::string ExitCommand::execute(const std::vector<std::string> & /*args*/) {
  return kSignalExit;
}

std::string ExitCommand::getName() const { return "exit"; }
std::string ExitCommand::getDescription() const { return "Exit the shell"; }

// ═════════════════════════════════════════════════════════════
//  ClearCommand
// ═════════════════════════════════════════════════════════════

std::string ClearCommand::execute(const std::vector<std::string> & /*args*/) {
  return kSignalClear;
}

std::string ClearCommand::getName() const { return "clear"; }
std::string ClearCommand::getDescription() const { return "Clear the screen"; }

// ═════════════════════════════════════════════════════════════
//  EchoCommand
// ═════════════════════════════════════════════════════════════

std::string EchoCommand::execute(const std::vector<std::string> &args) {
  if (args.size() <= 1) {
    return "";
  }

  std::ostringstream oss;
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (i > 1)
      oss << ' ';
    oss << args[i];
  }
  return oss.str();
}

std::string EchoCommand::getName() const { return "echo"; }
std::string EchoCommand::getDescription() const {
  return "Print arguments to output";
}

} // namespace devshell
