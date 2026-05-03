#ifndef DEVSHELL_COMMAND_H
#define DEVSHELL_COMMAND_H

// ─────────────────────────────────────────────────────────────
//  Command — Abstract base class for all shell commands.
//
//  Design rationale:
//    • Pure virtual interface enables polymorphic dispatch via
//      std::unique_ptr<Command>, stored in the registry's map.
//    • execute() returns a string rather than printing directly,
//      keeping commands decoupled from the TUI rendering layer.
//    • Special sentinel strings ("__EXIT__", "__CLEAR__") allow
//      commands to signal control flow to the main loop without
//      coupling to FTXUI internals.
// ─────────────────────────────────────────────────────────────

#include <string>
#include <vector>

namespace devshell {

// Sentinel return values for control-flow commands
inline constexpr const char* kSignalExit  = "__EXIT__";
inline constexpr const char* kSignalClear = "__CLEAR__";

class Command {
public:
    virtual ~Command() = default;

    /// Execute the command with the given arguments (argv[0] is the command name).
    /// Returns output text to display, or a sentinel signal for control-flow.
    [[nodiscard]]
    virtual std::string execute(const std::vector<std::string>& args) = 0;

    /// The canonical name used to invoke this command (e.g. "help", "exit").
    [[nodiscard]]
    virtual std::string getName() const = 0;

    /// A one-line description shown in help output.
    [[nodiscard]]
    virtual std::string getDescription() const = 0;

    // Non-copyable, movable
    Command() = default;
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = default;
    Command& operator=(Command&&) = default;
};

} // namespace devshell

#endif // DEVSHELL_COMMAND_H
