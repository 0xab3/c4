#define _POSIX_C_SOURCE 200809L
#include "common.h"
#define NOB_IMPLEMENTATION
#include "../nob.h"
#undef NOB_IMPLEMENTATION
#include "./unity.c"

int main() {
  Nob_String_Builder sb = {};
  auto filename = "./examples/while_loop.c4";
  nob_read_entire_file(filename, &sb);
  auto contents = nob_sb_to_sv(sb);

  auto source_file = CX_SOURCE(filename, contents.data);

  struct CX_Lexer lexer = {};
  auto ok = cxl_lex(&lexer, source_file);
  cxp_parse(&lexer);
}
