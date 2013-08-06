#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDIO_H
#include <stdio.h>
#endif /* HAVE_STDIO_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif /* HAVE_SIGNAL_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include "jdb_tgdb.h"
#include "fork_util.h"
#include "fs_util.h"
#include "pseudo.h"
#include "io.h"
#include "tgdb_types.h"
#include "queue.h"
#include "sys_util.h"
#include "ibuf.h"

#define PATH_MAX 4096
#define TTY_NAME_SIZE 64

/**
 * This is the main context for the gdbmi subsytem.
 */
struct tgdb_jdb {

    /** 
	 * This is set when this context has initialized itself
	 */
    int tgdb_initialized;

    /** 
	 * writing to this will write to the stdin of the debugger
	 */
    int debugger_stdin;

    /**
     * Reading from reads the stdout/stderr of the debugger
	 */
    int debugger_out;

    /**
     * writing to this will write to the stdin of the inferior
	 */
    int inferior_stdin;

    /**
	 * Reading from reads the stdout/stderr of the inferior
	 */
    int inferior_out;

    /**
	 * Only kept around so it can be closed properly
	 */
    int inferior_slave_fd;

    /** 
	 * pid of child process.
	 */
    pid_t debugger_pid;

    /** 
	 * The config directory that this context can write too.
	 */
    char config_dir[PATH_MAX];

    /**
	 * The init file for the debugger.
	 */
    char jdb_init_file[PATH_MAX];

    /**
	 * The name of the inferior tty.
	 */
    char inferior_tty_name[TTY_NAME_SIZE];

    /** 
	 * This is a list of all the commands generated since in the last call. 
	 */
    struct tgdb_list *client_command_list;

    /**
	 * This is the current output command from GDB.
	 */
    struct ibuf *tgdb_cur_output_command;

    /** The master, slave and slavename of the pty device */
    pty_pair_ptr pty_pair;
};

/* initialize_gdbmi
 *
 * initializes a gdbmi subsystem and sets up all initial values.
 */
static struct tgdb_jdb *initialize_tgdb_jdb(void)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb *)
            cgdb_malloc(sizeof (struct tgdb_jdb));

    jdb->tgdb_initialized = 0;
    jdb->debugger_stdin = -1;
    jdb->debugger_out = -1;

    jdb->inferior_stdin = -1;
    jdb->inferior_out = -1;
    jdb->inferior_slave_fd = -1;
    jdb->inferior_tty_name[0] = '\0';

    /* null terminate */
    jdb->config_dir[0] = '\0';
    jdb->jdb_init_file[0] = '\0';

    return jdb;
}

/* tgdb_setup_config_file: 
 * -----------------------
 *  Creates a config file for the user.
 *
 *  Pre: The directory already has read/write permissions. This should have
 *       been checked by tgdb-base.
 *
 *  Return: 1 on success or 0 on error
 */
static int tgdb_setup_config_file(struct tgdb_jdb *jdb, const char *dir)
{
    return 1;
}

void *jdb_create_context(const char *debugger,
        int argc, char **argv, const char *config_dir, struct logger *logger)
{
    struct tgdb_jdb *jdb = initialize_tgdb_jdb();
    char* cmds[] = {
        "jdb",
        "-connect",
        "com.sun.jdi.SocketAttach:hostname=localhost,port=8000",
        NULL
    };
    char** args = &(cmds[1]);
    int args_num = 0;
    while(args[args_num] != NULL) {
        ++args_num;
    }

    char jdb_debug_file[PATH_MAX];
    fs_util_get_path(config_dir, "jdb_tgdb_debug.txt", jdb_debug_file);
    io_debug_init(jdb_debug_file);

    jdb->debugger_pid =
            invoke_debugger(cmds[0], args_num, args,
            &jdb->debugger_stdin, &jdb->debugger_out,
            1, jdb->jdb_init_file);
    jdb->tgdb_cur_output_command = ibuf_init();

    return jdb;
}

int jdb_initialize(void *ctx,
        int *debugger_stdin, int *debugger_stdout,
        int *inferior_stdin, int *inferior_stdout)
{
    struct tgdb_jdb *jdb= (struct tgdb_jdb *) ctx;

    jdb->client_command_list = tgdb_list_init();

    *debugger_stdin = jdb->debugger_stdin;
    *debugger_stdout = jdb->debugger_out;

    jdb_open_new_tty(jdb, inferior_stdin, inferior_stdout);
    jdb->tgdb_initialized = 1;
    return 0;
}

int jdb_shutdown(void *ctx)
{
    return 0;
}

int jdb_is_client_ready(void *ctx)
{
    return 1;
}

