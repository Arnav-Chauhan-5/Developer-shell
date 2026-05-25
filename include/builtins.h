#ifndef DEVSHELL_BUILTINS_H
#define DEVSHELL_BUILTINS_H

// ─────────────────────────────────────────────────────────────
//  Built-in Commands — Concrete implementations of Command.
//
//  Each builtin is a final class (no further inheritance needed)
//  with deterministic, allocation-minimal implementations.
//
//  HelpCommand holds a raw pointer to the registry for
//  introspection — the registry must outlive the command.
//  This is guaranteed by construction order in main().
// ─────────────────────────────────────────────────────────────

#include "command.h"
#include "registry.h"

#include <string>
#include <vector>

namespace devshell {

// ── help ─────────────────────────────────────────────────────
class HelpCommand final : public Command {
public:
  /// Construct with a non-owning pointer to the registry.
  /// The registry MUST outlive this command instance.
  explicit HelpCommand(const CommandRegistry *registry);

  std::string execute(const std::vector<std::string> &args) override;
  std::string getName() const override;
  std::string getDescription() const override;

private:
  const CommandRegistry *registry_;
};

// ── exit ─────────────────────────────────────────────────────
class ExitCommand final : public Command {
public:
  std::string execute(const std::vector<std::string> &args) override;
  std::string getName() const override;
  std::string getDescription() const override;
};

// ── clear ────────────────────────────────────────────────────
class ClearCommand final : public Command {
public:
  std::string execute(const std::vector<std::string> &args) override;
  std::string getName() const override;
  std::string getDescription() const override;
};

// ── echo ─────────────────────────────────────────────────────
class EchoCommand final : public Command {
public:
  std::string execute(const std::vector<std::string> &args) override;
  std::string getName() const override;
  std::string getDescription() const override;
};

// ── cd (change directory) ────────────────────────────────────
class ChangeDirectoryCommand final : public Command {
public:
  std::string execute(const std::vector<std::string> &args) override;
  std::string getName() const override;
  std::string getDescription() const override;
};

} // namespace devshell

#endif // DEVSHELL_BUILTINS_H
