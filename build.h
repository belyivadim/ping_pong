#ifndef __BUILD_H__
#define __BUILD_H__

#include <stddef.h>
#include <stdbool.h>

// ------------------------------------ | Allocator |
static void *a_allocate(size_t bytes);
static void *a_reallocate(void *ptr, size_t old_size, size_t new_size);
static void *a_callocate(size_t nmemb, size_t memb_size);
static void a_free(void *ptr);


// ------------------------------------ | StringView |
typedef struct {
  const char *p_begin;
  size_t length;
} StringView;

#define string_view_empty ((StringView){0})
#define string_view_from_cstr(p_str) ((StringView){.p_begin = p_str, .length = strlen(p_str)})
#define string_view_from_cstr_slice(p_str, offset, len) ((StringView){.p_begin = (p_str) + (offset), .length = len})
#define string_view_from_constant(c) ((StringView){.p_begin = (c), .length = sizeof((c)) - 1})

#define string_view_is_empty(sv) (0 == (sv).length)

#define string_view_farg "%.*s"
#define string_view_expand(sv) (int)(sv).length, (sv).p_begin

#define string_view_slice(sv, offset, len) (string_view_from_cstr_slice((sv).p_begin, (offset), (len)))

static bool string_view_equals(const StringView *p_lhs, const StringView *p_rhs);
static bool string_view_equals_cstr(const StringView *p_sv, const char *p_str);
static bool string_view_starts_with_cstr(const StringView *p_sv, const char *p_start);
static bool string_view_ends_with_cstr(const StringView *p_sv, const char *p_end);

static size_t string_view_index_of(const StringView *p_sv, int rune);
static size_t string_view_last_index_of(const StringView *p_sv, int rune);

static StringView string_view_chop_by_delim(StringView *p_sv, int delim);
static StringView string_view_chop_by_sv(StringView *p_sv, const StringView *p_delim);



// ------------------------------------ | vec |
typedef struct {
  size_t count;
  size_t capacity;
} VecHeader;

#define VEC_GROW_FACTOR 2
#define VEC_INITIAL_CAPACITY 8

#define vec(T) T*

#define vec_get_header(v) (((VecHeader*)(v)) - 1)

#define vec_alloc(v) vec_alloc_reserved((v), VEC_INITIAL_CAPACITY)

#define vec_alloc_reserved(v, cap) \
  do {\
    size_t capacity = (cap);\
    assert(capacity > 0 && "Capacity should be greater than 0.");\
    (v) = a_allocate(sizeof(VecHeader) + capacity * sizeof(*(v))); \
    VecHeader *header = (VecHeader*)(v);\
    header->count = 0;\
    header->capacity = capacity;\
    (v) = (void*)((char*)v + sizeof(VecHeader));\
  } while (0)

#define vec_free(v) do { if (NULL != (v)) a_free(vec_get_header((v))); } while (0)

#define vec_count(v) (vec_get_header((v))->count)
#define vec_capacity(v) (vec_get_header((v))->capacity)
#define vec_is_empty(v) (0 == vec_count((v)))

#define vec_maybe_expand(v) \
  do {\
    if (vec_count(v) == vec_capacity(v)) {\
      size_t old_cap = vec_capacity(v);\
      vec_capacity(v) *= VEC_GROW_FACTOR;\
      void *tmp = a_reallocate(vec_get_header(v), old_cap * sizeof(*(v)), sizeof(VecHeader) + vec_capacity(v) * sizeof(*(v)));\
      if (NULL == tmp) log_fatal("realocation for vector with new capacity %lu failed!", 137, vec_capacity(v)); \
      v = (void*)((char*)tmp + sizeof(VecHeader));\
    }\
  } while(0)

static void vec_mb_expand(void **pp_vec, size_t el_size);
static void vec_expand(void **pp_vec, size_t el_size);

#define vec_push(v, e) \
  do {\
    vec_mb_expand((void**)&(v), sizeof(*(v)));\
    (v)[vec_count((v))++] = (e);\
  } while (0)

#define vec_pop(v) (--vec_count((v)))

#define vec_back(v) (assert(0 != vec_count((v))), v + vec_count((v)) - 1)

