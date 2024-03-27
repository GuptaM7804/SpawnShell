/* $begin shellmain */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <fcntl.h>

#define MAXARGS 128
#define MAXLINE 8192 /* Max text line length */

typedef enum { 
    IS_SIMPLE,
    IS_PIPE,
    IS_INPUT_REDIR,
    IS_OUTPUT_REDIR,
    IS_INPUT_OUTPUT_REDIR,
    IS_SEQ,
    IS_ANDIF,
    IS_ORIF
} Mode; /* simple command, |, >, <, ;, && */

typedef struct { 
    char *argv[MAXARGS]; /* Argument list */
    int argc; /* Number of args */
    int bg; /* Background job? */
    Mode mode; /* Handle special cases | > < ; */
} parsed_args; 

extern char **environ; /* Defined by libc */

/* Function prototypes */
void unix_error(char *msg);
void eval(char *cmdline);
parsed_args parseline(char *buf);
int builtin_command(char **argv, pid_t pid, int status);
void signal_handler(int sig);
int exec_cmd(char **argv, posix_spawn_file_actions_t *actions, pid_t *pid, int *status, int bg);
int find_index(char **argv, char *target); 

int main(int argc, char *argv[])
{
    char cmdline[MAXLINE]; /* Command line */

    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGCHLD, signal_handler);

    while (1) {
        char *result;
        /* Read */
        printf("CS361 >");
        result = fgets(cmdline, MAXLINE, stdin);
        if (result == NULL && ferror(stdin)) {
            fprintf(stderr, "fatal fgets error\n");
            exit(EXIT_FAILURE);
        }

        if (feof(stdin))
            exit(EXIT_SUCCESS);

        /* Evaluate */
        eval(cmdline);
    }
}

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char buf[MAXLINE];   /* Holds modified command line */
    pid_t pid;           /* Process id */
    int status;          /* Process status */
    posix_spawn_file_actions_t actions; /* used in performing spawn operations */
    posix_spawn_file_actions_init(&actions);
    int pipe_fds[2];
    pipe(pipe_fds); 

    strcpy(buf, cmdline);
    parsed_args parsed_line = parseline(buf);
    if (parsed_line.argv[0] == NULL) /* Ignore empty lines */
        return; 

    /* Not a bultin command */
    if (!builtin_command(parsed_line.argv, pid, status)) {
        switch (parsed_line.mode) {
        case IS_SIMPLE: /* cmd argv1  argv2 ... */
            if (!exec_cmd(parsed_line.argv, &actions, &pid, &status, parsed_line.bg)) 
                return;
            break;
        case IS_PIPE: ; /* command1 args | command2 args */
	    // TODO: handle pipe
		int pid1, pid2, child_status;
		posix_spawn_file_actions_t actions1, actions2;
	    	int i = find_index(parsed_line.argv, "|");
	    	posix_spawn_file_actions_init(&actions1);
	    	posix_spawn_file_actions_init(&actions2);
	    	posix_spawn_file_actions_adddup2(&actions1, pipe_fds[1], STDOUT_FILENO);
	    	posix_spawn_file_actions_addclose(&actions1, pipe_fds[0]);
	    	posix_spawn_file_actions_adddup2(&actions2, pipe_fds[0], STDIN_FILENO);
	    	posix_spawn_file_actions_addclose(&actions2, pipe_fds[1]);
	    	parsed_line.argv[i] = NULL;
	    	if (0 != posix_spawnp(&pid1, parsed_line.argv[0], &actions1, NULL, parsed_line.argv, environ))
			perror("spawn failed");
	    	if (0 != posix_spawnp(&pid2, parsed_line.argv[i+1], &actions2, NULL, &parsed_line.argv[i+1], environ))
              		perror("spawn failed");
	    	close(pipe_fds[0]);
	    	close(pipe_fds[1]);
	    	waitpid(pid1, &child_status, 0);
	    	waitpid(pid2, &child_status, 0);
	    	printf("pipe not yet implemented :(\n");
            	break;
        case IS_OUTPUT_REDIR: ; /* command args > output_redirection */
            // TODO: handle output redirection
	    i = find_index(parsed_line.argv, ">");
	    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, parsed_line.argv[i+1], S_IRWXU | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	    if (0 != posix_spawnp(&pid, parsed_line.argv[0], &actions, NULL, parsed_line.argv, environ))
		    perror("spawn failed");
	    wait(&status);
            break;
        case IS_INPUT_REDIR: ; /* command args < input_redirection */
            // TODO: handle input redirection
	    i = find_index(parsed_line.argv, "<");
	    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, parsed_line.argv[i+1], O_RDONLY, S_IWUSR);
	    if (0 != posix_spawnp(&pid, parsed_line.argv[0], &actions, NULL, parsed_line.argv, environ))
	    		perror("spawn failed");
	    wait(&status); 
            printf("input redirection not yet implemented :(\n");
            break;
        case IS_INPUT_OUTPUT_REDIR: /* command args < input_redirection > output_redirection */
            // TODO: handle input output redirection
	    i = find_index(parsed_line.argv, ">");
	    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, parsed_line.argv[i+1], O_WRONLY, S_IWUSR);
	    if (0 != posix_spawnp(&pid, parsed_line.argv[0], &actions, NULL, parsed_line.argv, environ))
	    	perror("spawn failed");
	    wait(&status); 
            printf("input output redirection not yet implemented :(\n");
            break;
        case IS_SEQ: /* command1 args ; command2 args */
            // TODO: handle sequential
	    i = find_index(parsed_line.argv, ";");
	    posix_spawn_file_actions_init(&actions1);
	    posix_spawn_file_actions_init(&actions2);
	    posix_spawn_file_actions_adddup2(&actions1, pipe_fds[1], STDOUT_FILENO);
	    posix_spawn_file_actions_addclose(&actions1, pipe_fds[0]);
	    posix_spawn_file_actions_adddup2(&actions2, pipe_fds[0], STDIN_FILENO);
	    posix_spawn_file_actions_addclose(&actions2, pipe_fds[1]);
	    parsed_line.argv[i] = NULL;
	    if (0 != posix_spawnp(&pid1, parsed_line.argv[0], &actions1, NULL, parsed_line.argv, environ))
	  	  perror("spawn failed");
	    if (0 != posix_spawnp(&pid2, parsed_line.argv[i+1], &actions2, NULL, &parsed_line.argv[i+1], environ))
	   	 perror("spawn failed");
	    close(pipe_fds[0]);
	    close(pipe_fds[1]);
	    waitpid(pid1, &child_status, 0);
	    waitpid(pid2, &child_status, 0);			
            printf("sequential commands not yet implemented :(\n");
            break;
        case IS_ANDIF: /* command1 args && command2 args */
            // TODO: handle "and if"
            printf("composed && commands not yet implemented :(\n");
            break;
        case IS_ORIF: /* command1 args || command2 args */
            // TODO: handle "or if"
            printf("composed || commands not yet implemented :(\n");
            break;
        }

        if (parsed_line.bg)
            printf("%d %s", pid, cmdline);
    }

    posix_spawn_file_actions_destroy(&actions);
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, pid_t pid, int status)
{
    if (!strcmp(argv[0], "exit")) /* exit command */
        exit(EXIT_SUCCESS);
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;
    if (!strcmp(argv[0], "?")) {
	printf("\npid:\%d status:\%d\n;", pid, status);
    }
    return 0; /* Not a builtin command */
}
/* $end eval */

