#ifndef PICO_LEXER_HH
#define PICO_LEXER_HH

#include "diagnostics.hpp"

#include <shared.hpp>
#include <utilities/formatting.hpp>
#include <variant>

namespace pico
{
class Lexer {
  using BufferIterator = std::string_view::iterator;
  static constexpr std::array<bool, 256> IDENTIFIER_VALID_FIRST = [] consteval {
    std::array<bool, 256> list{ };
    for (usize i = 0; i < 256; ++i) {
      if ((i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z')) {
        list[i] = true;
      }
    }
    list['_'] = true;
    return list;
  }();

  static constexpr std::array<bool, 256> IDENTIFIER_VALID = [] consteval {
    std::array<bool, 256> list = IDENTIFIER_VALID_FIRST;
    for (usize i = 0; i < 256; ++i) {
      if (i >= '0' && i <= '9') {
        list[i] = true;
      }
    }
    /// Some instructions in Picoblaze have special characters in them, so
    /// we include it in the list since we parse both instructions and identifiers together.
    list['&'] = true;
    list['@'] = true;
    return list;
  }();

  static constexpr std::array<bool, 256> NUMBER_VALID = [] consteval {
    std::array<bool, 256> list{ };
    for (usize i = 0; i < 256; ++i) {
      if ((i >= '0' && i <= '9') || (i >= 'A' && i <= 'F') || (i >= 'a' && i <= 'f')) {
        list[i] = true;
      }
    }
    return list;
  }();

  static constexpr std::array<bool, 256> OPERATORS = [] consteval {
    std::array<bool, 256> list{ };
    list['\''] = true;
    list['\"'] = true;
    list['~'] = true;
    list['['] = true;
    list[']'] = true;
    list[','] = true;
    list[':'] = true;
    list[';'] = true;
    list['%'] = true;
    list['#'] = true;
    list['$'] = true;
    return list;
  }();

  static constexpr std::array<bool, 256> WHITESPACE = [] consteval {
    std::array<bool, 256> list{ };
    for (usize i = 0; i < 256; ++i) {
      if (i == ' ' || i == '\t' || i == '\n' || i == '\r') {
        list[i] = true;
      }
    }
    return list;
  }();

public:
  enum TokenKind : u8;
  enum class InstructionKind : u8;
  enum class FlagKind : u8;
  enum class NumberLiteralBase : u8;
  struct SourceSpan;
  struct Token;
  class Context;

  Lexer() = default;

  [[nodiscard]] auto path() const noexcept -> std::string_view { return path_; }
  [[nodiscard]] auto tokens() const noexcept -> const std::vector<Token> & { return tokens_; }

  /// Sets the file path.
  auto SetFilePath(const std::string_view& path) noexcept -> Lexer & {
    path_ = path;
    return *this;
  }

  /// Reads the file at the configured path and lexes it, reporting into `diag`.
  auto Run(Diagnostics& diag) -> bool;
  /// Lexes an in-memory source buffer, reporting into `diag`. The pure, I/O-free core.
  auto Tokenize(std::string_view source, Diagnostics& diag) -> bool;

private:
  static auto TryIdentifier(Context& ctx) -> std::optional<Token>;
  static auto TryNarrowIdentifier(Context& ctx, Token& token) -> bool;
  static auto TryNarrowIntoFlag(std::string_view lower, Token& token) -> bool;
  static auto TryNarrowIntoRegister(std::string_view lower, Token& token) -> bool;
  static auto TryNarrowIntoInstruction(std::string_view lower, Token& token) -> bool;
  static auto TryNarrowIntoRegisterBank(char c, Token& token) -> bool;
  static auto TryNumber(Context& ctx) -> std::optional<Token>;
  static auto TryOperator(Context& ctx) -> std::optional<Token>;

  auto AddToken(Token&& token) -> bool;
  /// Resolves some token groups into a single token.
  auto Resolve(Diagnostics& diag) -> bool;
  auto TryResolveNumber(usize index, Diagnostics& diag) -> bool;

  std::string_view path_;
  std::vector<Token> tokens_;
};

enum Lexer::TokenKind : u8 {
  Invalid,
  InstructionOrDirective,
  Identifier,
  Register,
  Number,
  String,
  Flag,
  RegisterBank,
  /// Used in number base specifications: 'b, 'd; Upper/lower nibble specification: 'upper, 'lower
  SingleQuote,
  /// Used for character literals and string literals
  DoubleQuote,
  /// Bitwise not operator
  Tilda,
  /// Tables
  BracketLeft,
  BracketRight,
  Hashtag,
  /// Separator
  Comma,
  /// Labels
  Colon,
  /// Comments
  Semicolon,
  /// Environment variables
  Percent,
  /// String literals
  DollarSign
};

enum class Lexer::InstructionKind : u8 {
  Nop,
  Address,
  Add,
  AddCarry,
  Subtract,
  SubtractCarry,
  Input,
  Output,
  And,
  Call,
  CallAt,
  Compare,
  CompareCarry,
  Constant,
  DefaultJump,
  Disable,
  Enable,
  Fetch,
  HardwareBuild,
  InstructionInstantiation,
  Jump,
  JumpAt,
  Load,
  LoadAndReturn,
  NameRegister,
  Or,
  OutputK,
  RegisterBank,
  Return,
  ReturnInterrupt,
  RotateLeft,
  RotateRight,
  ShiftLeftWithZero,
  ShiftLeftWithOne,
  ShiftLeftWithCarry,
  ShiftLeftArith,
  ShiftRightWithZero,
  ShiftRightWithOne,
  ShiftRightWithCarry,
  ShiftRightArith,
  Star,
  Store,
  StringDefinition,
  TableDefinition,
  Test,
  TestCarry,
  Xor
};

enum class Lexer::FlagKind : u8 {
  Carry,
  Zero,
  NotCarry,
  NotZero,
};

enum class Lexer::NumberLiteralBase : u8 {
  Hexadecimal,
  Decimal,
  Binary
};

/// Location of a token within the source buffer. Carried on every token so the
/// parser can produce proper diagnostics.
struct Lexer::SourceSpan {
  u32 offset = 0;
  u16 line = 0;
  u16 column = 0;
  u16 length = 0;
};

struct Lexer::Token {
  TokenKind kind = Invalid;
  SourceSpan span{ };
  std::variant<u32, InstructionKind, FlagKind, std::string> value;