#define vec_at(v, i) ((v) + (i))

#define vec_clean(v) (vec_count(v) = 0)

#define vec_for_each(v, iter, body)\
  do {\
    size_t count = vec_count(v);\
    for (size_t iter = 0; iter < count; ++iter) body;\
  } while (0)

#define vec_append(v1, v2)\
  do {\
    size_t count = vec_count((v2));\
    for (size_t i = 0; i < count; ++i) vec_push((v1), (v2)[i]);\
  } while (0)


// ------------------------------------ | StringBuilder |
typedef struct {
  vec(char) data;
} StringBuilder;

#define string_builder_init(sb) do { vec_alloc(sb.data); vec_push(sb.data, '\0'); } while (0)
#define string_builder_init_with_capacity(sb, cap) vec_alloc_reserved(sb.data, (cap))
#define string_builder_free(sb) vec_free((sb).data)

#define string_builder_copy(dest, src)\
  do {\
    size_t src_len = string_builder_get_length((src));\
    string_builder_init_with_capacity((dest), src_len);\
    memcpy((dest).data, (src).data, src_len);\
    vec_count(dest.data) = src_len;\
  } while (0)

#define string_builder_get_length(sb) vec_count((sb).data)


static void string_builder_append_rune(StringBuilder *p_sb, int rune);
static void string_builder_append_string_view(StringBuilder *p_sb, const StringView *p_sv);
static void string_builder_append_cstr(StringBuilder *p_sb, const char *cstr);
static char *string_builder_build(StringBuilder *p_sb);


// ------------------------------------ | Logger |
#ifndef LOG_OUT
#define LOG_OUT stdout
#endif // !LOG_OUT

#ifndef LOG_ERR
#define LOG_ERR stderr
#endif // !LOG_ERR

typedef enum {
  LOG_ALL,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_FATAL,
  LOG_NONE
} LogSeverity;
 
extern LogSeverity g_log_severity; 

#define NAME_SIZE (sizeof(__FILE__))

#define NAME_BUF char buf[NAME_SIZE + 16]; strncpy(buf, __FILE__":", NAME_SIZE+2); snprintf(buf + NAME_SIZE, 14, "%d", __LINE__)
 
#define log_trace(msg) do { NAME_BUF; log_trace_w_caller(buf, (msg)); } while (0)
#define log_info(msg) do { NAME_BUF; log_info_w_caller(buf, (msg)); } while (0)
#define log_warning(msg) do { NAME_BUF; log_warning_w_caller(buf, (msg)); } while (0)
#define log_error(msg) do { NAME_BUF; log_error_w_caller(buf, (msg)); } while (0)
#define log_fatal(msg, exit_code) do { NAME_BUF; log_fatal_w_caller(__FILE__, (msg), (exit_code)); } while (0)
 
#define logf_trace(fmt, ...) do { NAME_BUF; logf_trace_w_caller(buf, (fmt), __VA_ARGS__); } while (0)
#define logf_info(fmt, ...) do { NAME_BUF; logf_info_w_caller(buf, (fmt), __VA_ARGS__); } while (0)
#define logf_warning(fmt, ...) do { NAME_BUF; logf_warning_w_caller(buf, (fmt), __VA_ARGS__); } while (0)
#define logf_error(fmt, ...) do { NAME_BUF; logf_error_w_caller(buf, (fmt), __VA_ARGS__); } while (0)
#define logf_fatal(exit_code, fmt, ...) do { NAME_BUF; logf_fatal_w_caller(buf, (exit_code), (fmt), __VA_ARGS__); } while (0)



static void log_trace_w_caller(const char *caller_name, const char *msg);
static void log_info_w_caller(const char *caller_name, const char *msg);
static void log_warning_w_caller(const char *caller_name, const char *msg);
static void log_error_w_caller(const char *caller_name, const char *msg);
static void log_fatal_w_caller(const char *caller_name, const char *msg, int exit_code);

