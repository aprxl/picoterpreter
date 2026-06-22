#include "cli.hpp"

#include <filesystem>

static constexpr usize MARGIN = 20;

namespace pico {

auto Cli::AddCommand(const char *name, Command command) -> void {
  commands_.insert(std::make_pair(name, std::move(command)));
}

auto Cli::Splash() const -> void {
  std::println("Picoblaze 6 interpreter, version {}, made by @aprxl.", VERSION);
  std::println("Usage: picoterpret <file_name> [options]");
  std::println("Available commands:\n");

  for (const auto &[name, cmd] : commands_) {
    std::println("--{}{:>{}}{}", name, "", MARGIN - name.size(),
                 cmd.description());
  }
}

auto Cli::Run(const std::vector<std::string_view> &params) const -> Options {
  Options options{};

  for (const auto &param : params) {
    /// Runtime commands are passed with double dashes always, so we check if this is an
    /// command or not.
    if (param.starts_with("--")) {
      const auto command_name = param.substr(2);
      if (!commands_.contains(command_name)) {
        std::println("Runtime command '{}' does not exist.", command_name);
        std::exit(1);
      }

      const auto& command = commands_.at(command_name);
      command.Execute(params, options);
    }
    else {
      namespace fs = std::filesystem;

      if (!fs::exists(param)) {
        std::println("File '{}' not found.", param);
        std::exit(1);
      }

      if (fs::is_directory(param)) {
        std::println("Path '{}' is a directory.", param);
        std::exit(1);
      }

      options.file_path = param;
    }
  }

  return options;
}

} // namespace pico
