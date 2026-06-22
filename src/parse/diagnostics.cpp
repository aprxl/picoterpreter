#include "diagnostics.hpp"
#include <cassert>
#include <cmath>

namespace pico
{
namespace
{
auto PrintDiagnostic(const Diagnostics::Message& message) -> void {
  std::string prefix{ };
  switch (message.severity) {
    case Diagnostics::Severity::Information: prefix = "info"; break;
    case Diagnostics::Severity::Tip: prefix = "tip"; break;
    case Diagnostics::Severity::Warning: prefix = "warning"; break;
    case Diagnostics::Severity::Error: prefix = "error"; break;
  }
  std::println("{}: {}", prefix, message.text);
}

auto PrintSnippet(const Diagnostics::Message& message) -> void {
  const u8 line_str_size = static_cast<u8>(std::log10(message.snippet->line)) + 1;
  std::println(" ---> {}:{}:{}", message.snippet->file_name, message.snippet->line, message.snippet->column + 1);
  std::println("  {:>{}} |", "", line_str_size);
  std::println("  {} | {}", message.snippet->line, message.snippet->text);
  std::println("  {:>{}} | {:>{}}^ here", "", line_str_size, "", message.snippet->column);
}
} // namespace

auto Diagnostics::DoesLastHaveSnippet( ) const -> bool {
  if (messages_.empty( )) {
    return false;
  }
  return messages_.back( ).snippet.has_value( );
}

auto Diagnostics::AddSnippetToLastMessage(Snippet&& snippet) -> void {
  if (messages_.empty( )) {
    return;
  }

  auto& back = messages_.back( );
  assert(!back.snippet.has_value( ) && "Attempted to override the snippet of a message.");
  back.snippet = std::forward<Snippet>(snippet);
}

auto Diagnostics::PrintAll() const -> void {
  for (const auto& message : messages_) {
    PrintDiagnostic(message);
    if (message.snippet) {
      PrintSnippet(message);
    }
  }
}
} // namespace pico
