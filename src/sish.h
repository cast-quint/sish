/*
 * Dimitrios Koropoulis 3967
 * csd3967@csd.uoc.gr
 * CS345 - Fall 2020
 * sish.h
 */

#ifndef SISH_H
#define SISH_H

#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <pwd.h>

#define BIN_PATH 	 "/usr/bin/"
#define SHELL_NAME 	 "sish"
#define PATH_SIZE 	 1024
#define ARG_MAX		 1024

#define TRUE 		 1
#define FALSE		 0

#define PIPE_CH     '>'
#define FILE_CH     '|'

#define F_RD_INDEX	 0
#define F_WR_INDEX	 1
#define F_AP_INDEX	 2

#define EXIT_SIGINT  3
#define ERROR 		-1


struct command {
	char* name;
	char** argv;

	char* file[3];
	struct command* pipe_next;
	struct command* pipe_prev;
};

void child_shell_sigint_handler(int signum);
void setup_parent_shell_sa(void);
void setup_child_shell_sa(void);
void setup_terminal(int reset_del);

int read_input(char *buff, size_t size);
void print_command_list(struct command* head);
struct command* parse_input(const char* buff);
void free_command_list(struct command* head);
int execute(struct command* head);
int execute_cd(char** argv);
void print_prompt(void);

#endif /* SISH_H */