static void logf_trace_w_caller(const char *caller_name, const char *fmt, ...);
static void logf_info_w_caller(const char *caller_name, const char *fmt, ...);
static void logf_warning_w_caller(const char *caller_name, const char *fmt, ...);
static void logf_error_w_caller(const char *caller_name, const char *fmt, ...);
static void logf_fatal_w_caller(const char *caller_name, int exit_code, const char *fmt, ...);




// ------------------------------------ | Build |
typedef enum {
  COMPILER_C_ANY,
  COMPILER_CPP_ANY,
  COMPILER_GCC,
  COMPILER_CLANG,
} Compiler;


typedef struct {
  const char *repository;
  const char *dest;
  const char *post_cmd;
} GitDependency;

typedef struct {
  Compiler compiler;
  const char* cflags;
  const char* link_with;
  vec(char*) modules;

  const char *target_name;
  const char *build_dir;
  bool cache_modules;

  vec(GitDependency) git_dependencies;
} CompileCmd;

static bool cmd_run_sync(CompileCmd *p_cmd);
static bool cmd_run_monolite_sync(CompileCmd *p_cmd);
static bool cmd_compile_monolite(CompileCmd *p_cmd, StringBuilder *p_cmd_sb_out);
static int run_str_cmd_sync(const char *cmd_str);
static bool make_dir(const char *path);
static bool file_exist(const char *filepath);
static int compare_mod_time(const char *path1, const char *path2);
static void cmd_free(CompileCmd *p_cmd);
static char *shift_args(int *argc, char ***argv);

static char *shift_args(int *argc, char ***argv) {
  if (0 == *argc) return NULL;

  char *arg = *argv[0];
  *argv += 1;
  *argc -= 1;
  return arg;
}

#define BUILD_IMPLEMENTATION
#ifdef BUILD_IMPLEMENTATION
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>



// ------------------------------------ | StringView |
static bool string_view_equals(const StringView *p_lhs, const StringView *p_rhs) {
  assert(NULL != p_lhs);
  assert(NULL != p_rhs);

  return p_lhs->length == p_rhs->length
    && 0 == strncmp(p_lhs->p_begin, p_rhs->p_begin, p_lhs->length);
}

static bool string_view_equals_cstr(const StringView *p_sv, const char *p_str) {
  assert(NULL != p_sv);
  assert(NULL != p_str);

  return 0 == strncmp(p_sv->p_begin, p_str, p_sv->length);
}

static bool string_view_starts_with_cstr(const StringView *p_sv, const char *p_start) {
  assert(NULL != p_sv);

  size_t start_len = strlen(p_start);
  return p_sv->length >= start_len
    && 0 == strncmp(p_sv->p_begin, p_start, start_len);
}

static bool string_view_ends_with_cstr(const StringView *p_sv, const char *p_end) {
  assert(NULL != p_sv);

  size_t end_len = strlen(p_end);
  return p_sv->length >= end_len
    && 0 == strncmp(p_sv->p_begin + p_sv->length - end_len, p_end, end_len);
}

static size_t string_view_index_of(const StringView *p_sv, int rune) {
  assert(NULL != p_sv);

  const char *found = strchr(p_sv->p_begin, rune);
  return found - p_sv->p_begin;
}

static size_t string_view_last_index_of(const StringView *p_sv, int rune) {
  assert(NULL != p_sv);

  const char *found = strrchr(p_sv->p_begin, rune);
  return found - p_sv->p_begin;
}


static StringView string_view_chop_by_delim(StringView *p_sv, int delim) {
  size_t i = 0;
  while (i < p_sv->length && p_sv->p_begin[i] != delim) {
    ++i;
  }

  StringView result = string_view_slice(*p_sv, 0, i);
  size_t offset = i;
  if (i < p_sv->length) {
    offset += 1;
  }

  *p_sv = string_view_slice(*p_sv, offset, p_sv->length - offset);

  return result;
}

static StringView string_view_chop_by_sv(StringView *p_sv, const StringView *p_delim) {
  size_t i = 0;
  while (i < p_sv->length && p_sv->length - i >= p_delim->length
    && 0 != strncmp(p_sv->p_begin + i, p_delim->p_begin, p_delim->length)) {
    i += 1;
  }


  StringView result = string_view_slice(*p_sv, 0, i);

  size_t offset = i;
  if (i < p_sv->length) {
    offset += 1;
  }

  *p_sv = string_view_slice(*p_sv, offset, p_sv->length - offset);

  return result;
}


