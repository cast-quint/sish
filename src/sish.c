/*
 * Dimitrios Koropoulis 3967
 * csd3967@csd.uoc.gr
 * CS345 - Fall 2020
 * sish
 */

#include "sish.h"

struct sigaction action;
sig_atomic_t child_shell_pid;

/* Quick and dirty fix to innit the pipe_prev pointers, as I added them MUCH later */
static void fix_pipe_prev_pointers(struct command* head) {
	struct command* current = NULL;

	current = head;

	while (current != NULL) {
		if (current->pipe_next != NULL) {
			current->pipe_next->pipe_prev = current;
		}
		current = current->pipe_next;
	}
}

/* Functions to control the char parsing */
static int is_valid(char c) {

	return (c != ' ') && (c != '\n')
	    && (c != '\0') && ( c != EOF);
}

static int is_terminating(char c) {
	return (c == '\n') || (c == '\0') || (c == EOF);
}

static int is_special(char c) {
	return (c == PIPE_CH) || (c == FILE_CH);
}

/* Used while parsing, to figure the exact file operation that is needed */
static int resolve_file_op(const char* buff, int index) {
	if (buff[index] == FILE_CH) {
		if (buff[index + 1] == FILE_CH) {
			if (buff[index + 2] == FILE_CH) {
				return F_AP_INDEX;
			}
			return F_WR_INDEX;
		}
		return F_RD_INDEX;
	}

	return -1;
}

static int is_absolute_path(const char* buff, int start_index, int length) {
	int is_abs = FALSE;
	int i;
	for (i = start_index; i < length; i++) {
		if (buff[i] == '/') is_abs = TRUE;
	}

	return is_abs;
}

/* Get the number of commands to be executed, size of the list basically */
static int get_command_count(struct command* current) {
	size_t command_count = 0;

	while (current != NULL) {
		command_count++;
		current = current->pipe_next;
	}

	return command_count;
}

/*
 * Parse the argument count of a command in the buffer
 * starting from buff[start_arg_index]
 */
static int get_argument_count(const char* buff, size_t start_arg_index) {

	size_t argc = 0;
	int in_argument_scope = FALSE;

	if (buff == NULL) {
		fprintf(stderr, "Error in get_argument_count: null buffer!");
		return -1;
	}

	/* Basically an FSM */
	while (!is_terminating(buff[start_arg_index]) && !is_special(buff[start_arg_index])) {

		if (buff[start_arg_index] == ' ') {
			in_argument_scope = FALSE;
		} else if (!in_argument_scope) {
			in_argument_scope = TRUE;
			argc++;
		}

		start_arg_index++;
	}

	return argc;
}

/* Create a new command struct and return it */
static struct command* new_command(const char* buff, size_t command_start_index, size_t name_len, const size_t argc) {
	struct command* new = NULL;
	int name_start_arg_index = 0;
	int i;

	/*if(!is_absolute_path(buff, command_start_index, name_len)) {
		name_len += strlen(BIN_PATH);
	}*/

	new = malloc(sizeof(struct command));
	new->name = malloc(sizeof(char) * (name_len + 1));
	new->name[0] = '\0';

	new->pipe_next = NULL;
	new->pipe_prev = NULL;
	new->file[F_RD_INDEX] = NULL;
	new->file[F_WR_INDEX] = NULL;
	new->file[F_AP_INDEX] = NULL;

	/*if (!is_absolute_path(buff, command_start_index, name_len)) {
		strncat(new->name, BIN_PATH, name_len * sizeof(char));
		name_start_arg_index = strlen(new->name);
	}*/

	for (i = name_start_arg_index; i < name_len; i++) {
		new->name[i] = buff[command_start_index++];
	}

	new->name[name_len] = '\0';

	new->argv = malloc((argc + 1) * sizeof(char *));
	new->argv[0] = new->name;
	for (i = 1; i <= argc; i++) {
		new->argv[i] = NULL;
	}

	return new;
}

