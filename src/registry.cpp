// ─────────────────────────────────────────────────────────────
//  CommandRegistry Implementation
// ─────────────────────────────────────────────────────────────

#include "registry.h"

namespace devshell {

void CommandRegistry::registerCommand(std::unique_ptr<Command> cmd) {
    if (!cmd) return;

    // Extract name before moving ownership into the map
    std::string name = cmd->getName();
    commands_[std::move(name)] = std::move(cmd);
}

std::string CommandRegistry::dispatch(const std::vector<std::string>& tokens) const {
    if (tokens.empty()) {
        return "";
    }

    const auto& command_name = tokens[0];
    auto it = commands_.find(command_name);

    if (it == commands_.end()) {
        return "devshell: command not found: " + command_name;
    }

    return it->second->execute(tokens);
}

bool CommandRegistry::hasCommand(const std::string& name) const {
    return commands_.contains(name);
}

const std::unordered_map<std::string, std::unique_ptr<Command>>&
CommandRegistry::commands() const {
    return commands_;
}

} // namespace devshell
