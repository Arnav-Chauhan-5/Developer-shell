// ─────────────────────────────────────────────────────────────
//  Developer Shell v1.0 — Entry Point
//  A custom TUI shell built with FTXUI and C++20.
//
//  Phase 8b: Batched Streaming & History Cap
//    • Custom block-cursor (inverted char at cursor_pos)
//    • Async external-command execution with batched UI
//      updates (flush every 20 lines or 50 ms)
//    • Mouse-wheel scrolling with auto-snap-to-bottom on typing
//    • Terminal stream capped at 500 lines to prevent lag
//  Preserves Phase 5 (arrow-key history), Phase 6 (Tab
//  auto-completion), Phase 7 (terminal stream UI).
// ─────────────────────────────────────────────────────────────

#include "builtins.h"
#include "command.h"
#include "parser.h"
#include "registry.h"
#include "shell.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>

#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

// ── Constants ────────────────────────────────────────────────
static constexpr std::size_t kMaxStreamLines = 500;  // history cap
static constexpr std::size_t kBatchSize      = 100;  // lines per flush
static constexpr int         kFlushIntervalMs = 100;  // max ms between flushes

// ── Helper: build the colored bash-style prompt element ──────
static Element MakePrompt() {
  return hbox({
      text("devshell") | bold | color(Color::Green),
      text(":")        | color(Color::GrayLight),
      text("~ ")       | bold | color(Color::Blue),
      text("$ ")       | color(Color::GrayLight),
  });
}

// ── Helper: trim terminal_stream to kMaxStreamLines ──────────
// Erases the oldest lines from the front of the vector so the
// total size never exceeds the cap.  Must be called on the main
// thread (or under a lock that covers terminal_stream).
static void TrimStream(std::vector<Element>& stream) {
  if (stream.size() > kMaxStreamLines) {
    stream.erase(stream.begin(),
                 stream.begin() +
                     static_cast<std::ptrdiff_t>(
                         stream.size() - kMaxStreamLines));
  }
}

