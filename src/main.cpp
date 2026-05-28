// ─────────────────────────────────────────────────────────────
//  Developer Shell v1.0 — Entry Point
//  A custom TUI shell built with FTXUI and C++20.
//
//  Phase 6: Tab Auto-Completion
//  Tab completes command names (first word) and file/directory
//  names (subsequent words) from the current directory.
// ─────────────────────────────────────────────────────────────

#include "builtins.h"
#include "command.h"
#include "parser.h"
#include "registry.h"
#include "shell.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

int main() {
  // ── State ────────────────────────────────────────────────
  std::string input_text;                 // current input buffer
  int cursor_pos = 0;                     // FTXUI cursor sync position
  std::vector<std::string> history_lines; // visible output log

  // ── Command History ─────────────────────────────────────
  std::vector<std::string> command_history; // previously executed commands
  int history_index = -1;                   // -1 = not navigating

  // ── Command Registry Setup ───────────────────────────────
  devshell::CommandRegistry registry;

  // Register built-in commands.
  // NOTE: HelpCommand receives a raw observer pointer to the registry.
  // Construction order guarantees registry outlives all commands.
  registry.registerCommand(std::make_unique<devshell::ExitCommand>());
  registry.registerCommand(std::make_unique<devshell::ClearCommand>());
  registry.registerCommand(std::make_unique<devshell::EchoCommand>());
  // HelpCommand registered last so it can see all other commands
  registry.registerCommand(std::make_unique<devshell::HelpCommand>(&registry));
  registry.registerCommand(std::make_unique<devshell::ChangeDirectoryCommand>());

  // Seed the history with a welcome banner
  history_lines.emplace_back("Welcome to " + std::string(devshell::kTitle));
  history_lines.emplace_back("Type 'help' for available commands.");
  history_lines.emplace_back("");

  // Forward-declare screen so on_enter lambda can call screen.Exit()
  auto screen = ScreenInteractive::Fullscreen();

  // ── Components ───────────────────────────────────────────

  // Input field configuration
  auto input_option = InputOption::Default();
  input_option.placeholder = "Type a command...";
  input_option.cursor_position = &cursor_pos;
  input_option.on_enter = [&] {
    if (input_text.empty())
      return;

    // Save non-empty command to history and reset navigation index
    command_history.push_back(input_text);
    history_index = -1;

    // 1. Echo the raw input to history
    history_lines.push_back("❯ " + input_text);

    // 2. Tokenize
    auto tokens = devshell::tokenize(input_text);
    input_text.clear();

    if (tokens.empty())
      return;

    // 3. Dispatch through the registry
    std::string result = registry.dispatch(tokens);

    // 4. Handle control-flow signals
    if (result == devshell::kSignalExit) {
      history_lines.push_back("  Goodbye.");
      screen.Exit();
      return;
    }

    if (result == devshell::kSignalClear) {
      history_lines.clear();
      return;
    }

    // 5. Append command output to history (support multi-line output)
    if (!result.empty()) {
      // Split on newlines so each line renders independently
      std::string::size_type pos = 0;
      std::string::size_type prev = 0;
      while ((pos = result.find('\n', prev)) != std::string::npos) {
        history_lines.push_back("  " + result.substr(prev, pos - prev));
        prev = pos + 1;
      }
      // Last (or only) line
      history_lines.push_back("  " + result.substr(prev));
    }
  };

  Component input_box = Input(&input_text, input_option);

  // ── Tab auto-completion & arrow-key history navigation ──
  input_box = CatchEvent(input_box, [&](Event event) {
    // ── Tab auto-completion ──────────────────────────────
    if (event == Event::Tab) {
      if (input_text.empty())
        return true;

      // Find the last word (the fragment to complete)
      auto last_space = input_text.rfind(' ');
      std::string prefix =
          (last_space == std::string::npos)
              ? input_text
              : input_text.substr(last_space + 1);

      if (prefix.empty())
        return true;

      bool is_command = (last_space == std::string::npos);
      std::string match;

      // Try command-name completion when the cursor is on the first word
      if (is_command) {
        auto names = registry.getCommandNames(); // already sorted
        for (const auto& name : names) {
          if (name.size() >= prefix.size() &&
              name.compare(0, prefix.size(), prefix) == 0) {
            match = name;
            break;
          }
        }
      }

      // File/directory completion (also used as fallback for first word)
      if (match.empty()) {
        std::vector<std::string> fs_matches;
        try {
          namespace fs = std::filesystem;
          for (const auto& entry :
               fs::directory_iterator(fs::current_path())) {
            std::string fname = entry.path().filename().string();
            if (fname.size() >= prefix.size() &&
                fname.compare(0, prefix.size(), prefix) == 0) {
              // Append a trailing separator for directories
              if (entry.is_directory())
                fname += fs::path::preferred_separator;
              fs_matches.push_back(fname);
            }
          }
        } catch (...) {
          // Ignore filesystem errors silently
        }
        if (!fs_matches.empty()) {
          std::sort(fs_matches.begin(), fs_matches.end());
          match = fs_matches.front();
        }
      }

      // Replace the incomplete word with the full match
      if (!match.empty()) {
        if (last_space == std::string::npos) {
          input_text = match;
        } else {
          input_text =
              input_text.substr(0, last_space + 1) + match;
        }
        // Sync FTXUI cursor to the end of the new string
        cursor_pos = static_cast<int>(input_text.size());
      }
      return true;
    }

    // ── Arrow Up — history navigation ────────────────────
    if (event == Event::ArrowUp) {
      if (command_history.empty())
        return true;
      if (history_index == -1) {
        // Start navigating from the most recent command
        history_index = static_cast<int>(command_history.size()) - 1;
      } else if (history_index > 0) {
        --history_index;
      }
      input_text = command_history[history_index];
      cursor_pos = static_cast<int>(input_text.size());
      return true;
    }

    // ── Arrow Down — history navigation ──────────────────
    if (event == Event::ArrowDown) {
      if (history_index == -1)
        return true; // nothing to navigate
      if (history_index < static_cast<int>(command_history.size()) - 1) {
        ++history_index;
        input_text = command_history[history_index];
        cursor_pos = static_cast<int>(input_text.size());
      } else {
        // Past the end — clear input and stop navigating
        history_index = -1;
        input_text.clear();
        cursor_pos = 0;
      }
      return true;
    }
    return false;
  });

  // ── Renderer ─────────────────────────────────────────────
  //
  //  ┌─ Developer Shell v1.0 ───────────────────────┐
  //  │                                               │
  //  │   (scrollable history area)                   │
  //  │                                               │
  //  ├───────────────────────────────────────────────┤
  //  │ ❯ _                                           │
  //  └───────────────────────────────────────────────┘

  auto renderer = Renderer(input_box, [&] {
    // Build history elements
    Elements history_elements;
    history_elements.reserve(history_lines.size());
    for (const auto &line : history_lines) {
      history_elements.push_back(text(line));
    }

    // History pane — fills available vertical space
    auto history_pane =
        vbox(std::move(history_elements)) |
        focusPositionRelative(0.0f, 1.0f) // auto-scroll to bottom
        | yframe                          // enable vertical scrolling
        | flex;                           // fill remaining space

    // Input prompt line
    auto prompt_line = hbox({
        text("❯ ") | bold | color(Color::Green),
        input_box->Render() | flex,
    });

    // Compose full layout
    return vbox({
               // Title bar
               text(devshell::kTitle) | bold | center | color(Color::Cyan),
               separator(),

               // History area
               history_pane,

               // Separator + input
               separator(),
               prompt_line,
           }) |
           border | color(Color::GrayLight);
  });

  // ── Keybinding: Ctrl-C to quit ──────────────────────────
  auto final_component = CatchEvent(renderer, [&](Event event) {
    if (event == Event::Special("\x03")) {
      screen.Exit();
      return true;
    }
    return false;
  });

  // ── Run ──────────────────────────────────────────────────
  screen.Loop(final_component);

  return 0;
}
