// Copyright (c) 2025 Michael Navarro
// MIT license, see LICENSE for more

#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define BUILD_FAILED_MSG                                                                           \
    nob_log(NOB_ERROR, "--- Build failed ---------------------------------------");

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_log(NOB_INFO, "--- Starting build -------------------------------------");

    bool install_rmbrl = false;
    Nob_Cmd cmd = {0};

    while (argc > 1)
    {
        char *flag = argv[1];
        if (strcmp(flag, "--install") == 0 || strcmp(flag, "-i") == 0)
        {
            install_rmbrl = true;
        }
        else
        {
            nob_log(NOB_WARNING, "Unknown flag: \"%s\"", flag);
        }
        nob_shift_args(&argc, &argv);
    }

#if defined(_MSC_VER)
    int res = nob_file_exists(BUILD_FOLDER "sqlite3.obj");
#else
    int res = nob_file_exists(BUILD_FOLDER "sqlite3.o");
#endif // end _MSC_VER

    if (res == 0) // build/sqlite3 object file does not exist, build it
    {
#if defined(_MSC_VER)
        nob_cmd_append(&cmd, "cl", "/c", "src/deps/sqlite3.c", "/Fo:build/sqlite3.obj");
#else
        nob_cmd_append(&cmd, "cc", "-c", "src/deps/sqlite3.c", "-o", "build/sqlite3.o");
#endif // end _MSC_VER

        if (!nob_cmd_run_sync_and_reset(&cmd))
        {
            BUILD_FAILED_MSG
            return 1;
        }
    }
    else if (res == -1) // error is already logged, can just early exit as we depend on sqlite3.
    {
        BUILD_FAILED_MSG
        return 1;
    }
    else
    {
#if defined(_MSC_VER)
        nob_log(NOB_INFO, "Found build/sqlite3.obj");
#else
        nob_log(NOB_INFO, "Found build/sqlite3.o");
#endif // end _MSC_VER
    }

#if defined(_MSC_VER)
    nob_cmd_append(&cmd, "cl", "/W4", "/I", "src/deps", "build/sqlite3.obj");
#else
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-Isrc/deps", "build/sqlite3.o");
#endif // end _MSC_VER

    Nob_File_Paths src_files = {0};
    if (!nob_read_entire_dir(SRC_FOLDER, &src_files))
    {
        BUILD_FAILED_MSG
        return 1;
    }

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
    nob_cmd_append(&cmd, "/Fe:" BUILD_FOLDER "rmbrl.exe", "/Fo:" BUILD_FOLDER "rmbrl.obj");
#else
    nob_cmd_append(&cmd, "-o", BUILD_FOLDER "rmbrl");
#endif // _MSC_VER

    if (!nob_cmd_run_sync_and_reset(&cmd))
    {
        BUILD_FAILED_MSG
        return 1;
    }

    if (install_rmbrl)
    {
#if defined(_WIN32)
        char *appdata = getenv("APPDATA");
        if (appdata == NULL)
        {
            nob_log(NOB_ERROR, "APPDATA environment variable not found!");
            BUILD_FAILED_MSG
            return 1;
        }
        char rmbrl_path[512];
        snprintf(rmbrl_path, sizeof(rmbrl_path), "%s\\rmbrl\\", appdata);

        char *install_path = rmbrl_path;
        nob_cmd_append(&cmd, "cmd", "/c", "copy", "build\\rmbrl.exe", install_path);
#else
        char *install_path = "/usr/local/bin";
        char *rmbrl_exe = BUILD_FOLDER "rmbrl";
        nob_cmd_append(&cmd, "sudo", "cp", rmbrl_exe, install_path);

        char *home_env = getenv("HOME");
        if (home_env == NULL)
        {
            nob_log(NOB_ERROR, "HOME environment variable not found!");
            BUILD_FAILED_MSG
            return 1;
        }
        char rmbrl_path[512];
        snprintf(rmbrl_path, sizeof(rmbrl_path), "%s/.local/share/rmbrl/", home_env);

#endif // end _WIN32

        // Windows - create path C:\Users\username\AppData\Roaming\rmbrl
        // POSIX - create path /home/username/.local/share/rmbrl
        if (!nob_mkdir_if_not_exists(rmbrl_path))
        {
            BUILD_FAILED_MSG
            return 1;
        }

        nob_log(NOB_INFO, "Installing remembrall to \"%s\"", install_path);
        if (!nob_cmd_run_sync_and_reset(&cmd))
        {
            BUILD_FAILED_MSG
            return 1;
        }

#if defined(_WIN32)
        // TODO: add rmbrl_path to PATH
        // Problem: we cannot just append to %PATH% with setx as that would combine both system and
        // user PATH
        // Probably will need to use setx to get user-level path value and pipe to temp env var
        // From there append rmbrl_path to user-level path using the temp env var as the placeholder
        // for existing user-level path.
        // Now, when %PATH% is used, it will have our rmbrl_path in the user-level path without
        // duplicating system-level and user-level PATH.

#endif
    }

    nob_log(NOB_INFO, "--- Build done -----------------------------------------");
    return 0;
}
