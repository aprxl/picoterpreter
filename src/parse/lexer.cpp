#include "lexer.hpp"

#include <filesystem>
#include <fstream>

namespace pico
{
namespace
{
/// Converts a single hexadecimal digit to its value, or `nullopt` if `c` is not
/// a hex digit.
constexpr auto CharToHexU8(const char c) noexcept -> std::optional<u8> {
  if (c >= '0' && c <= '9') {
    return static_cast<u8>(c - '0');
  }
  const char lower = (c >= 'A' && c <= 'F') ? static_cast<char>(c + 32_u8) : c;
  if (lower >= 'a' && lower <= 'f') {
    return static_cast<u8>(lower - 'a' + 10);
  }
  return std::nullopt;
}

auto StringToLower(const std::string_view str) -> std::string {
  std::string result{ };
  result.reserve(str.size( ));
  for (const char c : str) {
    result += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32_u8) : c;
  }
  return result;
}

/// Parses a digit string in the given base. Reports malformed digits / overflow
/// of the per-base digit count into `diag` and returns `nullopt` on hard errors.
auto StringToNumber(const std::string_view str,
  const Lexer::NumberLiteralBase base, Diagnostics& diag) -> std::optional<u32> {
  usize max_digits{ };
  u32 multiplier{ };
  std::string_view base_name;
  switch (base) {
    case Lexer::NumberLiteralBase::Decimal:
      max_digits = 3; multiplier = 10_u32; base_name = "Decimal";     break;
    case Lexer::NumberLiteralBase::Binary:
      max_digits = 8; multiplier = 2_u32;  base_name = "Binary";      break;
    case Lexer::NumberLiteralBase::Hexadecimal:
      max_digits = 3; multiplier = 16_u32; base_name = "Hexadecimal"; break;
  }

  usize size = str.size( );
  if (size > max_digits) {
    diag.AddMessage(Diagnostics::Severity::Warning,
      "{} numbers may only have {} digits, got {}.", base_name, max_digits, size);
    size = max_digits;
  }

  u32 result{ };
  u32 power = 1;
  /// Iterate the number from least to most-significant.
  for (usize i = size; i-- > 0; ) {
    const auto value = CharToHexU8(str[i]);
    if (!value) {
      diag.AddMessage(Diagnostics::Severity::Error,
        "Character '{}' is not a valid digit.", str[i]);
      return std::nullopt;
    }
    if (base == Lexer::NumberLiteralBase::Binary && *value > 1) {
      diag.AddMessage(Diagnostics::Severity::Error,
        "Binary numbers may only contain 0 and 1 digits, got '{}'.", *value);
      return std::nullopt;
    }
    if (base == Lexer::NumberLiteralBase::Decimal && *value > 9) {
      diag.AddMessage(Diagnostics::Severity::Error,
        "Decimal numbers may only contain 0...9 digits, got '{}'.", str[i]);
      return std::nullopt;
    }
    result += *value * power;
    power *= multiplier;
  }
  return result;
}

constexpr usize IDENTIFIER_MIN_CAPACITY = 8_usize;
constexpr usize NUMBER_MIN_CAPACITY = 3_usize;
} // namespace

auto Lexer::TryIdentifier(Context& ctx) -> std::optional<Token> {
  Token token{ };
  token.kind = Identifier;
  if (!ctx.IsIdentifierFirstValid( )) {
    return { };
  }

  bool has_only_hex_characters = true;
  std::string identifier{ };
  identifier.reserve(IDENTIFIER_MIN_CAPACITY);
  while (ctx.IsIdentifierValid( )) {
    /// Make sure that our identifier isn't a valid hexadecimal number.
    if (!ctx.IsNumberValid( ))
      has_only_hex_characters = false;
    identifier += *ctx.Current( );
    ctx.Next( );
  }

  const bool looks_like_hex_literal =
    has_only_hex_characters && identifier.size( ) > 1 && identifier.size( ) <= 3;
  token.value = std::move(identifier);
  /// Instead of writing different match predicates for registers and instructions,
  /// we just parse any identifier then later try to narrow it down into those two.
  /// If `Lexer::TryNarrowIdentifier` matches against a register or instruction,
  /// it'll assign the correct token kind to `token` and change the value accordingly.
  /// If it fails, it'll return `false`, so we error out by setting the token type to `Invalid`.
  if (!TryNarrowIdentifier(ctx, token)) {
    token.kind = Invalid;
    return std::make_optional(std::move(token));
  }

  /// If it stayed a plain identifier yet is composed solely of hex digits (e.g. `ab`),
  /// it's really a number: defer to `TryNumber`. Keywords that happen to be hex
  /// (e.g. `add`) have already been narrowed above, so they are kept as-is.
  if (token.kind == Identifier && looks_like_hex_literal) {
    return { };
  }
  return std::make_optional(std::move(token));
}

