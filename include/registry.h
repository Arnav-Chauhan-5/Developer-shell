#ifndef DEVSHELL_REGISTRY_H
#define DEVSHELL_REGISTRY_H

// ─────────────────────────────────────────────────────────────
//  CommandRegistry — Central dispatch table for shell commands.
//
//  Ownership model:
//    • Commands are stored as std::unique_ptr<Command> inside an
//      unordered_map keyed by command name.
//    • registerCommand() takes ownership via move semantics.
//    • The registry is non-copyable (owns unique resources).
//
//  Lookup is O(1) average via hash map. The dispatch() method
//  handles both lookup and execution in a single call.
// ─────────────────────────────────────────────────────────────

#include "command.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace devshell {

class CommandRegistry {
public:
    CommandRegistry() = default;

    /// Register a command. The registry takes exclusive ownership.
    /// If a command with the same name already exists, it is replaced.
    void registerCommand(std::unique_ptr<Command> cmd);

    /// Look up and execute a command by name.
    /// Returns the command's output string, or an error message if not found.
    [[nodiscard]]
    std::string dispatch(const std::vector<std::string>& tokens) const;

    /// Check if a command is registered under the given name.
    [[nodiscard]]
    bool hasCommand(const std::string& name) const;

    /// Get a read-only reference to the internal command map.
    /// Useful for the HelpCommand to enumerate all registered commands.
    [[nodiscard]]
    const std::unordered_map<std::string, std::unique_ptr<Command>>& commands() const;

    // Non-copyable (owns unique_ptrs)
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    CommandRegistry(CommandRegistry&&) = default;
    CommandRegistry& operator=(CommandRegistry&&) = default;

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};

} // namespace devshell

#endif // DEVSHELL_REGISTRY_H
