#ifndef PICO_PARSER_HH
#define PICO_PARSER_HH

#include "diagnostics.hpp"
#include "lexer.hpp"
#include <utilities/inline_vector.hpp>
#include <utilities/formatting.hpp>

namespace pico
{
  class Parser {
  public:
    enum class NodeType : u8;
    struct Node;
    class Context;

    Parser() = default;

    [[nodiscard]] auto nodes() const noexcept -> const std::vector<Node>& { return nodes_; }
    [[nodiscard]] auto labels() const noexcept -> const std::vector<Node>& { return labels_; }
    [[nodiscard]] auto address() const noexcept -> u16 { return address_; }

    auto AddInstruction(Node&& node) -> void;
    auto AddLabel(Node&& node) -> bool;

    auto Run(Diagnostics& diag, Lexer& lexer) -> bool;
    static auto ParseNoOperands(Context& ctx) -> std::optional<Node>;

  private:
    std::vector<Node> nodes_{ };
    std::vector<Node> labels_{ };
    u16 address_ = 0;
  };

  class Parser::Context {
  public:
    explicit Context(Diagnostics& diag, Lexer& lexer)
      : diag_(diag), tokens_(lexer.tokens( )) {}

    [[nodiscard]] auto diag() const noexcept -> Diagnostics& { return diag_; }

    [[nodiscard]] PICO_INLINE auto Peek() const -> const Lexer::Token& { return tokens_[current_]; }
    PICO_INLINE auto Consume() -> void { current_++; }
    PICO_INLINE auto Expect(Lexer::TokenKind kind) -> bool {
      if (Peek().kind == kind) {
        Consume();
        return true;
      }
      return false;
    }
    auto Save() -> void { saved_ = current_; }
    auto Restore() -> void { current_ = saved_; }

  private:
    usize current_ = 0, saved_ = 0;
    Diagnostics& diag_;
    std::span<const Lexer::Token> tokens_;
  };

  enum class Parser::NodeType : u8 {
    Invalid,
    Label,
    NoOperands,
    Register,
    Immediate,
    Dereference,
    Pointer,
    Conditional,
    ConditionalJump,
    OutputK,
    ReturnInterrupt,
    RegisterBank,
    Directive
  };

