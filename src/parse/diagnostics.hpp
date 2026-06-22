#ifndef PICO_DIAGNOSTIC_HH
#define PICO_DIAGNOSTIC_HH

#include <shared.hpp>

namespace pico
{
  class Diagnostics {
  public:
    enum class Severity : u8;
    struct Snippet;
    struct Message;

    Diagnostics() noexcept = default;

    [[nodiscard]] auto messages() const noexcept -> const std::vector<Message>& { return messages_; }

    template<typename ...Args>
    auto AddMessage(Severity severity, std::format_string<Args...> format, Args ...args) -> void {
      messages_.emplace_back(std::format(format, std::forward<Args>(args)...), std::nullopt, severity);
    }

    template<typename ...Args>
    auto AddMessage(Severity severity, Snippet& snippet, std::format_string<Args...> format, Args ...args) -> void {
      messages_.emplace_back(std::format(format, std::forward<Args>(args)...), std::move(snippet), severity);
    }

    auto DoesLastHaveSnippet() const -> bool;

    auto AddSnippetToLastMessage(Snippet&& snippet) -> void;

    auto PrintAll() const -> void;

  private:
    std::vector<Message> messages_;
  };

  enum class Diagnostics::Severity : u8 {
    Information,
    Tip,
    Warning,
    Error
  };

  struct Diagnostics::Snippet {
    uint16_t line;
    uint16_t column;
    std::string file_name;
    std::string text;
  };

  struct Diagnostics::Message {
    std::string text;
    std::optional<Snippet> snippet;
    Severity severity;
  };
}

#endif