/* Get a new filename from the input buffer and write it to a command */
static void new_file(const char* buff, size_t file_start_index, int file_index, struct command* command, size_t file_len) {
	int write_offset = 0;
	char* temp = NULL;
	int i;

	if (file_index == -1 || command->file[file_index] != NULL) {
		fprintf(stderr, "%s:%s\n", SHELL_NAME, "File parsing error");
	}

	/* Trying to get the absolute filename...it's a bit complicated */
	if (!is_absolute_path(buff, file_start_index, file_len)) {
		temp = getcwd(NULL, 0);
		file_len += strlen(temp) + 1;
		command->file[file_index] = malloc((file_len + 1) * sizeof(char));
		command->file[file_index][0] = '\0';
		strcat(command->file[file_index], temp);
		command->file[file_index][strlen(temp)] = '/';
		write_offset = strlen(temp) + 1;
		free(temp);
	} else {
		command->file[file_index] = malloc((file_len + 1) * sizeof(char));
	}

	for (i = 0 + write_offset; i < file_len; i++) {
		command->file[file_index][i] = buff[file_start_index++];
	}
	command->file[file_index][file_len] = '\0';
}

/* Get a new command argument from the input buffer and write it to the argv of command */
static void new_arg(const char* buff, size_t arg_start_index, struct command* command, size_t arg_len, size_t arg_count) {
	int arg_index = 0;
	int i;

	if (command == NULL) return;

	while (arg_index < arg_count && command->argv[arg_index] != NULL) {
		arg_index++;
	}

	if (arg_index == arg_count) return;

	command->argv[arg_index] = malloc((arg_len + 1) * sizeof(char));
	for (i = 0; i < arg_len; i++) {
		command->argv[arg_index][i] = buff[arg_start_index++];
	}
	command->argv[arg_index][arg_len] = '\0';
}

/*
 * Recursively parse every command from input buffer,
 * its arguments and possible file and pipe redirections,
 * and organise them in a doubly linked list. Returns the head of list.
 */
static struct command* generate_command_list(const char* buff, int buff_index) {

	struct command* command = NULL;

	int parsed_command_name = FALSE;
	int parsing_filename = FALSE;

	int file_index = -1;
	int arg_count = 0;
	size_t token_len = 0;
	int token_start = 0;


	if (buff == NULL) {
		fprintf(stderr, "%s/n", "Error in generate_command_string: null buffer!");
		return NULL;
	}

	while (!is_terminating(buff[buff_index])) {

		/* Skip any whitespace padding */
		if (buff[buff_index] == ' ' || buff[buff_index] == '\t') {
			buff_index++;
			continue;
		}

		/* If we encounter a 'special' char... */
		if (is_special(buff[buff_index])) {

			/* 	...and we haven't parsed the command name or we are parsing a filename, the input is invalid. */
			if (!parsed_command_name || parsing_filename) {
				fprintf(stderr, "%s:%s \'%c\'\n", SHELL_NAME, "parse error near", buff[buff_index]);
				free_command_list(command);
				return NULL;
			}

			/*
			 ...and the command name has been parsed OR we aren't parsing a filename
			 we now move to parse:
			 - A 2nd command       - if pipe char
			 - A file to work with - if file char
			*/
			if (buff[buff_index] == PIPE_CH) {
				buff_index++;
				command->pipe_next = generate_command_list(buff, buff_index);
				if (command->pipe_next == NULL) {
					free_command_list(command);
					return NULL;
				}

					return command;
			} else if (!parsing_filename) {
				file_index = resolve_file_op(buff, buff_index);
				buff_index = buff_index + file_index + 1;
				parsing_filename = TRUE;
				continue;
			}
		}


		token_start = buff_index;
		while (is_valid(buff[buff_index])) buff_index++;
		token_len = buff_index - token_start;

		if (!parsed_command_name) {
			arg_count = get_argument_count(buff, token_start);
			command = new_command(buff, token_start, token_len, arg_count);
			parsed_command_name = TRUE;
		} else if (parsing_filename) {
			new_file(buff, token_start, file_index, command, token_len);
			parsing_filename = FALSE;
			file_index = -1;
		} else {
			new_arg(buff, token_start, command, token_len, arg_count);
		}
	}