  /// A fast look-up table for instruction types. Since each instruction in Picoblaze can assume one or more shapes,
  /// given the operands that they're instantiated with (register, literals, pointers...), any instruction maps
  /// to a list of possible instruction types. The list is arranged from most common to the least common types whenever
  /// possible, so lookup is O(1) in most cases.
  static constexpr auto INSTRUCTION_KIND_TO_TYPES = [] consteval {
    std::array<detail::InlineVector<Parser::NodeType, 2>, static_cast<usize>(Lexer::InstructionKind::Maximum)> map{ };
#define ARRAY_ENTRY(kind, ...) map[static_cast<usize>(Lexer::InstructionKind::kind)] = { __VA_ARGS__ }
    /// ALU / register-assignment instructions: `sX, sY` or `sX, kk`.
    ARRAY_ENTRY(Add,           Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(AddCarry,      Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(Subtract,      Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(SubtractCarry, Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(And,           Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(Or,            Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(Xor,           Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(Compare,       Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(CompareCarry,  Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(Test,          Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(TestCarry,     Parser::NodeType::Register, Parser::NodeType::Immediate);
    ARRAY_ENTRY(Load,          Parser::NodeType::Register, Parser::NodeType::Immediate);
    /// `STAR sX, sY` (PB6, register-to-register only).
    ARRAY_ENTRY(Star,          Parser::NodeType::Register);
    /// `LOAD&RETURN sX, kk` (immediate / string / table name).
    ARRAY_ENTRY(LoadAndReturn, Parser::NodeType::Immediate);
    /// Shift / rotate: single register operand.
    ARRAY_ENTRY(RotateLeft,          Parser::NodeType::Register);
    ARRAY_ENTRY(RotateRight,         Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftLeftWithZero,   Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftLeftWithOne,    Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftLeftWithCarry,  Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftLeftArith,      Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftRightWithZero,  Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftRightWithOne,   Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftRightWithCarry, Parser::NodeType::Register);
    ARRAY_ENTRY(ShiftRightArith,     Parser::NodeType::Register);
    /// `HWBUILD sX` (single register).
    ARRAY_ENTRY(HardwareBuild, Parser::NodeType::Register);
    /// Memory / IO: `sX, kk` immediate port/address or `sX, (sY)` single-value dereference.
    ARRAY_ENTRY(Input,  Parser::NodeType::Immediate, Parser::NodeType::Dereference);
    ARRAY_ENTRY(Output, Parser::NodeType::Immediate, Parser::NodeType::Dereference);
    ARRAY_ENTRY(Fetch,  Parser::NodeType::Immediate, Parser::NodeType::Dereference);
    ARRAY_ENTRY(Store,  Parser::NodeType::Immediate, Parser::NodeType::Dereference);
    /// `OUTPUTK kk, p` (constant to constant port).
    ARRAY_ENTRY(OutputK, Parser::NodeType::OutputK);
    /// Branching: unconditional address or `C/NC/Z/NZ, address`.
    ARRAY_ENTRY(Jump, Parser::NodeType::Immediate, Parser::NodeType::ConditionalJump);
    ARRAY_ENTRY(Call, Parser::NodeType::Immediate, Parser::NodeType::ConditionalJump);
    /// Indirect branching: `(sX, sY)` register-pair pointer.
    ARRAY_ENTRY(JumpAt, Parser::NodeType::Pointer);
    ARRAY_ENTRY(CallAt, Parser::NodeType::Pointer);
    /// `RETURN` (unconditional) or `RETURN C/NC/Z/NZ`.
    ARRAY_ENTRY(Return, Parser::NodeType::NoOperands, Parser::NodeType::Conditional);
    /// `RETURNI ENABLE` / `RETURNI DISABLE`.
    ARRAY_ENTRY(ReturnInterrupt, Parser::NodeType::ReturnInterrupt);
    /// No operands.
    ARRAY_ENTRY(Nop, Parser::NodeType::NoOperands);
    /// `REGBANK A` / `REGBANK B`.
    ARRAY_ENTRY(RegisterBank, Parser::NodeType::RegisterBank);
    /// Assembler directives (operand shapes validated separately).
    ARRAY_ENTRY(Address,                  Parser::NodeType::Directive);
    ARRAY_ENTRY(Constant,                 Parser::NodeType::Directive);
    ARRAY_ENTRY(NameRegister,             Parser::NodeType::Directive);
    ARRAY_ENTRY(DefaultJump,              Parser::NodeType::Directive);
    ARRAY_ENTRY(InstructionInstantiation, Parser::NodeType::Directive);
    ARRAY_ENTRY(StringDefinition,         Parser::NodeType::Directive);
    ARRAY_ENTRY(TableDefinition,          Parser::NodeType::Directive);
    ARRAY_ENTRY(Enable,                   Parser::NodeType::Directive);
    ARRAY_ENTRY(Disable,                  Parser::NodeType::Directive);
#undef ARRAY_ENTRY
    return map;
  }();

  PICO_INLINE constexpr auto GetInstructionTypesFromKind(Lexer::InstructionKind kind) {
    return INSTRUCTION_KIND_TO_TYPES[static_cast<usize>(kind)];
  }

  constexpr auto IsInstructionCompatibleWith(Lexer::InstructionKind kind,
    Parser::NodeType type) noexcept -> bool {
    for (const auto t : INSTRUCTION_KIND_TO_TYPES[static_cast<usize>(kind)]) {
      if (t == type) {
        return true;
      }
    }
    return false;
  }

  struct Parser::Node {
    Lexer::InstructionKind kind = Lexer::InstructionKind::Nop;
    std::optional<Lexer::TokenValue> first, second;
    u16 address{ };
  };
}

REGISTER_FORMAT_OVERRIDE(pico::Parser::Node, "Node {{ Type: {} }}", [&] {
  return "a";
}());

#endif