  template <typename T>
  T& value_as( ) { return std::get<T>(value); }
  template <typename T>
  const T& value_as( ) const { return std::get<T>(value); }
};

/// To help the code structure, we declare everything about an instruction/directive in a single struct.
/// This way, adding or changing new instructions is trivial and automatically handled everywhere.
struct InstructionInfo {
  std::string_view mnemonic;
  Lexer::InstructionKind kind;
  std::string_view name;
};

inline constexpr auto INSTRUCTION_TABLE = std::to_array<InstructionInfo>({
  /// `Nop` has no mnemonic: it is created by the parser, never lexed, but
  /// still needs a display name.
  { "",             Lexer::InstructionKind::Nop,                      "Nop" },
  { "address",      Lexer::InstructionKind::Address,                  "Address" },
  { "add",          Lexer::InstructionKind::Add,                      "Add" },
  { "addcy",        Lexer::InstructionKind::AddCarry,                 "AddCarry" },
  { "sub",          Lexer::InstructionKind::Subtract,                 "Subtract" },
  { "subcy",        Lexer::InstructionKind::SubtractCarry,            "SubtractCarry" },
  { "input",        Lexer::InstructionKind::Input,                    "Input" },
  { "output",       Lexer::InstructionKind::Output,                   "Output" },
  { "and",          Lexer::InstructionKind::And,                      "And" },
  { "call",         Lexer::InstructionKind::Call,                     "Call" },
  { "call@",        Lexer::InstructionKind::CallAt,                   "CallAt" },
  { "compare",      Lexer::InstructionKind::Compare,                  "Compare" },
  { "comparecy",    Lexer::InstructionKind::CompareCarry,             "CompareCarry" },
  { "constant",     Lexer::InstructionKind::Constant,                 "Constant" },
  { "default_jump", Lexer::InstructionKind::DefaultJump,              "DefaultJump" },
  { "disable",      Lexer::InstructionKind::Disable,                  "Disable" },
  { "enable",       Lexer::InstructionKind::Enable,                   "Enable" },
  { "fetch",        Lexer::InstructionKind::Fetch,                    "Fetch" },
  { "hwbuild",      Lexer::InstructionKind::HardwareBuild,            "HardwareBuild" },
  { "inst",         Lexer::InstructionKind::InstructionInstantiation, "InstructionInstantiation" },
  { "jump",         Lexer::InstructionKind::Jump,                     "Jump" },
  { "jump@",        Lexer::InstructionKind::JumpAt,                   "JumpAt" },
  { "load",         Lexer::InstructionKind::Load,                     "Load" },
  { "load&return",  Lexer::InstructionKind::LoadAndReturn,            "LoadAndReturn" },
  { "namereg",      Lexer::InstructionKind::NameRegister,             "NameRegister" },
  { "or",           Lexer::InstructionKind::Or,                       "Or" },
  { "outputk",      Lexer::InstructionKind::OutputK,                  "OutputK" },
  { "regbank",      Lexer::InstructionKind::RegisterBank,             "RegisterBank" },
  { "return",       Lexer::InstructionKind::Return,                   "Return" },
  { "returni",      Lexer::InstructionKind::ReturnInterrupt,          "ReturnInterrupt" },
  { "rl",           Lexer::InstructionKind::RotateLeft,               "RotateLeft" },
  { "rr",           Lexer::InstructionKind::RotateRight,              "RotateRight" },
  { "sl0",          Lexer::InstructionKind::ShiftLeftWithZero,        "ShiftLeftWithZero" },
  { "sl1",          Lexer::InstructionKind::ShiftLeftWithOne,         "ShiftLeftWithOne" },
  { "sla",          Lexer::InstructionKind::ShiftLeftWithCarry,       "ShiftLeftWithCarry" },
  { "slx",          Lexer::InstructionKind::ShiftLeftArith,           "ShiftLeftArith" },
  { "sr0",          Lexer::InstructionKind::ShiftRightWithZero,       "ShiftRightWithZero" },
  { "sr1",          Lexer::InstructionKind::ShiftRightWithOne,        "ShiftRightWithOne" },
  { "sra",          Lexer::InstructionKind::ShiftRightWithCarry,      "ShiftRightWithCarry" },
  { "srx",          Lexer::InstructionKind::ShiftRightArith,          "ShiftRightArith" },
  { "star",         Lexer::InstructionKind::Star,                     "Star" },
  { "store",        Lexer::InstructionKind::Store,                    "Store" },
  { "string",       Lexer::InstructionKind::StringDefinition,         "StringDefinition" },
  { "table",        Lexer::InstructionKind::TableDefinition,          "TableDefinition" },
  { "test",         Lexer::InstructionKind::Test,                     "Test" },
  { "testcy",       Lexer::InstructionKind::TestCarry,                "TestCarry" },
  { "xor",          Lexer::InstructionKind::Xor,                      "Xor" },
});

struct FlagInfo {
  std::string_view text;
  Lexer::FlagKind kind;
  std::string_view name;
};

inline constexpr auto FLAG_TABLE = std::to_array<FlagInfo>({
  { "c",  Lexer::FlagKind::Carry,    "Carry" },
  { "z",  Lexer::FlagKind::Zero,     "Zero" },
  { "nc", Lexer::FlagKind::NotCarry, "NotCarry" },
  { "nz", Lexer::FlagKind::NotZero,  "NotZero" },
});

/// Looks up a (lowercased) mnemonic. Returns `nullopt` for non-keywords.
constexpr auto InstructionFromMnemonic(std::string_view mnemonic) -> std::optional<Lexer::InstructionKind> {
  for (const auto& entry : INSTRUCTION_TABLE) {
    if (!entry.mnemonic.empty( ) && entry.mnemonic == mnemonic) {
      return entry.kind;
    }
  }
  return std::nullopt;
}

constexpr auto InstructionName(Lexer::InstructionKind kind) -> std::string_view {
  for (const auto& entry : INSTRUCTION_TABLE) {
    if (entry.kind == kind) {
      return entry.name;
    }
  }
  return "Unknown";
}

constexpr auto FlagFromText(std::string_view text) -> std::optional<Lexer::FlagKind> {
  for (const auto& entry : FLAG_TABLE) {
    if (entry.text == text) {
      return entry.kind;
    }
  }
  return std::nullopt;
}

constexpr auto FlagName(Lexer::FlagKind kind) -> std::string_view {
  for (const auto& entry : FLAG_TABLE) {
    if (entry.kind == kind) {
      return entry.name;
    }
  }
  return "Unknown";
}

class Lexer::Context {
public:
  enum class State {
    Default,
    RightAfterSingleQuote
  };