auto Lexer::TryNarrowIdentifier(Context& ctx, Token& token) -> bool {
  if (token.kind != Identifier) {
    return false;
  }

  const auto& ident = token.value_as<std::string>( );
  const std::string lower = StringToLower(ident);
  /// Each helper only mutates `token` when it matches, so short-circuiting keeps
  /// the first match and avoids the later helpers clobbering it.
  bool narrowed = TryNarrowIntoInstruction(lower, token)
               || TryNarrowIntoRegister(lower, token)
               || TryNarrowIntoFlag(lower, token);

  /// A `RegisterBank` token is a single character (`a`/`b`), and cannot follow a
  /// single quote. Checking the length keeps instructions like `add` from
  /// being narrowed into register bank A.
  if (!narrowed && ctx.state( ) != Context::State::RightAfterSingleQuote && ident.size( ) == 1) {
    narrowed = TryNarrowIntoRegisterBank(ident.at(0), token);
  }

  ctx.state(Context::State::Default);
  /// If it wasn't narrowed, then we still have an identifier. So let's make sure
  /// that it is a valid one.
  if (!narrowed && (ident.find('*') != std::string::npos
                 || ident.find('&') != std::string::npos
                 || ident.find('@') != std::string::npos)) {
    ctx.diag( ).AddMessage(Diagnostics::Severity::Error,
      "Identifier '{}' contains characters reserved for instructions.", ident);
    return false;
  }
  return true;
}

auto Lexer::TryNarrowIntoFlag(const std::string_view lower, Token& token) -> bool {
  if (const auto kind = FlagFromText(lower)) {
    token.kind = Flag;
    token.value = *kind;
    return true;
  }
  return false;
}

auto Lexer::TryNarrowIntoRegister(const std::string_view lower, Token& token) -> bool {
  /// Registers are `s0`..`sf`: the letter `s` followed by a single hex nibble.
  if (lower.size( ) == 2 && lower[0] == 's') {
    if (const auto nibble = CharToHexU8(lower[1])) {
      token.kind = Register;
      token.value = static_cast<u32>(*nibble);
      return true;
    }
  }
  return false;
}

auto Lexer::TryNarrowIntoInstruction(const std::string_view lower, Token& token) -> bool {
  if (const auto kind = InstructionFromMnemonic(lower)) {
    token.kind = InstructionOrDirective;
    token.value = *kind;
    return true;
  }
  return false;
}

auto Lexer::TryNumber(Context& ctx) -> std::optional<Token> {
  if (!ctx.IsNumberValid( )) {
    return { };
  }

  std::string number{ };
  number.reserve(NUMBER_MIN_CAPACITY);
  while (ctx.IsNumberValid( )) {
    number += *ctx.Current( );
    ctx.Next( );
  }

  Token token;
  token.kind = Number;
  /// Because of the way base specifies work in Picoblaze, we can't convert this number
  /// into an actual number just yet. We instead do a second pass over all found tokens and,
  /// in that pass, we compute the desired value.
  token.value = std::move(number);
  return std::make_optional(std::move(token));
}

auto Lexer::TryNarrowIntoRegisterBank(const char c, Token& token) -> bool {
  if (c == 'A' || c == 'a') {
    token.kind = RegisterBank;
    token.value = 0_u32;
    return true;
  }

  if (c == 'B' || c == 'b') {
    token.kind = RegisterBank;
    token.value = 1_u32;
    return true;
  }
  return false;
}

auto Lexer::TryOperator(Context& ctx) -> std::optional<Token> {
  if (!ctx.IsOperator( )) {
    return { };
  }

  Token token;
  switch (*ctx.Current( )) {
    case '\'':
      token.kind = SingleQuote;
      /// Disambiguate the lexing of `'b` so `b` doesn't get parsed as a `RegisterBank`
      ctx.state(Context::State::RightAfterSingleQuote);
      break;
    case '\"': token.kind = DoubleQuote; break;
    case '~': token.kind = Tilda; break;
    case '[': token.kind = BracketLeft; break;
    case ']': token.kind = BracketRight; break;
    case '#': token.kind = Hashtag; break;
    case ',': token.kind = Comma; break;
    case ':': token.kind = Colon; break;
    case ';': token.kind = Semicolon; break;
    case '%': token.kind = Percent; break;
    case '$': token.kind = DollarSign; break;
    default: token.kind = Invalid;
  }

  ctx.Next( );
  return std::make_optional(std::move(token));
}

auto Lexer::AddToken(Token&& token) -> bool {
  const auto kind = token.kind;
  tokens_.emplace_back(std::move(token));
  /// A returned `Invalid` acts as a sentinel for badly lexed tokens.
  return kind != Invalid;
}

