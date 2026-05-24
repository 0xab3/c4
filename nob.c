#define NOB_IMPLEMENTATION
#include "./nob.h"
#define SRC_FOLDER "src/"
#define BUILD_FOLDER "build/"
int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);
  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "clang");
  nob_cmd_append(&cmd, "-std=gnu23");
  nob_cmd_append(&cmd, "-I./vendor/arena");
  nob_cmd_append(&cmd, "-Wconversion");
  nob_cmd_append(&cmd, "-Wall");
  nob_cmd_append(&cmd, "-Wextra");

  nob_cmd_append(&cmd, "-ggdb");
  nob_cmd_append(&cmd, SRC_FOLDER "main.c");

  nob_mkdir_if_not_exists(BUILD_FOLDER);

  nob_cmd_append(&cmd, "-o", BUILD_FOLDER "boom");
  if (!nob_cmd_run(&cmd, 0)) {
    nob_log(NOB_ERROR, "failed to build 3d!");
    return 0;
  }

  char const *_ = nob_shift_args(&argc, &argv);
  char const *arg = nob_shift_args(&argc, &argv);

  if (nob_sv_eq(nob_sv_from_cstr(arg), nob_sv_from_cstr("run"))) {
    nob_cmd_append(&cmd, "./build/boom");
    nob_cmd_run(&cmd, 0);
  }
}
