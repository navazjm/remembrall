// Copyright (c) 2025 Michael Navarro
// MIT license, see LICENSE for more

#include "./deps/sqlite3.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#endif

#define RMBRL_VERSION "v0.1.0"

#ifndef RMBRL_REALLOC
#include <stdlib.h>
#define RMBRL_REALLOC realloc
#endif /* RMBRL_REALLOC */

#ifndef RMBRL_FREE
#include <stdlib.h>
#define RMBRL_FREE free
#endif /* RMBRL_FREE */

#ifndef RMBRL_ASSERT
#include <assert.h>
#define RMBRL_ASSERT assert
#endif /* RMBRL_ASSERT */

#ifndef RMBRL_STRDUP
#include <string.h>
#define RMBRL_STRDUP strdup
#endif /* RMBRL_STRDUP */

static inline char *rmbrl_strdup(const char *str)
{
    char *dup = RMBRL_STRDUP(str);
    if (dup == NULL)
        RMBRL_ASSERT(0 && "rmbrl strdup: Out of memory");
    return dup;
}

#define RMBRL_TODO(message)                                                                        \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message);                         \
        abort();                                                                                   \
    } while (0)
#define RMBRL_UNREACHABLE(message)                                                                 \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message);                  \
        abort();                                                                                   \
    } while (0)
#define RMBRL_CLEANUP_RETURN(value)                                                                \
    do                                                                                             \
    {                                                                                              \
        result = (value);                                                                          \
        goto cleanup;                                                                              \
    } while (0)

// Dynamic Arrays

#ifndef RMBRL_DA_INIT_CAP
#define RMBRL_DA_INIT_CAP 256
#endif

#define rmbrl_da_reserve(da, expected_capacity)                                                    \
    do                                                                                             \
    {                                                                                              \
        if ((expected_capacity) > (da)->capacity)                                                  \
        {                                                                                          \
            if ((da)->capacity == 0)                                                               \
            {                                                                                      \
                (da)->capacity = RMBRL_DA_INIT_CAP;                                                \
            }                                                                                      \
            while ((expected_capacity) > (da)->capacity)                                           \
            {                                                                                      \
                (da)->capacity *= 2;                                                               \
            }                                                                                      \
            (da)->items = RMBRL_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items));       \
            RMBRL_ASSERT((da)->items != NULL && "Buy more RAM lol");                               \
        }                                                                                          \
    } while (0)

#define rmbrl_da_append(da, item)                                                                  \
    do                                                                                             \
    {                                                                                              \
        rmbrl_da_reserve((da), (da)->count + 1);                                                   \
        (da)->items[(da)->count++] = (item);                                                       \
    } while (0)

#define rmbrl_da_free(da) RMBRL_FREE((da).items)

void rmbrl_print_help()
{
    printf("Usage: program (COMMAND) [FLAGS]\n\n");

    printf("Commands:\n");
    printf("  add     Add memory to your collection (supports --project)\n");
    printf("  peek    Show what you're currently remembering (supports --all, --project)\n");
    printf("  clear   Forget memories (supports --all, --project)\n\n");

    printf("Command Flags:\n");
    printf("  -p, --project    Tag and filter memories by project name\n");
    printf("                   (supported by: add, peek, clear)\n");
    printf("  -a, --all        Apply operation to all memories\n");
    printf("                   (supported by: peek, clear)\n\n");

    printf("Global Flags:\n");
    printf("  -h, --help       Show help information\n");
    printf("  -V, --version    Show version information\n");
    printf("  -v, --verbose    Enable verbose output\n");
    printf("  -s, --silent     Enable silent mode\n");
    printf("  -n, --dry-run    Perform dry run without making changes\n");
}

typedef enum
{
    RMBRL_VERB_NORMAL = 0,
    RMBRL_VERB_SILENT,
    RMBRL_VERB_VERBOSE,
} Rmbrl_Verbosity_Level;

