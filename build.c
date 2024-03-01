#define BUILD_IMPLEMENTATION
#include "build.h"

LogSeverity g_log_severity = LOG_ALL;

int main(int argc, char **argv) {
  PLEASE_REBUILD_YOURSELF(argc, argv, "-g -std=c99");

  bool ok = true;

  CompileCmd cmd = {0};
  cmd.compiler = COMPILER_C_ANY;
  cmd.target_name = "ping_pong";
  cmd.build_dir = "build";
  cmd.cache_modules = false;
  cmd.cflags = "-g -Wall -pedantic -std=c99 -I./raylib/src/";
  cmd.link_with = "-L./raylib/src/ -lraylib -lm";

  vec_push(cmd.modules, "src/main");
  vec_push(cmd.modules, "src/network");

  if (!file_exist("raylib/src/libraylib.a")) {
    vec_push(cmd.git_dependencies, ((GitDependency){
      .repository = "https://github.com/raysan5/raylib.git",
      .dest = "raylib",
      .post_cmd = "cd raylib/src/ && make PLATFORM=PLATFORM_DESKTOP"
    }));
  }
  
  // tools required by raylib
  // ok = 0 == run_str_cmd_sync("sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev -y");

  if (ok) {
    ok = cmd_run_sync(&cmd);
  }

  char *prog = shift_args(&argc, &argv);
  char *sub_cmd = shift_args(&argc, &argv);

  if (ok && NULL != sub_cmd && 0 == strncmp(sub_cmd, "run", 3)) {
    StringBuilder args_sb = {0};
    string_builder_append_cstr(&args_sb, "./");
    string_builder_append_cstr(&args_sb, cmd.target_name);
    string_builder_append_rune(&args_sb, ' ');

    while (argc > 0) {
      char *arg = shift_args(&argc, &argv);
      string_builder_append_cstr(&args_sb, arg);
      string_builder_append_rune(&args_sb, ' ');
    }

    ok = 0 == run_str_cmd_sync(string_builder_build(&args_sb));

    string_builder_free(args_sb);
  }

  cmd_free(&cmd);

  return !ok;
}
