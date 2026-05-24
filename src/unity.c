#define NOB_IMPLEMENTATION
#include "../our_nob.h"
#undef NOB_IMPLEMENTATION

#define ARENA_IMPLEMENTATION
#include <arena.h>
#undef ARENA_IMPLEMENTATION

#include "lexer.c"
#include "parser.c"