typedef enum
{
    RMBRL_LOG_INFO,
    RMBRL_LOG_WARNING,
    RMBRL_LOG_ERROR,
} Rmbrl_Log_Level;

void rmbrl_log(Rmbrl_Log_Level level, const char *fmt, ...)
{
    switch (level)
    {
    case RMBRL_LOG_INFO:
        fprintf(stderr, "[INFO] ");
        break;
    case RMBRL_LOG_WARNING:
        fprintf(stderr, "[WARNING] ");
        break;
    case RMBRL_LOG_ERROR:
        fprintf(stderr, "[ERROR] ");
        break;
    default:
        RMBRL_UNREACHABLE("log level");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

typedef enum
{
    RMBRL_CMD_UNKNOWN = 0,
    RMBRL_CMD_ADD,
    RMBRL_CMD_PEEK,
    RMBRL_CMD_CLEAR,
} Rmbrl_Command_Function;

char *rmbrl_command_function_str(Rmbrl_Command_Function func)
{
    switch (func)
    {
    case RMBRL_CMD_UNKNOWN:
        return "?";
    case RMBRL_CMD_ADD:
        return "add";
    case RMBRL_CMD_PEEK:
        return "peek";
    case RMBRL_CMD_CLEAR:
        return "clear";
    default:
        RMBRL_UNREACHABLE("command function str");
    }
}

typedef struct
{
    Rmbrl_Command_Function function;
    Rmbrl_Verbosity_Level verbosity;
    char *project;
    char *task;
    bool all;
    bool dry_run;
} Rmbrl_Command;

typedef struct
{
    char **items;
    size_t capacity;
    size_t count;
} Rmbrl_DA_Strs;

void rmbrl_command_debug_print(Rmbrl_Command *cmd)
{
    char *cmd_verbosity;
    switch (cmd->verbosity)
    {
    case RMBRL_VERB_NORMAL:
        cmd_verbosity = "normal";
        break;
    case RMBRL_VERB_SILENT:
        cmd_verbosity = "silent";
        break;
    case RMBRL_VERB_VERBOSE:
        cmd_verbosity = "verbose";
        break;
    default:
        RMBRL_UNREACHABLE("command debug print");
    }

    rmbrl_log(RMBRL_LOG_INFO, "Parsed Command Line Args:\n");
    rmbrl_log(RMBRL_LOG_INFO, "    function: %s\n", rmbrl_command_function_str(cmd->function));
    rmbrl_log(RMBRL_LOG_INFO, "    task: %s\n", cmd->task);
    rmbrl_log(RMBRL_LOG_INFO, "    project: %s\n", cmd->project);
    rmbrl_log(RMBRL_LOG_INFO, "    all: %s\n", cmd->all ? "true" : "false");
    rmbrl_log(RMBRL_LOG_INFO, "    dry-run: %s\n", cmd->dry_run ? "true" : "false");
    rmbrl_log(RMBRL_LOG_INFO, "    verbosity: %s\n", cmd_verbosity);
}

//
// NOTES:
//
// Use sqlite3_exec when statement does not use user supplied input
// Otherwise, manually prepare the statement to prevent SQL Injections.
//
// SQLITE_ENABLE_UPDATE_DELETE_LIMIT Compile time flag is off by default.
// Therefore, we cannot reliably use ORDER BY with DELETE or UPDATE.
// So, in order to delete the most recent item, --all is NOT passed, then we will
// have to fetch that item first and then delete by ID.
// Otherwise, if --all flag is passed, we don't need to fetch first.
//

int rmbrl_db_begin_transaction(sqlite3 *db, Rmbrl_Verbosity_Level verbosity)
{
    char *err_msg;
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, 0, &err_msg) != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to begin transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    if (verbosity == RMBRL_VERB_VERBOSE)
        rmbrl_log(RMBRL_LOG_INFO, "Begin transaction...\n");

    return 0;
}

int rmbrl_db_rollback_transaction(sqlite3 *db, Rmbrl_Verbosity_Level verbosity)
{
    char *err_msg;
    if (sqlite3_exec(db, "ROLLBACK;", NULL, 0, &err_msg) != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to rollback transaction: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    if (verbosity == RMBRL_VERB_VERBOSE)
        rmbrl_log(RMBRL_LOG_INFO, "Rollback transaction...\n");

    return 0;
}

int rmbrl_command_add(Rmbrl_Command *cmd, sqlite3 *db)
{
    if (strlen(cmd->task) > 256)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Task \"%s\" exceeds char limit of 256 bytes.\n", cmd->task);
        return 1;
    }
    if (cmd->project && strlen(cmd->project) > 256)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Project \"%s\" exceeds char limit of 256 bytes.\n",
                  cmd->project);
        return 1;
    }

    if (cmd->dry_run)
    {
        rmbrl_log(RMBRL_LOG_INFO, "Performing dry run. Memory will NOT be remembered!\n");
        int result = rmbrl_db_begin_transaction(db, cmd->verbosity);
        if (result != 0)
            return result;
    }

    sqlite3_stmt *stmt;
    const char *raw_stmt = "INSERT INTO memories (task, project) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, raw_stmt, -1, &stmt, NULL) != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_bind_text(stmt, 1, cmd->task, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, cmd->project ? cmd->project : "", -1, SQLITE_STATIC);

    if (cmd->verbosity == RMBRL_VERB_VERBOSE)
    {
        char *stmt_str = sqlite3_expanded_sql(stmt);
        rmbrl_log(RMBRL_LOG_INFO, "Query: %s\n", stmt_str);
    }

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to remember: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    rmbrl_log(RMBRL_LOG_INFO, "\"%s\" was added to your memory!\n", cmd->task);

    if (cmd->dry_run)
    {
        result = rmbrl_db_rollback_transaction(db, cmd->verbosity);
        if (result != 0)
            return result;
    }

    return 0;
}

