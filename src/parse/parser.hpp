#ifndef PICO_PARSER_HH
#define PICO_PARSER_HH

#include "diagnostics.hpp"
#include "lexer.hpp"
#include "utilities/inline_vector.hpp"

namespace pico
{
  class Parser {
  public:
    enum class InstructionType : u8;
    struct Node;
    class Context;

    Parser() = default;

    void WithTokens(const std::vector<Lexer::Token>& tokens) { tokens_ = tokens; }
    auto Run(Context& context) -> bool;

  private:
    std::span<const Lexer::Token> tokens_;
  };

  class Parser::Context {
  public:
    explicit Context(Diagnostics& diag) : diag_(diag) {}

  private:
    Diagnostics& diag_;
  };

  enum class InstructionType : u8 {
    Invalid,
    NoOperands,
    Register,
    Immediate,
    Dereference,
    Pointer,
    Conditional,
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
    std::array<detail::InlineVector<InstructionType, 2>, static_cast<usize>(Lexer::InstructionKind::Maximum)> map{ };
#define ARRAY_ENTRY(kind, ...) map[static_cast<usize>(Lexer::InstructionKind::kind)] = { __VA_ARGS__ }
    /// ALU / register-assignment instructions: `sX, sY` or `sX, kk`.
    ARRAY_ENTRY(Add,           InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(AddCarry,      InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(Subtract,      InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(SubtractCarry, InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(And,           InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(Or,            InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(Xor,           InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(Compare,       InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(CompareCarry,  InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(Test,          InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(TestCarry,     InstructionType::Register, InstructionType::Immediate);
    ARRAY_ENTRY(Load,          InstructionType::Register, InstructionType::Immediate);
    /// `STAR sX, sY` (PB6, register-to-register only).
    ARRAY_ENTRY(Star,          InstructionType::Register);
    /// `LOAD&RETURN sX, kk` (immediate / string / table name).
    ARRAY_ENTRY(LoadAndReturn, InstructionType::Immediate);
    /// Shift / rotate: single register operand.
    ARRAY_ENTRY(RotateLeft,         InstructionType::Register);
    ARRAY_ENTRY(RotateRight,        InstructionType::Register);
    ARRAY_ENTRY(ShiftLeftWithZero,  InstructionType::Register);
    ARRAY_ENTRY(ShiftLeftWithOne,   InstructionType::Register);
    ARRAY_ENTRY(ShiftLeftWithCarry, InstructionType::Register);
    ARRAY_ENTRY(ShiftLeftArith,     InstructionType::Register);
    ARRAY_ENTRY(ShiftRightWithZero, InstructionType::Register);
    ARRAY_ENTRY(ShiftRightWithOne,  InstructionType::Register);
    ARRAY_ENTRY(ShiftRightWithCarry,InstructionType::Register);
    ARRAY_ENTRY(ShiftRightArith,    InstructionType::Register);
    /// `HWBUILD sX` (single register).
    ARRAY_ENTRY(HardwareBuild, InstructionType::Register);
    /// Memory / IO: `sX, kk` immediate port/address or `sX, (sY)` single-value dereference.
    ARRAY_ENTRY(Input,  InstructionType::Immediate, InstructionType::Dereference);
    ARRAY_ENTRY(Output, InstructionType::Immediate, InstructionType::Dereference);
    ARRAY_ENTRY(Fetch,  InstructionType::Immediate, InstructionType::Dereference);
    ARRAY_ENTRY(Store,  InstructionType::Immediate, InstructionType::Dereference);
    /// `OUTPUTK kk, p` (constant to constant port).
    ARRAY_ENTRY(OutputK, InstructionType::OutputK);
    /// Branching: unconditional address or `C/NC/Z/NZ, address`.
    ARRAY_ENTRY(Jump, InstructionType::Immediate, InstructionType::Conditional);
    ARRAY_ENTRY(Call, InstructionType::Immediate, InstructionType::Conditional);
    /// Indirect branching: `(sX, sY)` register-pair pointer.
    ARRAY_ENTRY(JumpAt, InstructionType::Pointer);
    ARRAY_ENTRY(CallAt, InstructionType::Pointer);
    /// `RETURN` (unconditional) or `RETURN C/NC/Z/NZ`.
    ARRAY_ENTRY(Return, InstructionType::NoOperands, InstructionType::Conditional);
    /// `RETURNI ENABLE` / `RETURNI DISABLE`.
    ARRAY_ENTRY(ReturnInterrupt, InstructionType::ReturnInterrupt);
    /// No operands.
    ARRAY_ENTRY(Nop, InstructionType::NoOperands);
    /// `REGBANK A` / `REGBANK B`.
    ARRAY_ENTRY(RegisterBank, InstructionType::RegisterBank);
    /// Assembler directives (operand shapes validated separately).
    ARRAY_ENTRY(Address,                  InstructionType::Directive);
    ARRAY_ENTRY(Constant,                 InstructionType::Directive);
    ARRAY_ENTRY(NameRegister,             InstructionType::Directive);
    ARRAY_ENTRY(DefaultJump,              InstructionType::Directive);
    ARRAY_ENTRY(InstructionInstantiation, InstructionType::Directive);
    ARRAY_ENTRY(StringDefinition,         InstructionType::Directive);
    ARRAY_ENTRY(TableDefinition,          InstructionType::Directive);
    ARRAY_ENTRY(Enable,                   InstructionType::Directive);
    ARRAY_ENTRY(Disable,                  InstructionType::Directive);
#undef ARRAY_ENTRY
    return map;
  }();

  PICO_INLINE constexpr auto GetInstructionTypesFromKind(Lexer::InstructionKind kind) {
    return INSTRUCTION_KIND_TO_TYPES[static_cast<usize>(kind)];
  }

  constexpr auto IsInstructionCompatibleWith(Lexer::InstructionKind kind, InstructionType type) noexcept -> bool {
    for (const auto t : INSTRUCTION_KIND_TO_TYPES[static_cast<usize>(kind)]) {
      if (t == type) {
        return true;
      }
    }
    return false;
  }
}

#endif
