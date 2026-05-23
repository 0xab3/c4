#pragma once

#include <stddef.h>
#include <stdint.h>
struct CX_SourceFile {
  char const *name;
  char const *contents;
};

struct CX_DArrayGeneric {
  size_t count;
  size_t capacity;
  uint8_t items[];
};

struct CX_DArrayMeta {
  size_t count;
  size_t capacity;
};

#define CX_Array(T) T *

#define cx_da_foreach(it, da)                                                          \
  for (typeof(da->items) it = (da)->items; it < (da)->items + (da)->count; ++it)
#define cx_da_meta(da) ((struct CX_DArrayMeta *)((struct CX_DArrayGeneric *)(da) - 1))
#define cx_da_count(da) (cx_da_meta(da))->count;
#define cx_da_capacity(da) (cx_da_meta(da))->capacity;

#ifndef CX_ASSERT
#include <assert.h>
#define CX_ASSERT assert
#endif /* CX_ASSERT */

#ifndef CX_REALLOC
#include <stdlib.h>
#define CX_REALLOC realloc
#endif /* CX_REALLOC */

#ifndef CX_FREE
#include <stdlib.h>
#define CX_FREE free
#endif /* CX_FREE */

#ifndef CX_DA_INIT_CAP
#define CX_DA_INIT_CAP 256
#endif

#define cx_da_reserve(da, expected_capacity, elem_size)                                \
  do {                                                                                 \
    if ((expected_capacity) > (da)->capacity) {                                        \
      if ((da)->capacity == 0) {                                                       \
        (da)->capacity = CX_DA_INIT_CAP;                                               \
      }                                                                                \
      while ((expected_capacity) > (da)->capacity) {                                   \
        (da)->capacity *= 2;                                                           \
      }                                                                                \
      (da) = CX_REALLOC((da),                                                          \
                        sizeof(struct CX_DArrayGeneric) + (da)->capacity * elem_size); \
      CX_ASSERT((da) != NULL && "Buy more RAM lol");                                   \
    }                                                                                  \
  } while (0)

#define cx_da_init(da)                                                                 \
  do {                                                                                 \
    (da) = CX_REALLOC((da),                                                            \
                      sizeof(struct CX_DArrayGeneric) + CX_DA_INIT_CAP * sizeof(*da)); \
    CX_ASSERT((da) != NULL && "Buy more RAM lol");                                     \
    struct CX_DArrayGeneric *generic = (struct CX_DArrayGeneric *)da;                  \
    generic->capacity = CX_DA_INIT_CAP;                                                \
    generic->count = 0;                                                                \
    (da) = (typeof((da)))(generic + 1);                                                \
  } while (0)

#define cx_da_append(da, item)                                                         \
  do {                                                                                 \
    if ((da) == NULL) {                                                                \
      cx_da_init(da);                                                                  \
      struct CX_DArrayGeneric *generic = (struct CX_DArrayGeneric *)(da) - 1;          \
      (da)[(generic)->count++] = (item);                                               \
    } else {                                                                           \
      struct CX_DArrayGeneric *as_generic = (struct CX_DArrayGeneric *)(da) - 1;       \
      cx_da_reserve(as_generic, (as_generic)->count + 1, sizeof(*da));                 \
      (da) = (typeof(da))(as_generic->items);                                          \
      (da)[(as_generic)->count++] = (item);                                            \
    }                                                                                  \
  } while (0)

#define cx_da_free(da) CX_FREE((da))
#define cx_da_last(da) (da)->items[(CX_ASSERT((da)->count > 0), (da)->count - 1)]

#define CX_SOURCE(filename, contents)                                                  \
  (struct CX_SourceFile) { filename, contents }

#define CX_CAST(type, value) ((type)(value))
#define CX_UNREACHABLE(...)                                                            \
  do {                                                                                 \
    fprintf(stderr, "UNREACHABLE: %s:%d: ", __FILE__, __LINE__);                       \
    fprintf(stderr, __VA_ARGS__);                                                      \
    fprintf(stderr, "\n");                                                             \
    abort();                                                                           \
  } while (0)