int rmbrl_command_peek(Rmbrl_Command *cmd, sqlite3 *db)
{
    if (cmd->project && strlen(cmd->project) > 256)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Project \"%s\" exceeds char limit of 256 bytes.\n",
                  cmd->project);
        return 1;
    }

    sqlite3_stmt *stmt;
    char *raw_stmt;

    if (cmd->project)
        raw_stmt = "SELECT * FROM memories WHERE project = ? ORDER BY created_at DESC;";
    else
        raw_stmt = "SELECT * FROM memories ORDER BY created_at DESC;";

    if (sqlite3_prepare_v2(db, raw_stmt, -1, &stmt, NULL) != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (cmd->project)
        sqlite3_bind_text(stmt, 1, cmd->project, -1, SQLITE_STATIC);

    if (cmd->verbosity == RMBRL_VERB_VERBOSE)
    {
        char *stmt_str = sqlite3_expanded_sql(stmt);
        rmbrl_log(RMBRL_LOG_INFO, "Query: %s\n", stmt_str);
    }

    if (cmd->verbosity != RMBRL_VERB_SILENT)
        rmbrl_log(RMBRL_LOG_INFO, "Currently Remembering:\n");

    int result;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        // int id = sqlite3_column_int(stmt, 0);

        const unsigned char *task = sqlite3_column_text(stmt, 1);
        rmbrl_log(RMBRL_LOG_INFO, "    \"%s\"", task);

        const char *project = (char *)sqlite3_column_text(stmt, 2);
        if (strlen(project) > 0)
            fprintf(stderr, " -- %s", project);

        if (cmd->verbosity == RMBRL_VERB_VERBOSE)
        {
            char *created_at = rmbrl_strdup((char *)sqlite3_column_text(stmt, 3));
            created_at[10] = '\0';
            fprintf(stderr, " -- %s", created_at);
            RMBRL_FREE(created_at);
        }

        fprintf(stderr, "\n");

        if (!cmd->all)
        {
            result = SQLITE_DONE;
            break;
        }
    }
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to remember: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    return 0;
}

