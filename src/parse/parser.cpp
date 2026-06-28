#include "parser.hpp"

namespace pico
{
  auto Parser::Run(Diagnostics& diag, Lexer& lexer) -> bool {
    if (lexer.tokens( ).empty( )) {
      diag.AddMessage(Diagnostics::Severity::Information, "No tokens were found.");
      return true;
    }

    Context ctx(diag, lexer);
    for (const auto& token : lexer.tokens( )) {
      if (token.kind == Lexer::InstructionOrDirective) {
        const auto& possible_instruction_types
          = GetInstructionTypesFromKind(token.value_as<Lexer::InstructionKind>( ));

        std::optional<Node> node;

        for (const auto& type : possible_instruction_types) {
          switch (type) {
            case NodeType::NoOperands: node = ParseNoOperands(ctx); break;
            default: __builtin_trap(); break;
          }
        }

        if (!node.has_value()) {
          return false;
        }

        nodes_.push_back(*node);
      }
      else {
        switch (token.kind) {
          case Lexer::Identifier: { PICO_TODO("Something else"); break; }
          default: diag.AddMessage(Diagnostics::Severity::Error, "Unexpected token."); return false;
        }
      }
    }
    return true;
  }

  auto Parser::ParseNoOperands(Context& ctx) -> std::optional<Node> {
    const auto& instr = ctx.Peek( );
    Node node;
    node.kind = instr.value_as<Lexer::InstructionKind>( );
    ctx.Consume( );
    return std::make_optional(node);
  }
}
