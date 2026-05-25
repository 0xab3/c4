#define _POSIX_C_SOURCE 200809L
#include "./unity.c"

int main() {
  Nob_String_Builder sb = {0};
  char const *filename = "./examples/pointers.c4";
  nob_read_entire_file(filename, &sb);
  Nob_String_View contents = nob_sb_to_sv(sb);

  struct CX_SourceFile source_file = CX_SOURCE(filename, contents);

  struct CX_Lexer lexer = {0};
  cxl_lexer_init(&lexer, source_file);
  bool ok = cxl_lex(&lexer);
  assert(ok);

  CX_Parser parser = {0};
  cxp_parser_init(&parser, lexer);
  CX_Module *mod = cxp_parse(&parser);
  NOB_UNUSED(mod);
}