  Context(std::string_view source, std::string_view path, Diagnostics& diag) noexcept
    : contents_(source), path_(path), diag_(diag) {
    iterator_ = saved_ = line_start_ = contents_.begin( );
  }

  [[nodiscard]] auto state( ) const noexcept -> State { return state_; }
  auto state(const State state) noexcept -> void { state_ = state; }

  [[nodiscard]] auto diag( ) const noexcept -> Diagnostics& { return diag_; }
  [[nodiscard]] auto Current( ) const noexcept -> const BufferIterator& { return iterator_; }
  auto Next( ) noexcept -> void { ++iterator_; ++column_; }
  [[nodiscard]] auto Ended( ) const noexcept -> bool { return iterator_ == contents_.end( ); }

  /// Marks the start of a token. `SpanFromSaved` then describes the consumed range.
  auto Save( ) noexcept -> void {
    saved_ = iterator_;
    saved_line_ = line_;
    saved_column_ = column_;
  }

  auto Restore( ) noexcept -> void { iterator_ = saved_; }
  [[nodiscard]] auto SpanFromSaved( ) const noexcept -> SourceSpan {
    return SourceSpan{
      .offset = static_cast<u32>(std::distance(contents_.begin( ), saved_)),
      .line = saved_line_,
      .column = saved_column_,
      .length = static_cast<u16>(std::distance(saved_, iterator_)),
    };
  }