int main() {
  // ── State ────────────────────────────────────────────────
  std::string input_text;                     // current input buffer
  int cursor_pos = 0;                         // FTXUI cursor sync position
  std::vector<Element> terminal_stream;       // rendered history (Elements)

  // ── Command History ─────────────────────────────────────
  std::vector<std::string> command_history;   // previously executed commands
  int history_index = -1;                     // -1 = not navigating

  // ── Async Execution State ──────────────────────────────
  bool is_running = false;                    // true while an external cmd streams
  std::atomic<bool> should_stop{false};       // cross-thread cancellation flag
  std::thread worker_thread;                  // background execution thread

  // ── Scroll State ───────────────────────────────────────
  float scroll_y = 1.0f;                     // 1.0 = bottom, 0.0 = top

  // ── Command Registry Setup ───────────────────────────────
  devshell::CommandRegistry registry;

  registry.registerCommand(std::make_unique<devshell::ExitCommand>());
  registry.registerCommand(std::make_unique<devshell::ClearCommand>());
  registry.registerCommand(std::make_unique<devshell::EchoCommand>());
  registry.registerCommand(std::make_unique<devshell::HelpCommand>(&registry));
  registry.registerCommand(std::make_unique<devshell::ChangeDirectoryCommand>());

  // Seed the stream with a welcome banner
  terminal_stream.push_back(
      text("Welcome to " + std::string(devshell::kTitle))
      | bold | color(Color::Cyan));
  terminal_stream.push_back(
      text("Type 'help' for available commands.") | color(Color::GrayDark));
  terminal_stream.push_back(text(""));

  // Forward-declare screen so lambdas can call screen.Exit() / screen.Post()
  auto screen = ScreenInteractive::Fullscreen();

  // ── Input Component ──────────────────────────────────────

  auto input_option = InputOption::Default();
  input_option.placeholder = "";
  input_option.cursor_position = &cursor_pos;

  // ── Custom cursor transform ──────────────────────────────
  // Removes the default full-line highlight.  Renders a standard
  // terminal block cursor: the character at cursor_pos is drawn
  // with inverted colors.  If the cursor is past the end of the
  // string an inverted space is appended.
  input_option.transform = [&](InputState state) {
    // When unfocused or a command is actively running, render
    // plain text with no cursor decoration.
    if (!state.focused || is_running) {
      return text(input_text);
    }

    int pos = std::clamp(cursor_pos, 0, static_cast<int>(input_text.size()));

    std::string before = input_text.substr(0, pos);
    std::string cursor_ch =
        (pos < static_cast<int>(input_text.size()))
            ? std::string(1, input_text[pos])
            : " ";
    std::string after =
        (pos < static_cast<int>(input_text.size()))
            ? input_text.substr(pos + 1)
            : "";

    return hbox({
        text(before),
        text(cursor_ch) | inverted,
        text(after),
    });
  };

  // ── on_enter: dispatch commands ──────────────────────────
  input_option.on_enter = [&] {
    if (is_running || input_text.empty())
      return;

    // Save command to history and reset navigation
    command_history.push_back(input_text);
    history_index = -1;

    // Freeze the prompt + typed text into the stream
    terminal_stream.push_back(hbox({
        MakePrompt(),
        text(input_text),
    }));

    // Tokenize and clear input
    auto tokens = devshell::tokenize(input_text);
    input_text.clear();
    cursor_pos = 0;
    scroll_y = 1.0f;

    if (tokens.empty())
      return;

    // ────────────────────────────────────────────────────────
    //  Builtin path — synchronous dispatch
    // ────────────────────────────────────────────────────────
    if (registry.hasCommand(tokens[0])) {
      std::string result = registry.dispatch(tokens);

      // Control-flow signals
      if (result == devshell::kSignalExit) {
        terminal_stream.push_back(
            text("Goodbye.") | color(Color::GrayDark));
        screen.Exit();
        return;
      }
      if (result == devshell::kSignalClear) {
        terminal_stream.clear();
        return;
      }

      // Append output line-by-line
      if (!result.empty()) {
        std::string::size_type pos = 0;
        std::string::size_type prev = 0;
        while ((pos = result.find('\n', prev)) != std::string::npos) {
          terminal_stream.push_back(text(result.substr(prev, pos - prev)));
          prev = pos + 1;
        }
        terminal_stream.push_back(text(result.substr(prev)));
        TrimStream(terminal_stream);
      }
      return;
    }

    // ────────────────────────────────────────────────────────
    //  External command path — async streaming via std::thread
    // ────────────────────────────────────────────────────────

    // Reconstruct the full command string from tokens
    std::string cmd;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      if (i > 0)
        cmd += ' ';
      cmd += tokens[i];
    }
    cmd += " 2>&1"; // merge stderr into stdout

    // Join any leftover previous worker (should be finished already)
    if (worker_thread.joinable())
      worker_thread.join();

    is_running = true;
    should_stop = false;

    worker_thread = std::thread([&, cmd]() {
      FILE *pipe = _popen(cmd.c_str(), "r");
      if (!pipe) {
        screen.Post([&, cmd]() {
          terminal_stream.push_back(
              text("devshell: failed to start process: " + cmd)
              | color(Color::Red));
          is_running = false;
        });
        return;
      }

      // ── Lock-free double-buffer accumulation ─────────────
      // The worker reads from the OS pipe into a purely local
      // vector (no shared state, no mutex).  When the batch is
      // ready it is std::move'd into a screen.Post() lambda
      // that executes on the FTXUI main thread — the only
      // thread that ever touches terminal_stream — so no lock
      // is needed for the final insert either.
      //
      // Flush triggers:
      //   • local_batch.size() >= kBatchSize  (100 lines)
      //   • kFlushIntervalMs elapsed          (100 ms)
      //   • subprocess finished               (final flush)

      std::vector<Element> local_batch;
      local_batch.reserve(kBatchSize);

      auto last_flush = std::chrono::steady_clock::now();

      // Move the local batch into a Post() for the main thread.
      auto FlushBatch = [&]() {
        if (local_batch.empty())
          return;
        // Capture by value — the main-thread lambda owns the data
        screen.Post([&, batch = std::move(local_batch)]() {
          terminal_stream.insert(terminal_stream.end(),
                                 batch.begin(), batch.end());
          TrimStream(terminal_stream);
          scroll_y = 1.0f;
        });
        // Reset local accumulator for the next batch
        local_batch.clear();
        local_batch.reserve(kBatchSize);
        last_flush = std::chrono::steady_clock::now();
      };

      // Read subprocess output with line accumulation so
      // partial fgets buffers don't split a single line.
      std::string line_buffer;
      std::array<char, 256> buffer{};

      while (!should_stop.load(std::memory_order_relaxed) &&
             std::fgets(buffer.data(),
                        static_cast<int>(buffer.size()), pipe)) {
        line_buffer += buffer.data();

        // Extract every complete line (delimited by '\n')
        std::string::size_type nl;
        while ((nl = line_buffer.find('\n')) != std::string::npos) {
          std::string line = line_buffer.substr(0, nl);
          line_buffer.erase(0, nl + 1);

          if (!line.empty() && line.back() == '\r')
            line.pop_back();

          local_batch.push_back(text(line));
        }

        // Flush if batch is full or time interval elapsed
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_flush)
                .count();

        if (local_batch.size() >= kBatchSize ||
            elapsed_ms >= kFlushIntervalMs) {
          FlushBatch();
        }
      }

      // Flush any remaining partial line (no trailing newline)
      if (!line_buffer.empty()) {
        if (!line_buffer.empty() && line_buffer.back() == '\r')
          line_buffer.pop_back();
        if (!line_buffer.empty() && line_buffer.back() == '\n')
          line_buffer.pop_back();
        local_batch.push_back(text(line_buffer));
      }

      // Final flush — push remaining lines to the main thread
      FlushBatch();

      _pclose(pipe);

      // Signal the main thread that execution is done.
      screen.Post([&]() {
        is_running = false;
      });
    });
  };

  Component input_box = Input(&input_text, input_option);

  // ── Tab auto-completion & arrow-key history navigation ──
  //
  // When is_running is true, ALL keyboard events are swallowed
  // so the user cannot interfere with a streaming command.
  input_box = CatchEvent(input_box, [&](Event event) {
    // Disable all keyboard input while a command is streaming
    if (is_running)
      return true;

    // ── Tab auto-completion ──────────────────────────────
    if (event == Event::Tab) {
      if (input_text.empty())
        return true;

      auto last_space = input_text.rfind(' ');
      std::string prefix =
          (last_space == std::string::npos)
              ? input_text
              : input_text.substr(last_space + 1);

      if (prefix.empty())
        return true;

      bool is_command = (last_space == std::string::npos);
      std::string match;

      // Command-name completion (first word only)
      if (is_command) {
        auto names = registry.getCommandNames();
        for (const auto &name : names) {
          if (name.size() >= prefix.size() &&
              name.compare(0, prefix.size(), prefix) == 0) {
            match = name;
            break;
          }
        }
      }

      // File/directory completion (arguments, or fallback for first word)
      if (match.empty()) {
        std::vector<std::string> fs_matches;
        try {
          namespace fs = std::filesystem;
          for (const auto &entry :
               fs::directory_iterator(fs::current_path())) {
            std::string fname = entry.path().filename().string();
            if (fname.size() >= prefix.size() &&
                fname.compare(0, prefix.size(), prefix) == 0) {
              if (entry.is_directory())
                fname += fs::path::preferred_separator;
              fs_matches.push_back(fname);
            }
          }
        } catch (...) {
        }
        if (!fs_matches.empty()) {
          std::sort(fs_matches.begin(), fs_matches.end());
          match = fs_matches.front();
        }
      }

      if (!match.empty()) {
        if (last_space == std::string::npos) {
          input_text = match;
        } else {
          input_text = input_text.substr(0, last_space + 1) + match;
        }
        cursor_pos = static_cast<int>(input_text.size());
        scroll_y = 1.0f;
      }
      return true;
    }

    // ── Arrow Up — history navigation ────────────────────
    if (event == Event::ArrowUp) {
      if (command_history.empty())
        return true;
      if (history_index == -1) {
        history_index = static_cast<int>(command_history.size()) - 1;
      } else if (history_index > 0) {
        --history_index;
      }
      input_text = command_history[history_index];
      cursor_pos = static_cast<int>(input_text.size());
      scroll_y = 1.0f;
      return true;
    }

    // ── Arrow Down — history navigation ──────────────────
    if (event == Event::ArrowDown) {
      if (history_index == -1)
        return true;
      if (history_index < static_cast<int>(command_history.size()) - 1) {
        ++history_index;
        input_text = command_history[history_index];
        cursor_pos = static_cast<int>(input_text.size());
      } else {
        history_index = -1;
        input_text.clear();
        cursor_pos = 0;
      }
      scroll_y = 1.0f;
      return true;
    }
    return false;
  });

  // ── Renderer ─────────────────────────────────────────────
  //
  //  Pure terminal stream.  The live input line sits at the
  //  very bottom of the vbox; focusPositionRelative + yframe
  //  handle scrolling, driven by scroll_y.

  auto renderer = Renderer(input_box, [&] {
    Elements stream;
    stream.reserve(terminal_stream.size() + 2);

    for (auto &el : terminal_stream) {
      stream.push_back(el);
    }

    // Live input line — always rendered so FTXUI's focus system
    // stays connected to the Input component.  During command
    // execution the prompt is hidden (input_text is empty and
    // the transform returns empty text when is_running is true).
    if (!is_running) {
      stream.push_back(hbox({
          MakePrompt(),
          input_box->Render() | flex,
      }));
    } else {
      // Keep the component in the tree for FTXUI focus management
      // but give it zero visual height.
      stream.push_back(input_box->Render() | size(HEIGHT, EQUAL, 0));
    }

    return vbox(std::move(stream))
        | focusPositionRelative(0.0f, scroll_y)
        | yframe
        | vscroll_indicator
        | flex;
  });

  // ── Outer event handler — Ctrl-C, mouse scroll, auto-snap ─
  auto final_component = CatchEvent(renderer, [&](Event event) {
    // ── Ctrl-C: quit ─────────────────────────────────────
    if (event == Event::Special("\x03")) {
      should_stop = true;
      screen.Exit();
      return true;
    }

    // ── Mouse wheel scrolling ────────────────────────────
    if (event.is_mouse()) {
      if (event.mouse().button == Mouse::WheelUp) {
        float step = std::min(
            0.3f,
            3.0f / std::max(1, static_cast<int>(terminal_stream.size())));
        scroll_y = std::max(0.0f, scroll_y - step);
        return true;
      }
      if (event.mouse().button == Mouse::WheelDown) {
        float step = std::min(
            0.3f,
            3.0f / std::max(1, static_cast<int>(terminal_stream.size())));
        scroll_y = std::min(1.0f, scroll_y + step);
        return true;
      }
      return false; // other mouse events pass through
    }

    // ── Auto-snap to bottom on any keyboard input ────────
    // This fires for regular typing, Enter, Tab, arrows, etc.
    // It does NOT fire for Custom events (from screen.Post),
    // so streaming output won't fight the user's scroll.
    scroll_y = 1.0f;
    return false; // let event propagate to inner components
  });

  // ── Run ──────────────────────────────────────────────────
  screen.Loop(final_component);

  // ── Cleanup ──────────────────────────────────────────────
  should_stop = true;
  if (worker_thread.joinable())
    worker_thread.join();

  return 0;
}