	return command;
}

/*
 *	Read the input from the shell. Flushes chars exciding the input limit.
 *	Does NOT block signals, as the user may SIGINT while typing the command.
 */
int read_input(char *buff, size_t size) {

	int temp = 0;

	if (buff == NULL) {
		fprintf(stderr, "Error in read_input: input buffer is NULL!\n");
		return ERROR;
	}

	fgets(buff, size, stdin);

	/* If input is larger than allowed, flush the */
	/* remaining chars as to not affect later inputs */
	if (buff[strlen(buff) - 1] != '\n') {
		while ((temp = getchar()) != '\n' && temp != EOF);
	}

	return 0;
}

/* Prints the shell prompt */
void print_prompt(void) {
	sigprocmask(SIG_BLOCK, &action.sa_mask, NULL);

	struct passwd *p = getpwuid(getuid());
	char path_buff[PATH_SIZE];

	getcwd(path_buff, PATH_SIZE);

	if (!p) {
		puts("Error: Can't get user name or ID.");
		exit(EXIT_FAILURE);
	}

	printf("\033[1;32m");
	printf("[%s] [%s] $ ", p->pw_name, path_buff);
	printf("\033[0m");

	sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL);
}

/*
 * Flushes all the unclosed pipe file descriptors.
 * Used when needed to exit from execution of a pipe of commands unexpectedly
 */
static void flush_pipes(int* pipefd, int command_number, size_t command_count) {
	int i;
	for (i = command_number; i < command_count; i++) {
		if (i > 0) {
			close(pipefd[2* (i - 1)]); //INPUT
		}

		if (i < command_count - 1) {
			close(pipefd[2*i + 1]); //OUTPUT
		}

	}
}

/* Frees the list pointed to by head, and exits with status */
static void custom_exit(int status, struct command* head) {
	free_command_list(head);
	exit(status);
}

/* Set up the controlling shell to ignore SIGINTS */
void setup_parent_shell_sa(void) {
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);

	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);
}

/* Handler of SIGINT for all the childs of the controlling shell, including the child shell */
void child_shell_sigint_handler(int signum) {
	exit(EXIT_SIGINT);
}

/* Set child shell SIGINT handler */
void setup_child_shell_sa(void) {
	sigprocmask(SIG_BLOCK, &action.sa_mask, NULL);
	action.sa_handler = child_shell_sigint_handler;
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);
	sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL);
}

/* Set up the functionality of the other terminal-related signals */
void setup_terminal(int reset_del) {

    struct termios terminal;
	int termfd;

	termfd = open("/dev/tty", O_RDWR);

	tcgetattr(termfd, &terminal);

	if (reset_del) {
		terminal.c_cc[VERASE] = 0x7F;  /* DEL */
	} else {
	    /* FIXME: doent work, don't know why */
        terminal.c_cc[VERASE] = 0x08;  /* BS */
    }


    terminal.c_cc[VSTOP]  = 0x13;      /* ^S */
	terminal.c_cc[VSTART] = 0x11;      /* ^Q */

	tcsetattr(termfd, TCSANOW, &terminal);

	close(termfd);
}



/* Free all allocated memory used in the command list */
void free_command_list(struct command* head) {
	struct command* temp = NULL;
	int i;

	while (head != NULL) {
		temp = head->pipe_next;

		free(head->name);

		for (i = 1; head->argv[i] != NULL; i++) {
			free(head->argv[i]);
		}

		free(head->argv);

		free(head->file[F_RD_INDEX]);
		free(head->file[F_WR_INDEX]);
		free(head->file[F_AP_INDEX]);

		free(head);


		head = temp;
	}
}

/*
 * Wrapper function for parsing the input from the shell.
 * Calls generate_command_list and fixes the prev_pipe pointers.
 * Blocks any signals.
 */
struct command* parse_input(const char* buff) {
	sigprocmask(SIG_BLOCK, &action.sa_mask, NULL);

	struct command* head = NULL;

	head = generate_command_list(buff, 0);
	fix_pipe_prev_pointers(head);

	sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL);

	return head;
}