int rmbrl_command_clear(Rmbrl_Command *cmd, sqlite3 *db)
{
    if (cmd->project && strlen(cmd->project) > 256)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Project \"%s\" exceeds char limit of 256 bytes.\n",
                  cmd->project);
        return 1;
    }

    if (cmd->dry_run)
    {
        rmbrl_log(RMBRL_LOG_INFO, "Performing dry run. Memory will NOT be forgotten!\n");
        int result = rmbrl_db_begin_transaction(db, cmd->verbosity);
        if (result != 0)
            return result;
    }

    int id = -1;
    sqlite3_stmt *stmt;
    char *raw_stmt;

    // not clearing all forgotten items, have to retrieve id first before deleting.
    if (!cmd->all)
    {
        if (cmd->project)
            raw_stmt = "SELECT * FROM memories WHERE project = ? ORDER BY created_at DESC LIMIT 1;";
        else
            raw_stmt = "SELECT * FROM memories ORDER BY created_at DESC LIMIT 1";

        if (sqlite3_prepare_v2(db, raw_stmt, -1, &stmt, NULL) != SQLITE_OK)
        {
            rmbrl_log(RMBRL_LOG_ERROR, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            return 1;
        }

        if (cmd->project)
            sqlite3_bind_text(stmt, 1, cmd->project, -1, SQLITE_STATIC);

        if (cmd->verbosity == RMBRL_VERB_VERBOSE)
        {
            char *stmt_str = sqlite3_expanded_sql(stmt);
            rmbrl_log(RMBRL_LOG_INFO, "Query: %s\n", stmt_str);
        }

        int result = sqlite3_step(stmt);
        if (result == SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            rmbrl_log(RMBRL_LOG_ERROR, "Failed to remember memory to delete: %s\n",
                      sqlite3_errmsg(db));
            return 1;
        }

        id = sqlite3_column_int(stmt, 0);
        if (cmd->verbosity == RMBRL_VERB_VERBOSE)
        {
            rmbrl_log(RMBRL_LOG_INFO, "Found Memory:\n");
            const unsigned char *task = sqlite3_column_text(stmt, 1);
            rmbrl_log(RMBRL_LOG_INFO, "    \"%s\"", task);

            const char *project = (char *)sqlite3_column_text(stmt, 2);
            if (strlen(project) > 0)
                fprintf(stderr, " -- %s", project);

            char *created_at = rmbrl_strdup((char *)sqlite3_column_text(stmt, 3));
            created_at[10] = '\0';
            fprintf(stderr, " -- %s", created_at);
            RMBRL_FREE(created_at);

            fprintf(stderr, "\n");
        }
        sqlite3_finalize(stmt);
    }

    if (id == -1)
    {
        if (cmd->project)
            raw_stmt = "DELETE FROM memories WHERE project = ? RETURNING *;";
        else
            raw_stmt = "DELETE FROM memories RETURNING *;";
    }
    else
    {
        raw_stmt = "DELETE FROM memories WHERE id = ? RETURNING *;";
    }

    if (sqlite3_prepare_v2(db, raw_stmt, -1, &stmt, NULL) != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (id != -1)
        sqlite3_bind_int(stmt, 1, id);
    else if (cmd->project)
        sqlite3_bind_text(stmt, 1, cmd->project, -1, SQLITE_STATIC);

    if (cmd->verbosity == RMBRL_VERB_VERBOSE)
    {
        char *stmt_str = sqlite3_expanded_sql(stmt);
        rmbrl_log(RMBRL_LOG_INFO, "Query: %s\n", stmt_str);
    }

    if (cmd->verbosity != RMBRL_VERB_SILENT)
        rmbrl_log(RMBRL_LOG_INFO, "Forgotten Memories:\n");

    int result;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        // int id = sqlite3_column_int(stmt, 0);

        const unsigned char *task = sqlite3_column_text(stmt, 1);
        rmbrl_log(RMBRL_LOG_INFO, "    \"%s\"", task);

        const char *project = (char *)sqlite3_column_text(stmt, 2);
        if (strlen(project) > 0)
            fprintf(stderr, " -- %s", project);

        if (cmd->verbosity == RMBRL_VERB_VERBOSE)
        {
            char *created_at = rmbrl_strdup((char *)sqlite3_column_text(stmt, 3));
            created_at[10] = '\0';
            fprintf(stderr, " -- %s", created_at);
            RMBRL_FREE(created_at);
        }

        fprintf(stderr, "\n");
    }
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to forget memories: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (cmd->dry_run)
    {
        result = rmbrl_db_rollback_transaction(db, cmd->verbosity);
        if (result != 0)
            return result;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        rmbrl_print_help();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    {
        rmbrl_print_help();
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
        printf("remembrall " RMBRL_VERSION "\n");
        return 0;
    }

    Rmbrl_Command cmd = {0};

    if (strcmp(argv[1], "add") == 0)
        cmd.function = RMBRL_CMD_ADD;
    else if (strcmp(argv[1], "peek") == 0)
        cmd.function = RMBRL_CMD_PEEK;
    else if (strcmp(argv[1], "clear") == 0)
        cmd.function = RMBRL_CMD_CLEAR;

    if (cmd.function == RMBRL_CMD_UNKNOWN)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Unknown command '%s'\n", argv[1]);
        rmbrl_print_help();
        return 1;
    }

    Rmbrl_DA_Strs ignored_flags = {0};
    for (int i = 2; i < argc; ++i)
    {
        if ((strcmp(argv[i], "--all") == 0 || strcmp(argv[i], "-a") == 0) &&
            cmd.function != RMBRL_CMD_ADD)
        {
            cmd.all = true;
            continue;
        }
        if (strcmp(argv[i], "--dry-run") == 0 || strcmp(argv[i], "-n") == 0)
        {
            cmd.dry_run = true;
            continue;
        }
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
        {
            cmd.verbosity = RMBRL_VERB_VERBOSE;
            continue;
        }
        if (strcmp(argv[i], "--silent") == 0 || strcmp(argv[i], "-s") == 0)
        {
            cmd.verbosity = RMBRL_VERB_SILENT;
            continue;
        }
        // support "-p=name" and "-p name"
        // support "--project=name" and "--project name"
        if (strncmp(argv[i], "-p", 2) == 0 || strncmp(argv[i], "--project", 9) == 0)
        {
            if (argv[i][2] == '=')
            {
                cmd.project = argv[i] + 3;
                continue;
            }
            if (argv[i][9] == '=')
            {
                cmd.project = argv[i] + 10;
                continue;
            }

            // assume cmd is in format of "--project name"

            // bounds check and check name was provided
            // ex:
            //   correct: rmbrl peek -p name
            //   wrong  : rmbrl peek -p --all
            //   wrong  : rmbrl peek -p -v
            if (++i < argc && argv[i][0] != '-')
            {
                cmd.project = argv[i];
                continue;
            }
            rmbrl_log(RMBRL_LOG_ERROR, "Project flag provided but missing project name\n");
            return 1;
        }

        // since no match above, assume first unknown arg not prefixed with '-' is the task
        // description.
        //
        // EX:
        //  rmbrl add "this is a test" banana
        //  rmbrl add "this is a test" -t
        //  rmbrl add --banana "this is a test" "description 2"
        //
        //  for all of these "this a test" will be the task description
        //
        //  rmbrl add orange "this is a test" --banana
        //
        //  orange will be the task since it is the first uknown arg not prefixed with '-'
        if (cmd.function == RMBRL_CMD_ADD && cmd.task == NULL && argv[i][0] != '-')
        {
            cmd.task = argv[i];
            continue;
        }

        rmbrl_da_append(&ignored_flags, argv[i]);
    }

    if (cmd.verbosity != RMBRL_VERB_SILENT && ignored_flags.count > 0)
    {
        rmbrl_log(RMBRL_LOG_WARNING, "Ignoring flags: ");
        for (size_t i = 0; i < ignored_flags.count; ++i)
        {
            fprintf(stderr, "%s", ignored_flags.items[i]);
            if (i < ignored_flags.count - 1)
                fprintf(stderr, ", ");
        }
        fprintf(stderr, "\n");
    }
    rmbrl_da_free(ignored_flags);

    if (cmd.verbosity == RMBRL_VERB_VERBOSE)
        rmbrl_command_debug_print(&cmd);

    if (cmd.function == RMBRL_CMD_ADD && cmd.task == NULL)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Running \"add\" command but missing task description\n");
        return 1;
    }

    char db_path[512];