enum newlinestyle {
    JDB_NL,
    JDB_CR_NL,
    JDB_CR
};

/**
 * Checks to see if ibuf ends with the string ending.
 *
 * \param ibuf
 * The string to check the ending of
 *
 * \param style
 * represents what kind of newline is associated with this command.
 *
 * \param success
 * 1 if ibuf ends with ending, otherwise 0
 *
 * \return
 * 0 on success, or -1 on error
 */
static int ends_with_jdb_prompt(struct ibuf *ibuf, enum newlinestyle style,
        int *success)
{
    return 0;

}

/**
 * Determine's what kind of newline the string ends in.
 *
 */
static int jdb_get_newline_style(struct ibuf *ibuf, enum newlinestyle *style)
{
    return 0;
}

/* 
 * 1. Align the 'input' data into null-terminated strings that
 *    end in a newline.
 * 2. Check to see if a command has been fully recieved.
 *    2a. If it has, go to step 3
 *    2b. If it hasn't, return from the function
 * 3. parse the command.
 * 4. traverse the parse tree to populate the tgdb_list with
 *    commands the user/front end is looking for.
 */
int jdb_parse_io(void *ctx,
        const char *input_data, const size_t input_data_size,
        char *debugger_output, size_t * debugger_output_size,
        char *inferior_output, size_t * inferior_output_size,
        struct tgdb_list *list)
{
    return 0;
}

struct tgdb_list *jdb_get_client_commands(void *ctx)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb *) ctx;

    return jdb->client_command_list;
}

pid_t jdb_get_debugger_pid(void *ctx)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb*) ctx;

    return jdb->debugger_pid;
}

int jdb_user_ran_command(void *ctx)
{
    return 0;
}

int jdb_prepare_for_command(void *ctx, struct tgdb_command *com)
{
    return 0;
}

int jdb_get_current_location(void *ctx, int on_startup)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb*) ctx;
    struct tgdb_command *client_command = NULL;
    const char* cmd = "run";
    client_command = tgdb_command_create(cmd, TGDB_COMMAND_TGDB_CLIENT,
    (void *) cmd);

    tgdb_list_append(jdb->client_command_list, client_command);
    return 0;
}

/* Here are the two functions that deal with getting tty information out
 * of the annotate_two subsystem.
 */

static int close_inferior_connection(void *ctx)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb*) ctx;

    if (!jdb) {
        logger_write_pos(logger, __FILE__, __LINE__,
                "close_inferior_connection error");
        return -1;
    }

    if (jdb->pty_pair)
        pty_pair_destroy(jdb->pty_pair);

    return 0;
}

static int jdb_set_inferior_tty(void *ctx)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb*) ctx;

    if (!jdb) {
        logger_write_pos(logger, __FILE__, __LINE__,
                "jdb_set_inferior_tty error");
        return -1;
    }

    /*if (commands_issue_command(a2->c,*/
                    /*a2->client_command_list,*/
                    /*ANNOTATE_TTY,*/
                    /*pty_pair_get_slavename(a2->pty_pair), 0) == -1) {*/
        /*logger_write_pos(logger, __FILE__, __LINE__,*/
                /*"commands_issue_command error");*/
        /*return -1;*/
    /*}*/

    return 0;
}

int jdb_open_new_tty(void *ctx, int *inferior_stdin, int *inferior_stdout)
{
    struct tgdb_jdb *jdb = (struct tgdb_jdb*) ctx;

    close_inferior_connection(jdb);

    jdb->pty_pair = pty_pair_create();
    if (!jdb->pty_pair) {
        logger_write_pos(logger, __FILE__, __LINE__, "pty_pair_create failed");
        return -1;
    }

    *inferior_stdin = pty_pair_get_masterfd(jdb->pty_pair);
    *inferior_stdout = pty_pair_get_masterfd(jdb->pty_pair);

    jdb_set_inferior_tty(jdb);

    return 0;
}

int jdb_get_source_filename_pair(void *ctx, const char *file)
{
    return 0;
}

int jdb_err_msg(void *ctx) 
{
    return 0;
}

int jdb_is_misc_prompt(void *ctx)
{ 
    return 0;
}

int jdb_completion_callback(void *ctx, const char *command)
{
    return 0;
}

int jdb_get_inferior_sources(void *ctx)
{
    return 0;
}

char *jdb_return_client_command(void *ctx, enum tgdb_command_type c)
{
    return NULL;
}

char *jdb_client_modify_breakpoint(void *ctx,
        const char *file, int line, enum tgdb_breakpoint_action b)
{
    return NULL;
}

char *jdb_get_tty_name(void *ctx)
{
    return NULL;
}