/*
 * For every command in the pipe, forks a child, sets up file descriptors as needed and executes the command.
 * Parent just forks and waits. Also, responds to SIGINTS and failure exits.
 */
int execute(struct command* head) {

	struct command* command = head;
	size_t command_count = get_command_count(command);
	size_t pipe_count = command_count - 1;
	size_t pipefd_count = 2 * pipe_count;
	int pipefd[pipefd_count];
	int status;
	int pid;
	int fd;
	int i;

	int fd_stream[3] = {
		STDIN_FILENO,
		STDOUT_FILENO,
		STDOUT_FILENO
	};
	int fd_flags[3] = {
		O_RDONLY,
		O_WRONLY | O_CREAT | O_TRUNC,
		O_WRONLY | O_CREAT | O_APPEND
	};

	/* init all the needed pipes */
	for (i = 0; i < pipe_count; i++) {
		pipe(pipefd + 2 * i);
	}

	for (i = 0; i < command_count; command = command->pipe_next, i++) {
		pid = fork();

		if (pid == -1) {
			flush_pipes(pipefd, i, pipe_count);
			return EXIT_FAILURE;
		}

		if (pid == 0) {
			sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL);

			if (command->file[F_RD_INDEX] != NULL && command->pipe_prev != NULL) {
				fprintf(stderr, "%s: %s: Multiple input targets\n", SHELL_NAME, command->name);
				custom_exit(EXIT_FAILURE, head);
			}

			if ((command->file[F_WR_INDEX] != NULL || command->file[F_AP_INDEX] != NULL) && command->pipe_next != NULL) {
				fprintf(stderr, "%s: %s: Multiple output targets\n", SHELL_NAME, command->name);
				custom_exit(EXIT_FAILURE, head);
			}

			/* Dupe file descriptors to std streams, if needed */
			for (i = 0; i < 3; i++) {
				if (command->file[i] != NULL) {
					fd = open(command->file[i], fd_flags[i], 0666);
					if (fd == -1) {
						perror(command->file[i]);
						custom_exit(EXIT_FAILURE, head);
					}

					if (dup2(fd, fd_stream[i]) == -1) {
						perror(command->file[i]);
						custom_exit(EXIT_FAILURE, head);
					}

					close(fd);
				}
			}


			/* Dupe pipe write to stdout, if needed */
			if (command->pipe_next != NULL) {
				if (dup2(pipefd[2*i + 1], STDOUT_FILENO) == -1) {
					fprintf(stderr, "%s", command->name);
					perror("dup2 error");
					custom_exit(EXIT_FAILURE, head);
				}
			}

			/* Dupe pipe read to stdin, if needed */
			if (command->pipe_prev != NULL) {
				if (dup2(pipefd[2 * (i - 1)], STDIN_FILENO) == -1) {
					fprintf(stderr, "%s", command->name);
					perror("dup2 error");
					custom_exit(EXIT_FAILURE, head);
				}
			}

			execvp(command->name, command->argv);
			perror(command->name);
			custom_exit(EXIT_FAILURE, head);
		} else if (pid != 0) {
			wait(&status);

			if (i > 0) {
				close(pipefd[2* (i - 1)]);
			}

			if (i < command_count - 1) {
				close(pipefd[2*i + 1]);
			}

			if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE) {
				flush_pipes(pipefd, i, pipe_count);
				return EXIT_FAILURE;
			}

			if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SIGINT) {
				flush_pipes(pipefd, i, pipe_count);
				return EXIT_SIGINT;
			}

			if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) {
				flush_pipes(pipefd, i, pipe_count);
				return EXIT_SIGINT;
			}
		}
	}

	return EXIT_SUCCESS;
}


/* Execute the build-in shell command cd */
int execute_cd(char** argv) {
	char* dir = NULL;

	if (argv[1] != NULL) {
		if (argv[2] != NULL) {
			fprintf(stderr, "%s: %s\n", "cd", "too many arguments");
			return -1;
		}

		dir = argv[1];
	} else {
		dir = getpwuid(getuid())->pw_dir;
	}

	return chdir(dir);
}

