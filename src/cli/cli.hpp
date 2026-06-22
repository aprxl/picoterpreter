#ifndef PICO_CLI_HH
#define PICO_CLI_HH

#include <shared.hpp>

namespace pico {

/// A helper class for handling runtime commands and arguments.
class Cli {
public:
  class Command;
  struct Options;

  explicit Cli() = default;

  auto commands() const noexcept
      -> const std::map<std::string_view, Command> & {
    return commands_;
  }

  /// Adds a new command to the Cli commands list.
  auto AddCommand(const char *name, Command command) -> void;
  /// Prints out the splash screen.
  auto Splash() const -> void;
  /// Runs commands given the runtime arguments.
  [[nodiscard]] auto Run(const std::vector<std::string_view> &params) const -> Options;

private:
  // Using std::map instead of std::unordered_map to automatically sort out
  // the command names, so printing in Cli::Splash prints out commands in
  // alphabetical order.
  std::map<std::string_view, Command> commands_;
};

/// A simple runtime command.
class Cli::Command {
  using Fn = std::function<bool(std::span<const std::string_view>, Options &)>;

public:
  explicit Command(const char *description, usize expects, Fn &&fn)
      : fn_(fn), description_(description), expects_(expects) {}

  auto expects() const noexcept -> usize { return expects_; }
  auto description() const noexcept -> const std::string_view & {
    return description_;
  }

  /// Executes this command given the runtime arguments.
  /// Automatically changes the runtime options accordingly.
  auto Execute(std::span<const std::string_view> args,
               Options &options) const noexcept -> bool {
    return fn_(args, options);
  }

private:
  Fn fn_;
  std::string_view description_;
  usize expects_;
};

struct Cli::Options {
  bool lsp_mode;
  bool dump_tokens;
  bool dump_ast;
  std::string file_path;
};

} // namespace pico

#endif
