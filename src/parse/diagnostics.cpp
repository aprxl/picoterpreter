/// Silence some deprecated message here.
#define _CRT_SECURE_NO_WARNINGS
#include "diagnostics.hpp"
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace pico
{
namespace
{
constexpr std::string_view RESET  = "\x1b[0m";
constexpr std::string_view DIM    = "\x1b[2m";
constexpr std::string_view RED    = "\x1b[31m";
constexpr std::string_view YELLOW = "\x1b[33m";
constexpr std::string_view GREEN  = "\x1b[32m";
constexpr std::string_view CYAN   = "\x1b[36m";

/// Whether stdout should receive ANSI colors. Disabled when `NO_COLOR` is set or output doesn't support it,
/// so the output stays plain text.
auto Colored( ) -> bool {
  static const bool enabled = [] {
    if (std::getenv("NO_COLOR") != nullptr) {
      return false;
    }
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
  }( );
  return enabled;
}

/// Wraps `text` in a color escape (and a reset) when colors are enabled. `code` may be a
/// combination of attributes, e.g. `std::format("{}{}", BOLD, RED)`.
auto Paint(const std::string_view code, const std::string_view text) -> std::string {
  if (!Colored( )) {
    return std::string(text);
  }
  return std::format("{}{}{}", code, text, RESET);
}

/// Ensure the terminal is properly set up to handle the output.
auto EnsureTerminalSetup( ) -> void {
  static const bool done = [] {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
      SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    return true;
  }( );
  (void)done;
}

/// Foreground color for the triangle that points into the source snippet.
auto SeverityColor(const Diagnostics::Severity severity) -> std::string_view {
  switch (severity) {
    case Diagnostics::Severity::Information: return CYAN;
    case Diagnostics::Severity::Tip:         return GREEN;
    case Diagnostics::Severity::Warning:     return YELLOW;
    case Diagnostics::Severity::Error:       return RED;
  }
  return RESET;
}

/// Color parameters for the severity badge, as `attributes;foreground;background`.
auto SeverityBadgeStyle(const Diagnostics::Severity severity) -> std::string_view {
  switch (severity) {
    case Diagnostics::Severity::Information: return "1;97;44"; // bold white on blue
    case Diagnostics::Severity::Tip:         return "1;30;42"; // bold black on green
    case Diagnostics::Severity::Warning:     return "1;30;43"; // bold black on yellow
    case Diagnostics::Severity::Error:       return "1;97;41"; // bold white on red
  }
  return "0";
}

auto SeverityLabel(const Diagnostics::Severity severity) -> std::string_view {
  switch (severity) {
    case Diagnostics::Severity::Information: return "INFO";
    case Diagnostics::Severity::Tip:         return "TIP";
    case Diagnostics::Severity::Warning:     return "WARNING";
    case Diagnostics::Severity::Error:       return "ERROR";
  }
  return "?";
}

/// A padded, all-caps label rendered with a colored background; falls back to `[LABEL]`
/// when colors are disabled.
auto Badge(const Diagnostics::Severity severity) -> std::string {
  const auto label = SeverityLabel(severity);
  if (!Colored( )) {
    return std::format("[{}]", label);
  }
  return std::format("\x1b[{}m {} {}", SeverityBadgeStyle(severity), label, RESET);
}

auto PrintHeader(const Diagnostics::Message& message) -> void {
  const auto badge = Badge(message.severity);
  if (message.snippet) {
    const auto& snippet = *message.snippet;
    const auto location = Paint(CYAN,
      std::format("{}:{}:{}", snippet.file_name, snippet.line, snippet.column + 1));
    std::println("{} {} {}", badge, Paint(DIM, "·"), location);
  } else {
    std::println("{}", badge);
  }
  std::println("    {}", message.text);
}

auto PrintSnippet(const Diagnostics::Message& message) -> void {
  const auto& snippet = *message.snippet;
  const auto gutter = std::to_string(snippet.line).size( );
  const std::string bar_indent = std::format("  {: <{}} ", "", gutter);
  const auto bar = Paint(DIM, "│");
  std::println("{}{}", bar_indent, bar);
  std::println("  {} {} {}",
    Paint(DIM, std::format("{: >{}}", snippet.line, gutter)), bar, snippet.text);
  std::println("{}{} {}{}",
    bar_indent, bar, std::string(snippet.column, ' '),
    Paint(SeverityColor(message.severity), "▲"));
  std::println("{}{}", bar_indent, Paint(DIM, "╵"));
}
} // namespace

auto Diagnostics::PrintAll() const -> void {
  EnsureTerminalSetup( );
  for (const auto& message : messages_) {
    PrintHeader(message);
    if (message.snippet) {
      PrintSnippet(message);
    }
  }
}
} // namespace pico
