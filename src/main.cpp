#include <cli/cli.hpp>
#include <parse/lexer.hpp>
#include <parse/diagnostics.hpp>
#include <shared.hpp>

static const pico::Cli::Command lsp_mode("Enables LSP protocol for IDE usage.", 0,
  [ ](auto, pico::Cli::Options &options) {
  options.lsp_mode = true;
  return true;
});

static const pico::Cli::Command dump_tokens("Dumps all tokens.", 0,
  [ ](auto, pico::Cli::Options &options) {
  options.dump_tokens = true;
  return true;
});

static const pico::Cli::Command dump_ast("Dumps all AST nodes.", 0,
  [ ](auto, pico::Cli::Options &options) {
  options.dump_ast = true;
  return true;
});

static const auto cli = [ ] {
  pico::Cli cli{};

  cli.AddCommand("lsp-mode", lsp_mode);
  cli.AddCommand("dump-tokens", dump_tokens);
  cli.AddCommand("dump-ast", dump_ast);

  return cli;
}();

auto main(int argc, char *argv[ ]) -> i32 {
  /// Skip the program path because we don't really need it.
  const std::vector<std::string_view> args(argv + 1, argv + argc);
  if (argc <= 1) {
    cli.Splash( );
    return 0;
  }

  const pico::Cli::Options options = cli.Run(args);
  if (options.file_path.empty( )) {
    std::println("Nothing to do.");
    return 1;
  }

  pico::Diagnostics diag;
  pico::Lexer lexer;
  lexer.SetFilePath(options.file_path);
  if (!lexer.Run(diag)) {
    diag.PrintAll( );
    return 1;
  }

  if (options.dump_tokens) {
    for (const auto& token : lexer.tokens( )) {
      /// Tokens have their own `std::formatter` template overrides.
      std::println("{}", token);
    }
  }

  // Print any warnings.
  diag.PrintAll( );
  return 0;
}
