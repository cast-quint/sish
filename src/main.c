/*
 * Dimitrios Koropoulis 3967
 * csd3967@csd.uoc.gr
 * CS345 - Fall 2020
 * main.c
 */

#include  "sish.h"

extern struct sigaction action;
extern sig_atomic_t child_shell_pid;

int main(void) {

	struct command* head = NULL;
	char buff[ARG_MAX];
	int status;
    pid_t child_shell_pid;

	setup_parent_shell_sa();
	setup_terminal(FALSE);

    while (TRUE) {

		child_shell_pid = fork();

		if (child_shell_pid == 0) {
			setup_child_shell_sa();

			while (TRUE) {

				print_prompt();
				read_input(buff, ARG_MAX);
				head = parse_input(buff);

				sigprocmask(SIG_BLOCK, &action.sa_mask, NULL);

				if (head == NULL) continue;


				if (!strcmp(head->name, "cd")) {
					execute_cd(head->argv);
				} else if (!strcmp(head->name, "exit")) {
					free_command_list(head);
					exit(EXIT_SUCCESS);
				} else {
					execute(head);
				}

				free_command_list(head);
				head = NULL;

				sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL);
			}
		} else {
			wait(&status);

			if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SIGINT) {
				printf("\n");
				continue;
			} else {
				setup_terminal(TRUE);
				exit(EXIT_SUCCESS);
			}
		}
	}

	return 0;
}

