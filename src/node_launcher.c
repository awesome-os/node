/**
 * @file src/node_launcher.c
 * @description Zero-dependency native C11 Win64/POSIX self-hydrating Ghost Node.js launcher.
 * Hydrates Node.js 27 (ABI 153, default) or Node.js 26 (ABI 147, B8G_NODE_LINE=26) once
 * from awesome-os/b8g-ghost (cas-v1) into ~/.b8g/cas/node/<sha256>/ and execs verbatim.
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
  #define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>
  #include <process.h>
  #define PATH_SEP '\\'
  #define OS_KEY "win64"
  #define NODE_REL_BIN "node.exe"
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #define PATH_SEP '/'
  #if defined(__APPLE__) && defined(__aarch64__)
    #define OS_KEY "macos-arm64"
  #elif defined(__aarch64__)
    #define OS_KEY "linux-arm64"
  #else
    #define OS_KEY "linux-x64"
  #endif
  #define NODE_REL_BIN "bin/node"
#endif

#define GHOST_NODE_LAUNCHER_VERSION "0.2.0"
#define INDEX_URL "https://raw.githubusercontent.com/awesome-os/b8g-ghost/main/manifests/index.json"
#define BLOB_BASE_URL "https://github.com/awesome-os/b8g-ghost/releases/download/cas-v1"
#define MAX_PATH_BUF 1024

static int file_exists(const char *path) {
#if defined(_WIN32)
  DWORD attr = GetFileAttributesA(path);
  return (attr != INVALID_FILE_ATTRIBUTES);
#else
  struct stat st;
  return (stat(path, &st) == 0);
#endif
}

static void mkdir_p(const char *dir) {
  char tmp[MAX_PATH_BUF];
  snprintf(tmp, sizeof(tmp), "%s", dir);
  size_t len = strlen(tmp);
  if (len == 0) return;
  if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') tmp[len - 1] = '\0';
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char saved = *p;
      *p = '\0';
#if defined(_WIN32)
      if (!(p - tmp == 2 && tmp[1] == ':')) _mkdir(tmp);
#else
      mkdir(tmp, 0755);
#endif
      *p = saved;
    }
  }
#if defined(_WIN32)
  _mkdir(tmp);
#else
  mkdir(tmp, 0755);
#endif
}

static int get_cas_dir(char *out, size_t cap) {
  const char *env_dir = getenv("B8G_GHOST_CACHE_DIR");
  if (env_dir && env_dir[0] != '\0') {
    snprintf(out, cap, "%s", env_dir);
    return 0;
  }
#if defined(_WIN32)
  const char *home = getenv("USERPROFILE");
  if (!home || home[0] == '\0') home = getenv("HOME");
  if (!home || home[0] == '\0') return -1;
  snprintf(out, cap, "%s\\.b8g\\cas", home);
#else
  const char *home = getenv("HOME");
  if (!home || home[0] == '\0') return -1;
  snprintf(out, cap, "%s/.b8g/cas", home);
#endif
  return 0;
}

static char *read_file_text(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return NULL;
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  long sz = ftell(fp);
  if (sz <= 0 || sz > 16 * 1024 * 1024) {
    fclose(fp);
    return NULL;
  }
  fseek(fp, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(fp);
    return NULL;
  }
  size_t n = fread(buf, 1, (size_t)sz, fp);
  buf[n] = '\0';
  fclose(fp);
  return buf;
}

static int lookup_sha256_in_index(const char *json, const char *filename, char out_sha[65]) {
  char key[256];
  snprintf(key, sizeof(key), "\"%s\"", filename);
  const char *pos = strstr(json, key);
  if (!pos) return -1;
  const char *sha_key = strstr(pos, "\"sha256\"");
  if (!sha_key) return -1;
  const char *colon = strchr(sha_key + 8, ':');
  if (!colon) return -1;
  const char *q1 = strchr(colon, '"');
  if (!q1) return -1;
  q1++;
  for (int i = 0; i < 64; i++) {
    char c = q1[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return -1;
    }
    out_sha[i] = (c >= 'A' && c <= 'F') ? (char)(c + 32) : c;
  }
  out_sha[64] = '\0';
  return 0;
}

static int run_cmd(const char *cmd) {
  return system(cmd);
}

static int refresh_index(const char *cas_dir, const char *index_path) {
  mkdir_p(cas_dir);
  char cmd[MAX_PATH_BUF * 2];
  snprintf(cmd, sizeof(cmd), "curl -fsSL \"%s\" -o \"%s.tmp\"", INDEX_URL, index_path);
  if (run_cmd(cmd) != 0) return -1;
#if defined(_WIN32)
  char tmp_path[MAX_PATH_BUF];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", index_path);
  MoveFileExA(tmp_path, index_path, MOVEFILE_REPLACE_EXISTING);
#else
  char tmp_path[MAX_PATH_BUF];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", index_path);
  rename(tmp_path, index_path);
#endif
  return 0;
}

static int hydrate_blob(const char *cas_dir, const char *sha, const char *target_dir) {
  mkdir_p(target_dir);
  char tmp_arc[MAX_PATH_BUF];
  snprintf(tmp_arc, sizeof(tmp_arc), "%s%cnode-%s.tar.zst", cas_dir, PATH_SEP, sha);

  char cmd[MAX_PATH_BUF * 3];
  fprintf(stderr, "[awesome-os/node] Hydrating cas-v1 blob %.12s...\n", sha);
  snprintf(cmd, sizeof(cmd), "curl -fsSL \"%s/%s\" -o \"%s\"", BLOB_BASE_URL, sha, tmp_arc);
  if (run_cmd(cmd) != 0) {
    remove(tmp_arc);
    return -1;
  }

#if defined(_WIN32)
  snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", tmp_arc, target_dir);
#else
  snprintf(cmd, sizeof(cmd), "tar --zstd -xf \"%s\" -C \"%s\"", tmp_arc, target_dir);
#endif
  int rc = run_cmd(cmd);
  remove(tmp_arc);
  return rc == 0 ? 0 : -1;
}

#if defined(_WIN32)
static void append_quoted_win32_arg(char *dst, size_t cap, const char *arg) {
  size_t len = strlen(dst);
  if (len + 2 >= cap) return;
  if (len > 0) {
    dst[len++] = ' ';
    dst[len] = '\0';
  }
  int needs_quote = (arg[0] == '\0' || strpbrk(arg, " \t\n\v\"") != NULL);
  if (!needs_quote) {
    snprintf(dst + len, cap - len, "%s", arg);
    return;
  }
  if (len + 1 < cap) dst[len++] = '"';
  size_t backslashes = 0;
  for (const char *p = arg; *p && len + 4 < cap; p++) {
    if (*p == '\\') {
      backslashes++;
    } else if (*p == '"') {
      for (size_t i = 0; i < backslashes * 2 + 1 && len + 2 < cap; i++) dst[len++] = '\\';
      dst[len++] = '"';
      backslashes = 0;
    } else {
      for (size_t i = 0; i < backslashes && len + 2 < cap; i++) dst[len++] = '\\';
      backslashes = 0;
      dst[len++] = *p;
    }
  }
  for (size_t i = 0; i < backslashes * 2 && len + 2 < cap; i++) dst[len++] = '\\';
  if (len + 1 < cap) dst[len++] = '"';
  dst[len] = '\0';
}

static int exec_node_win32(const char *node_bin, int argc, char **argv) {
  size_t cap = 32768;
  char *cmdline = (char *)calloc(1, cap);
  if (!cmdline) return 1;
  append_quoted_win32_arg(cmdline, cap, node_bin);
  for (int i = 1; i < argc; i++) {
    append_quoted_win32_arg(cmdline, cap, argv[i]);
  }

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  if (!CreateProcessA(node_bin, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
    fprintf(stderr, "[awesome-os/node] CreateProcess failed (%lu) for %s\n", GetLastError(), node_bin);
    free(cmdline);
    return 127;
  }
  free(cmdline);
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return (int)exit_code;
}
#endif

int main(int argc, char **argv) {
  const char *line = getenv("B8G_NODE_LINE");
  if (!line || (strcmp(line, "26") != 0 && strcmp(line, "27") != 0)) {
    line = "27";
  }

  char logical_key[128];
  snprintf(logical_key, sizeof(logical_key), "node-%s-%s.tar.zst", line, OS_KEY);

  char cas_dir[MAX_PATH_BUF];
  if (get_cas_dir(cas_dir, sizeof(cas_dir)) != 0) {
    fprintf(stderr, "[awesome-os/node] Could not resolve CAS cache dir (~/.b8g/cas).\n");
    return 1;
  }

  if (argc == 2 && strcmp(argv[1], "--ghost-info") == 0) {
    printf("@awesome-os/node-launcher %s | line=v%s | abi=%s | key=%s | cas=%s\n",
           GHOST_NODE_LAUNCHER_VERSION, line,
           (strcmp(line, "26") == 0 ? "147" : "153"),
           logical_key, cas_dir);
    return 0;
  }

  char index_path[MAX_PATH_BUF];
  snprintf(index_path, sizeof(index_path), "%s%cindex.json", cas_dir, PATH_SEP);

  char sha[65] = {0};
  char *json = read_file_text(index_path);
  int found = (json != NULL && lookup_sha256_in_index(json, logical_key, sha) == 0);
  if (json) free(json);

  if (!found) {
    if (refresh_index(cas_dir, index_path) != 0) {
      fprintf(stderr, "[awesome-os/node] Failed to fetch cas-v1 index from %s\n", INDEX_URL);
      return 1;
    }
    json = read_file_text(index_path);
    found = (json != NULL && lookup_sha256_in_index(json, logical_key, sha) == 0);
    if (json) free(json);
    if (!found) {
      fprintf(stderr, "[awesome-os/node] Entry '%s' not found in cas-v1 index.\n", logical_key);
      return 1;
    }
  }

  char target_dir[MAX_PATH_BUF];
  char node_bin[MAX_PATH_BUF];
  snprintf(target_dir, sizeof(target_dir), "%s%cnode%c%s", cas_dir, PATH_SEP, PATH_SEP, sha);
  snprintf(node_bin, sizeof(node_bin), "%s%c%s", target_dir, PATH_SEP, NODE_REL_BIN);

  if (!file_exists(node_bin)) {
    if (hydrate_blob(cas_dir, sha, target_dir) != 0 || !file_exists(node_bin)) {
      fprintf(stderr, "[awesome-os/node] Failed to hydrate %s (%s)\n", logical_key, sha);
      return 1;
    }
  }

#if defined(_WIN32)
  return exec_node_win32(node_bin, argc, argv);
#else
  argv[0] = node_bin;
  execv(node_bin, argv);
  perror("[awesome-os/node] execv");
  return 127;
#endif
}