// ------------------------------------ | vec |
static void vec_mb_expand(void **pp_vec, size_t el_size) {
  assert(pp_vec != NULL);

  if (NULL == *pp_vec) {
    *pp_vec = a_allocate(sizeof(VecHeader) + VEC_INITIAL_CAPACITY * el_size);
    VecHeader *header = (VecHeader*)(*pp_vec);
    header->count = 0;
    header->capacity = VEC_INITIAL_CAPACITY;
    *pp_vec = (void*)((char*)*pp_vec + sizeof(VecHeader));
    return;
  }

  VecHeader *p_header = vec_get_header(*pp_vec);

  if (p_header->count == p_header->capacity) {
    vec_expand(pp_vec, el_size);
  }
}

static void vec_expand(void **pp_vec, size_t el_size) {
  assert(pp_vec != NULL);
  assert(*pp_vec != NULL);

  VecHeader *p_header = vec_get_header(*pp_vec);
  size_t old_cap = p_header->capacity;
  p_header->capacity *= VEC_GROW_FACTOR;
  void *tmp = 
    a_reallocate(p_header, 
                 sizeof(VecHeader) + old_cap * el_size, 
                 sizeof(VecHeader) + p_header->capacity * el_size);
  if (NULL == tmp) logf_fatal(137, "realocation for vector with new capacity %lu failed!", p_header->capacity); 
  *pp_vec = (void*)((char*)tmp + sizeof(VecHeader));
}


// ------------------------------------ | StringBuilder |
static void string_builder_append_rune(StringBuilder *p_sb, int rune) {
  assert(rune >= -128 && rune <= 127);
  vec_push(p_sb->data, (char)rune);
}

static void string_builder_append_string_view(StringBuilder *p_sb, const StringView *p_sv) {
  for (size_t i = 0; i < p_sv->length; ++i) {
    string_builder_append_rune(p_sb, p_sv->p_begin[i]);
  }
}

static void string_builder_append_cstr(StringBuilder *p_sb, const char *cstr) {
  string_builder_append_string_view(p_sb, &string_view_from_cstr(cstr));
}

static char *string_builder_build(StringBuilder *p_sb) {
  if ('\0' != *vec_back(p_sb->data)) {
    string_builder_append_rune(p_sb, '\0');
  }
  return p_sb->data;
}


// ------------------------------------ | Logger |
#define DO_LOG(severity)\
  do {\
    va_list args;\
    va_start(args, fmt);\
    fprintf(LOG_OUT, "[%s:" severity "]: ", caller_name);\
    vfprintf(LOG_OUT, fmt, args);\
  } while (0)

static void logf_trace_w_caller(const char *caller_name, const char *fmt, ...) {
  if (g_log_severity > LOG_ALL) return;
  DO_LOG("TRACE");
}

static void logf_info_w_caller(const char *caller_name, const char *fmt, ...) {
  if (g_log_severity > LOG_INFO) return;
  DO_LOG("INFO");
}

static void logf_warning_w_caller(const char *caller_name, const char *fmt, ...) {
  if (g_log_severity > LOG_WARNING) return;
  DO_LOG("WARN");
}

static void logf_error_w_caller(const char *caller_name, const char *fmt, ...) {
  if (g_log_severity > LOG_ERROR) return;
  DO_LOG("ERROR");
}

static void logf_fatal_w_caller(const char *caller_name, int exit_code, const char *fmt, ...) {
  if (g_log_severity > LOG_FATAL) return;
  DO_LOG("FATAL");
  exit(exit_code);
}


static void log_trace_w_caller(const char *caller_name, const char *msg) {
  logf_trace_w_caller(caller_name, "%s\n", msg);
}

static void log_info_w_caller(const char *caller_name, const char *msg) {
  logf_info_w_caller(caller_name, "%s\n", msg);
}

static void log_warning_w_caller(const char *caller_name, const char *msg) {
  logf_warning_w_caller(caller_name, "%s\n", msg);
}

