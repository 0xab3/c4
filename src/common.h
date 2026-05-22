#pragma once

struct C4_SourceFile {
  char const* name;
  char const* contents;
};
#define C4_SOURCE(filename, contents) (struct C4_SourceFile){filename, contents}

#define C4_CAST(type, value) ((type)(value))
#define C4_UNREACHABLE(...)                                                      \
  do {                                                                         \
    fprintf(stderr, "UNREACHABLE: %s:%d: ", __FILE__, __LINE__);               \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    abort();                                                                   \
  } while (0)