/* Run commands using posix_spawnp */
int exec_cmd(char **argv, posix_spawn_file_actions_t *actions, pid_t *pid, int *status, int bg)
{
    int ret = posix_spawnp(pid, argv[0], actions, NULL, argv, environ);
    if (0 != ret) {
	    perror("spawn failed");
    }
    
    if (!bg) {
        if (waitpid(*pid, status, 0) < 0) 
            unix_error("waitfg: waitpid error");
    }

    return 1;
}
/* $end exec_cmd */

/* signal handler */
void signal_handler(int sig)
{
    if (sig == SIGINT) {
	    printf("\ncaught sigint\nCS361 >");
    }
    else if (sig == SIGTSTP) {
            printf("\ncaught sigtstp\nCS361 >");
    }
    else if (sig == SIGCHLD) {
    	    printf("CS361 >");
	    int sig_err = errno;
	    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
	    errno = sig_err;
    }
}

/* finds index of the matching target in the arguments */
int find_index(char **argv, char *target)
{
    for (int i = 0; argv[i] != NULL; i++) {
        if (!strcmp(argv[i], target))
            return i;
    }
    
    return 0;
}

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
parsed_args parseline(char *buf)
{
    char *delim; /* Points to first space delimiter */
    parsed_args pa;

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) { /* Ignore leading spaces */
        buf++;
    } 

    /* Build the argv list */
    pa.argc = 0;
    while ((delim = strchr(buf, ' '))) {
        pa.argv[pa.argc++] = buf;
        *delim = '\0';
        buf = delim + 1;

        while (*buf && (*buf == ' ')) { /* Ignore spaces */
            buf++;
        }
    }
    pa.argv[pa.argc] = NULL;

    if (pa.argc == 0) /* Ignore blank line */
        return pa;

    /* Should the job run in the background? */
    if ((pa.bg = (*pa.argv[pa.argc - 1] == '&')) != 0)
        pa.argv[--pa.argc] = NULL;

    /* Detect various command modes */
    pa.mode = IS_SIMPLE;
    if (find_index(pa.argv, "|"))
        pa.mode = IS_PIPE;
    else if (find_index(pa.argv, ";")) 
        pa.mode = IS_SEQ; 
    else if (find_index(pa.argv, "&&"))
        pa.mode = IS_ANDIF;
    else if (find_index(pa.argv, "||"))
        pa.mode = IS_ORIF;
    else {
        if (find_index(pa.argv, "<")) 
            pa.mode = IS_INPUT_REDIR;
        if (find_index(pa.argv, ">")) {
            if (pa.mode == IS_INPUT_REDIR)
                pa.mode = IS_INPUT_OUTPUT_REDIR;
            else
                pa.mode = IS_OUTPUT_REDIR; 
        }
    }

    return pa;
}
/* $end parseline */