static void log_error_w_caller(const char *caller_name, const char *msg) {
  logf_error_w_caller(caller_name, "%s\n", msg);
}

static void log_fatal_w_caller(const char *caller_name, const char *msg, int exit_code) {
  logf_fatal_w_caller(caller_name, exit_code, "%s\n", msg);
}


// ------------------------------------ | Allocator |
static void *a_allocate(size_t bytes) {
  void *ptr = malloc(bytes);
  assert(NULL != ptr);
  return ptr;
}

static void *a_reallocate(void *ptr, size_t old_size, size_t new_size) {
  (void)old_size;
  void *new_ptr = realloc(ptr, new_size);
  assert(NULL != new_ptr);
  return new_ptr;
}

static void *a_callocate(size_t nmemb, size_t memb_size) {
  void *ptr = calloc(nmemb, memb_size);
  assert(NULL != ptr);
  return ptr;
}

static void a_free(void *ptr) {
  free(ptr);
}


// ------------------------------------ | Build |
static bool file_exist(const char *filepath) {
  return access(filepath, F_OK) == 0;
}

static int compare_mod_time(const char *path1, const char *path2) {
  struct stat info1 = {0};
  struct stat info2 = {0};

  if (0 == stat(path1, &info1) && 0 == stat(path2, &info2)) {
    if (info1.st_mtime < info2.st_mtime) {
      return 2;
    } else if (info1.st_mtime > info2.st_mtime) {
      return 1;
    } else {
      return 0;
    }
  }

  return -1;
}

#define PLEASE_REBUILD_YOURSELF(argc, argv, cflags)\
  do {\
    int rebuild_is_needed = compare_mod_time(__FILE__, argv[0]);\
    if (1 == rebuild_is_needed || -1 == rebuild_is_needed) {\
      StringBuilder rebuild_cmd = {0};\
      set_compiler(&rebuild_cmd, COMPILER_C_ANY);\
      string_builder_append_rune(&rebuild_cmd, ' ');\
      string_builder_append_cstr(&rebuild_cmd, (cflags));\
      string_builder_append_cstr(&rebuild_cmd, " -o ");\
      string_builder_append_cstr(&rebuild_cmd, argv[0]);\
      string_builder_append_rune(&rebuild_cmd, ' ');\
      string_builder_append_cstr(&rebuild_cmd, __FILE__);\
      log_info("REBUILDING.");\
      bool rebuilded_successfuly = 0 == run_str_cmd_sync(string_builder_build(&rebuild_cmd));\
      if (rebuilded_successfuly) {\
        vec_count(rebuild_cmd.data) = 0;\
        for (size_t i = 0; i < argc; ++i) {\
          string_builder_append_cstr(&rebuild_cmd, argv[i]);\
          string_builder_append_rune(&rebuild_cmd, ' ');\
        }\
        if (0 == run_str_cmd_sync(string_builder_build(&rebuild_cmd))) exit(0);\
        exit(1);\
      }\
      string_builder_free(rebuild_cmd);\
    }\
  } while (0)


static bool make_dir(const char *path) {
  StringBuilder sb = {0};

  string_builder_append_cstr(&sb, "mkdir -p ");
  string_builder_append_cstr(&sb, path);
  
  bool ok = 0 == run_str_cmd_sync(string_builder_build(&sb));

  string_builder_free(sb);
  return ok;
}

static void cmd_free(CompileCmd *p_cmd) {
  assert(NULL != p_cmd);

  vec_free(p_cmd->modules);
  vec_free(p_cmd->git_dependencies);
}

static bool cmd_set_compiler_if_exist(StringBuilder *p_sb, const char *compiler) {
  assert(NULL != p_sb);

  bool ok = true;

  StringBuilder sb = {0};
  string_builder_append_cstr(&sb, compiler);
  string_builder_append_cstr(&sb, " --version > /dev/null 2>&1");
  const char *sb_str = string_builder_build(&sb);

  LogSeverity sev = g_log_severity;
  g_log_severity = LOG_ERROR;
  bool compiler_exists = 0 == run_str_cmd_sync(sb_str);
  g_log_severity = sev;

  if (!compiler_exists) {
    logf_info("Compiler %s is not found\n", compiler);
    ok = false;
    goto defer;
  }

  logf_info("Compiling with %s\n", compiler);

  string_builder_append_cstr(p_sb, compiler);
  string_builder_append_rune(p_sb, ' ');

defer:
  string_builder_free(sb);
  return ok;
}

