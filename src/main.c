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

  auto source_file = C4_SOURCE(filename, contents.data);

  struct C4_Lexer lexer = {};
  auto tokens = c4lex_parse_token(&lexer, source_file);
  // for (size_t it_index = 0; it_index < tokens.count; it_index++) {
  //   auto it = tokens.items[it_index];
  //   nob_log(NOB_INFO, ""SV_Fmt"\n", SV_Arg(it));
  //
  // }
}
