// Copyright (c) 2025 Michael Navarro
// MIT license, see LICENSE for more

#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_log(NOB_INFO, "--- Starting build -------------------------------------");

    Nob_Cmd cmd = {0};

    int res = nob_file_exists(BUILD_FOLDER "sqlite3.o");
    if (res == 0) // build/sqlite3.o does not exist, build it
    {
#if defined(_MSC_VER)
        nob_cmd_append(&cmd, "cl", "/c", "src/deps/sqlite3.c", "/Fo:build/sqlite3.o");
#else
        nob_cmd_append(&cmd, "cc", "-c", "src/deps/sqlite3.c", "-o", "build/sqlite3.o");
#endif // _MSC_VER

        if (!nob_cmd_run_sync_and_reset(&cmd))
            return 1;
    }
    else if (res == -1) // error is already logged, can just early exit as we depend on sqlite3.
        return 1;
    else
        nob_log(NOB_INFO, "Found build/sqlite3.o");

#if defined(_MSC_VER)
    nob_cmd_append(&cmd, "cl", "/W4", "/I", "src/deps", "build/sqlite3.o");
#else
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-Isrc/deps", "build/sqlite3.o");
#endif // _MSC_VER

    Nob_File_Paths src_files = {0};
    if (!nob_read_entire_dir(SRC_FOLDER, &src_files))
        return 1;

    for (size_t i = 0; i < src_files.count; ++i)
    {
        const char *temp_file_name = src_files.items[i];
        if (strcmp(temp_file_name, ".") == 0)
            continue;
        if (strcmp(temp_file_name, "..") == 0)
            continue;

        const char *temp_full_path = nob_temp_sprintf("%s%s", SRC_FOLDER, temp_file_name);
        if (nob_get_file_type(temp_full_path) == NOB_FILE_DIRECTORY)
            continue;

        int len = strlen(temp_file_name);
        const char *file_ext = &temp_file_name[len - 2];
        if (strcmp(file_ext, ".h") == 0)
            continue;
        nob_cmd_append(&cmd, temp_full_path);
    }

#if defined(_MSC_VER)
    nob_cmd_append(&cmd, "/Fe:" BUILD_FOLDER "rmbrl.exe");
#else
    nob_cmd_append(&cmd, "-o", BUILD_FOLDER "rmbrl");
#endif // _MSC_VER

    if (!nob_cmd_run_sync_and_reset(&cmd))
        return 1;

    nob_log(NOB_INFO, "--- Build complete -------------------------------------");
    return 0;
}