static bool set_compiler(StringBuilder *p_sb, Compiler compiler) {
  assert(NULL != p_sb);

  log_info("Looking for a compiler.");

  bool compiler_found = false;
  switch (compiler) {
    case COMPILER_GCC: compiler_found = cmd_set_compiler_if_exist(p_sb, "gcc"); break;
    case COMPILER_CLANG: compiler_found = cmd_set_compiler_if_exist(p_sb, "clang"); break;

    case COMPILER_CPP_ANY:
    case COMPILER_C_ANY: {
      compiler_found = cmd_set_compiler_if_exist(p_sb, "cc")
        || cmd_set_compiler_if_exist(p_sb, "clang")
        || cmd_set_compiler_if_exist(p_sb, "gcc");
      break;
    }
  }

  return compiler_found;
}

static int run_str_cmd_sync(const char *cmd_str) {
  logf_info("CMD: %s\n", cmd_str);
  int status = system(cmd_str);

  if (-1 == status) {
    logf_error("Could not execute the command `%s`: %s\n", cmd_str, strerror(errno));
    return -1;
  }

  if (!WIFEXITED(status)) {
    log_error("CMD exited abnormally.\n");
  }

  return WEXITSTATUS(status);
}

bool cmd_compile_monolite(CompileCmd *p_cmd, StringBuilder *p_cmd_sb_out) {
  assert(NULL != p_cmd);
  assert(NULL != p_cmd_sb_out);

  if (!set_compiler(p_cmd_sb_out, p_cmd->compiler)) {
    return false;
  }

  string_builder_append_cstr(p_cmd_sb_out, p_cmd->cflags);
  string_builder_append_rune(p_cmd_sb_out, ' ');

  for (size_t i = 0; i < vec_count(p_cmd->modules); ++i) {
    string_builder_append_cstr(p_cmd_sb_out, p_cmd->modules[i]);
    string_builder_append_cstr(p_cmd_sb_out, ".c ");
  }

  string_builder_append_cstr(p_cmd_sb_out, "-o ");
  string_builder_append_cstr(p_cmd_sb_out, p_cmd->target_name);

  string_builder_append_rune(p_cmd_sb_out, ' ');
  string_builder_append_cstr(p_cmd_sb_out, p_cmd->link_with);

  return true;
}

static bool cmd_run_git_deps_sync(CompileCmd *p_cmd) {
  assert(NULL != p_cmd);

  if (NULL == p_cmd->git_dependencies) {
    return true;
  }

  bool ok = true;

  for (size_t i = 0; i < vec_count(p_cmd->git_dependencies); ++i) {
    GitDependency *dep = p_cmd->git_dependencies + i;

    bool curr_ok = true;
    if (!file_exist(dep->dest)) {
      StringBuilder git_cmd = {0};
      string_builder_append_cstr(&git_cmd, "git clone ");
      string_builder_append_cstr(&git_cmd, dep->repository);
      string_builder_append_rune(&git_cmd, ' ');
      string_builder_append_cstr(&git_cmd, dep->dest);

      curr_ok = 0 == run_str_cmd_sync(string_builder_build(&git_cmd));
      string_builder_free(git_cmd);
    }

    if (curr_ok) {
      curr_ok = 0 == run_str_cmd_sync(dep->post_cmd);
    }

    ok &= curr_ok;
  }

  return ok;
}