  [[nodiscard]] auto Is(const char c) const noexcept -> bool {
    if (Ended( )) { return false; }
    return *iterator_ == c;
  }

  [[nodiscard]] auto IsIdentifierFirstValid( ) const noexcept -> bool {
    if (Ended( )) { return false; }
    return IDENTIFIER_VALID_FIRST[*iterator_];
  }

  [[nodiscard]] auto IsIdentifierValid( ) const noexcept -> bool {
    if (Ended( )) { return false; }
    return IDENTIFIER_VALID[*iterator_];
  }

  [[nodiscard]] auto IsNumberValid( ) const noexcept -> bool {
    if (Ended( )) { return false; }
    return NUMBER_VALID[*iterator_];
  }

  [[nodiscard]] auto IsOperator( ) const noexcept -> bool {
    if (Ended( )) { return false; }
    return OPERATORS[*iterator_];
  }

  [[nodiscard]] auto IsWhitespace( ) const noexcept -> bool {
    if (Ended( )) { return false; }
    return WHITESPACE[*iterator_];
  }

  auto SkipWhitespace( ) noexcept -> void {
    while (!Ended( ) && IsWhitespace( )) {
      if (Is('\n')) {
        IncrementLine( );
      }
      Next( );
    }
  }

  auto IncrementLine( ) noexcept -> void {
    line_++;
    /// `Context::Next` will bump this to one, correctly.
    column_ = 0;
    line_start_ = iterator_ + 1;
  }

  [[nodiscard]] auto GetCurrentLine( ) const noexcept -> std::string_view;
  [[nodiscard]] auto GetSnippet( ) const noexcept -> Diagnostics::Snippet;

private:
  std::string_view contents_;
  std::string_view path_;
  Diagnostics& diag_;
  u16 line_ = 1, column_ = 1;
  u16 saved_line_ = 1, saved_column_ = 1;
  BufferIterator iterator_, saved_, line_start_;
  State state_{ };
};
}

REGISTER_FORMAT_OVERRIDE(pico::Lexer::TokenKind, "{}", [&] {
  using namespace pico;
  switch (self) {
    case Lexer::InstructionOrDirective: return "InstructionOrDirective";
    case Lexer::Identifier: return "Identifier";
    case Lexer::Register: return "Register";
    case Lexer::Number: return "Number";
    case Lexer::String: return "String";
    case Lexer::Flag: return "Flag";
    case Lexer::RegisterBank: return "RegisterBank";
    case Lexer::SingleQuote: return "SingleQuote";
    case Lexer::DoubleQuote: return "DoubleQuote";
    case Lexer::Tilda: return "Tilda";
    case Lexer::BracketLeft: return "BracketLeft";
    case Lexer::BracketRight: return "BracketRight";
    case Lexer::Hashtag: return "Hashtag";
    case Lexer::Comma: return "Comma";
    case Lexer::Colon: return "Colon";
    case Lexer::Semicolon: return "Semicolon";
    case Lexer::Percent: return "Percent";
    case Lexer::DollarSign: return "DollarSign";
    default: return "None";
  }
}())

REGISTER_FORMAT_OVERRIDE(pico::Lexer::InstructionKind, "{}", pico::InstructionName(self))

REGISTER_FORMAT_OVERRIDE(pico::Lexer::FlagKind, "{}", pico::FlagName(self))

REGISTER_FORMAT_OVERRIDE(pico::Lexer::Token,
  "Token {{ Kind: {}, Value: {}, Span: {}:{} }}", self.kind, [&] {
  using namespace pico;
  switch (self.kind) {
    case Lexer::InstructionOrDirective:
      return std::format("{}", self.value_as<Lexer::InstructionKind>());
    case Lexer::Flag:
      return std::format("{}", self.value_as<Lexer::FlagKind>());
    case Lexer::Number:
    case Lexer::Register:
    case Lexer::RegisterBank:
      return std::format("{}", self.value_as<u32>());
    case Lexer::String:
    case Lexer::Identifier:
      return self.value_as<std::string>();
    default:
      return std::string{"Invalid"};
  }
}(), self.span.line, self.span.column)

#endif