#ifdef _WIN32
    char *appdata = getenv("APPDATA");
    if (appdata == NULL)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "APPDATA environment variable not found!\n");
        return 1;
    }
    snprintf(db_path, sizeof(db_path), "%s\\rmbrl\\", appdata);
    int result = mkdir(db_path);
#elif defined(__APPLE__) && defined(__MACH__)

    char *home_env = getenv("HOME");
    if (home_env == NULL)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "HOME environment variable not found!\n");
        return 1;
    }
    snprintf(db_path, sizeof(db_path), "%s/Library/Application Support/rmbrl/", home_env);
    int result = mkdir(db_path, 0755);
#elif defined(__linux__)
    char *home_env = getenv("HOME");
    if (home_env == NULL)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "HOME environment variable not found!\n");
        return 1;
    }
    snprintf(db_path, sizeof(db_path), "%s/.local/share/rmbrl/", home_env);
    int result = mkdir(db_path, 0755);
#else
    rmbrl_log(RMBRL_LOG_ERROR, "Running on an unknown operating system.\n");
    return 1;
#endif

    // NOTE: db_path will already exist if user provided --install flag when building remembrall
    if (result == -1 && errno != EEXIST)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Failed to create path: %s\nReason: %s\n", db_path,
                  strerror(errno));
        return 1;
    }
    strncat(db_path, "rmbrl.db", 9);

    if (cmd.verbosity == RMBRL_VERB_VERBOSE)
        rmbrl_log(RMBRL_LOG_INFO, "DB Path: %s\n", db_path);

    sqlite3 *db;
    result = sqlite3_open(db_path, &db);
    if (result != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "Databased connection failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    if (cmd.verbosity == RMBRL_VERB_VERBOSE)
        rmbrl_log(RMBRL_LOG_INFO, "Database connection successful!\n");

    char *err_msg;
    char *statement = "CREATE TABLE IF NOT EXISTS memories("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                      "task TEXT NOT NULL,"
                      "project TEXT DEFAULT '' NOT NULL,"
                      "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL);";

    result = sqlite3_exec(db, statement, NULL, 0, &err_msg);
    if (result != SQLITE_OK)
    {
        rmbrl_log(RMBRL_LOG_ERROR, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        RMBRL_CLEANUP_RETURN(1);
    }

    if (cmd.verbosity == RMBRL_VERB_VERBOSE)
        rmbrl_log(RMBRL_LOG_INFO, "Table \"memories\" exists or created successfully!\n");

    switch (cmd.function)
    {
    case RMBRL_CMD_ADD:
        result = rmbrl_command_add(&cmd, db);
        break;
    case RMBRL_CMD_PEEK:
        result = rmbrl_command_peek(&cmd, db);
        break;
    case RMBRL_CMD_CLEAR:
        result = rmbrl_command_clear(&cmd, db);
        break;
    default:
        RMBRL_UNREACHABLE("Command function type");
        break;
    }

cleanup:
    sqlite3_close(db);
    return result;
}
