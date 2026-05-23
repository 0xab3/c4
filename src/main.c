#define _POSIX_C_SOURCE 200809L
#include "common.h"
#define NOB_IMPLEMENTATION
#include "../nob.h"
#undef NOB_IMPLEMENTATION
#include "./unity.c"

int main() {
  Nob_String_Builder sb = {};
  char const* filename = "./examples/while_loop.c4";
  nob_read_entire_file(filename, &sb);
  Nob_String_View contents = nob_sb_to_sv(sb);

  struct CX_SourceFile source_file = CX_SOURCE(filename, contents.data);

  struct CX_Lexer lexer = {};
  bool ok = cxl_lex(&lexer, source_file);
  cxp_parse(&lexer);
}