auto Lexer::Resolve(Diagnostics& diag) -> bool {
  for (usize i = 0; i < tokens_.size(); ++i) {
    if (tokens_.at(i).kind == Number) {
      if (!TryResolveNumber(i, diag)) {
        return false;
      }
    }
  }
  return true;
}

auto Lexer::TryResolveNumber(const usize index, Diagnostics& diag) -> bool {
  auto base = NumberLiteralBase::Hexadecimal;
  /// Check if have a base specifier after the number. If so, update accordingly.
  if (tokens_.size( ) > index + 2
      && tokens_.at(index + 1).kind == SingleQuote
      && tokens_.at(index + 2).kind == Identifier) {
    const auto& base_specifier =
      tokens_.at(index + 2).value_as<std::string>( );
    if (base_specifier.size( ) > 1) {
      diag.AddMessage(Diagnostics::Severity::Error,
        "Invalid base specifier '{}'.", base_specifier);
      return false;
    }

    switch (base_specifier.at(0)) {
      case 'd': base = NumberLiteralBase::Decimal; break;
      case 'b': base = NumberLiteralBase::Binary; break;
      default:
        diag.AddMessage(Diagnostics::Severity::Error,
          "Unknown base specifier '{}', expected 'd' or 'b'.", base_specifier);
        return false;
    }

    /// Remove both the single quote and the base specifier.
    tokens_.erase(tokens_.begin( ) + static_cast<ptrdiff_t>(index) + 1,
      tokens_.begin( ) + static_cast<ptrdiff_t>(index) + 3);
  }

  auto& token = tokens_.at(index);
  /// Copy the text out before we overwrite `token.value` with the parsed result;
  /// the string and the `u32` share the same variant storage.
  const std::string text = token.value_as<std::string>( );
  const auto parsed = StringToNumber(text, base, diag);
  if (!parsed) {
    diag.AddMessage(Diagnostics::Severity::Error,
      "Unable to parse number token '{}'.", text);
    return false;
  }

  u32 value = *parsed;
  /// The maximum numeric literal supported in Picoblaze is 0x3FF.
  if (value > 0x3ff_u32) {
    diag.AddMessage(Diagnostics::Severity::Warning,
      "Number literal '{}' exceeds limit of 3FF, truncating...", text);
    value = 0x3ff_u32;
  }

  token.value.emplace<u32>(value);
  return true;
}

auto Lexer::Run(Diagnostics& diag) -> bool {
  const std::filesystem::path path(path_);
  std::ifstream file(path);
  if (!file.is_open( )) {
    diag.AddMessage(Diagnostics::Severity::Error, "Could not open file '{}'.", path_);
    return false;
  }

  const std::string contents(std::istreambuf_iterator(file), {});
  return Tokenize(contents, diag);
}

auto Lexer::Tokenize(const std::string_view source, Diagnostics& diag) -> bool {
  static constexpr std::array<std::optional<Token>(*)(Context&), 3> MATCH_FN = {
    TryIdentifier,
    TryNumber,
    TryOperator
  };

  Context ctx(source, path_, diag);
  /// In case we are reusing this Lexer instance.
  tokens_.clear( );
  while (!ctx.Ended( )) {
    ctx.SkipWhitespace( );
    /// Make sure that `Context::SkipWhitespace` didn't leave us as the end of the file.
    if (ctx.Ended( )) {
      break;
    }

    ctx.Save( );
    bool found{ };
    for (const auto& fn : MATCH_FN) {
      if (auto token = fn(ctx); token.has_value( )) {
        token->span = ctx.SpanFromSaved( );
        found = AddToken(std::move(*token));
        break;
      }
      ctx.Restore( );
    }

    if (!found) {
      if (!diag.DoesLastHaveSnippet( )) {
        diag.AddSnippetToLastMessage(ctx.GetSnippet( ));
      }
      diag.AddMessage(Diagnostics::Severity::Error, "Lexer failed.");
      return false;
    }
  }
  /// Resolve some token pairs such as:
  /// 123 ' d -> decimal number
  /// 00010111 ' b -> binary number
  /// [ 44, 23, 00 ]'d -> table of decimal numbers
  ///
  /// This method directly changes the tokens lexed thus far and also removes any tokens that are meaningless
  /// to the parser.
  if (!Resolve(diag)) {
    return false;
  }
  return true;
}

auto Lexer::Context::GetCurrentLine( ) const noexcept -> std::string_view {
  const auto start = static_cast<usize>(std::distance(contents_.begin( ), line_start_));
  usize end = contents_.find('\n', start);
  if (end == std::string_view::npos) {
    end = contents_.size( );
  }
  return contents_.substr(start, end - start);
}

auto Lexer::Context::GetSnippet( ) const noexcept -> Diagnostics::Snippet {
  Diagnostics::Snippet snippet;
  snippet.line = line_;
  snippet.column = column_;
  snippet.file_name = path_;
  snippet.text = GetCurrentLine( );
  return snippet;
}
}