static bool cmd_run_modules_sync(CompileCmd *p_cmd) {
  StringBuilder cmd_module_base = {0};
  StringBuilder cmd_target = {0};
  StringBuilder cmd_module = {0};
  bool ok = true;
  bool any_module_was_rebuilt = false;

  if (!set_compiler(&cmd_module_base, p_cmd->compiler)) {
    return false;
  }

  string_builder_append_cstr(&cmd_module_base, p_cmd->cflags);
  string_builder_append_rune(&cmd_module_base, ' ');
  string_builder_copy(cmd_target, cmd_module_base);

  for (size_t i = 0; i < vec_count(p_cmd->modules); ++i) {
    StringView sv_module = string_view_from_cstr(p_cmd->modules[i]);
    StringView module_name = {0};
    while (!string_view_is_empty(sv_module)) {
      module_name = string_view_chop_by_delim(&sv_module, '/');
    }

    if (string_view_is_empty(module_name)) {
      logf_error("Error in module name %s\n", p_cmd->modules[i]);
      ok = false;
      continue;
    }

    StringBuilder src_path_sb = {0};
    string_builder_append_cstr(&src_path_sb, p_cmd->modules[i]);
    string_builder_append_cstr(&src_path_sb, ".c");

    StringBuilder obj_path_sb = {0};
    if (NULL != p_cmd->build_dir) {
      string_builder_append_cstr(&obj_path_sb, p_cmd->build_dir);
      string_builder_append_rune(&obj_path_sb, '/');
    }
    string_builder_append_string_view(&obj_path_sb, &module_name);
    string_builder_append_cstr(&obj_path_sb, ".o");

    const char *src_path = string_builder_build(&src_path_sb);
    const char *obj_path = string_builder_build(&obj_path_sb);

    string_builder_append_cstr(&cmd_target, obj_path);
    string_builder_append_rune(&cmd_target, ' ');

    int rebuild_is_needed = compare_mod_time(src_path, obj_path);
    rebuild_is_needed = 1 == rebuild_is_needed || -1 == rebuild_is_needed;

    if (rebuild_is_needed) {
      any_module_was_rebuilt = true;
      string_builder_copy(cmd_module, cmd_module_base);
      string_builder_append_cstr(&cmd_module, "-c ");
      string_builder_append_cstr(&cmd_module, src_path);
      string_builder_append_cstr(&cmd_module, " -o ");
      string_builder_append_cstr(&cmd_module, obj_path);

      ok = 0 == run_str_cmd_sync(string_builder_build(&cmd_module)) && ok;
      string_builder_free(cmd_module);
    }

    string_builder_free(src_path_sb);
    string_builder_free(obj_path_sb);
  }

  // target
  if (ok) {
    if (any_module_was_rebuilt || !file_exist(p_cmd->target_name)) {
      string_builder_append_cstr(&cmd_target, " -o ");
      string_builder_append_cstr(&cmd_target, p_cmd->target_name);
      string_builder_append_rune(&cmd_target, ' ');
      string_builder_append_cstr(&cmd_target, p_cmd->link_with);

      ok = 0 == run_str_cmd_sync(string_builder_build(&cmd_target));
    } else {
      log_info("No files that need to be rebuilt.");
    }
  }

  string_builder_free(cmd_target);
  string_builder_free(cmd_module_base);
  return ok;
}

static bool cmd_run_monolite_sync(CompileCmd *p_cmd) {
  assert(NULL != p_cmd);

  bool ok = true;
  StringBuilder cmd_sb = {0};

  if (!cmd_compile_monolite(p_cmd, &cmd_sb)) {
    ok = false;
    goto defer;
  }


  ok = cmd_run_git_deps_sync(p_cmd);

  if (ok) {
    const char *cmd_str = string_builder_build(&cmd_sb);
    ok = 0 == run_str_cmd_sync(cmd_str); 
  }

defer:
  if (string_builder_get_length(cmd_sb)) {
    string_builder_free(cmd_sb);
  }

  return ok;
}

static bool cmd_run_sync(CompileCmd *p_cmd) {
  assert(NULL != p_cmd);

  if (!(NULL != p_cmd->build_dir && make_dir(p_cmd->build_dir))) {
    return false;
  }

  if (p_cmd->cache_modules) {
    return cmd_run_modules_sync(p_cmd);
  } else {
    return cmd_run_monolite_sync(p_cmd);
  }
}


#endif // !BUILD_IMPLEMENTATION

#endif // !__BUILD_H__

