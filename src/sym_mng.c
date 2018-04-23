/*
 * sym_mng.c
 *
 *  Created on: Apr 15, 2018
 *      Author: ariel
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define SIZE_OF_FD 64
#define BUFFER_SIZE 512

typedef struct {
	int pid_num;
	int pipe_num;
} ChildProc;

ChildProc* list_of_processes = NULL;
int number_of_processes = 0;

void free_child_proc(ChildProc* ptr) {
	if (NULL != ptr) {
		free(ptr);
		ptr = NULL;
	}
}

int remove_process(ChildProc* list_of_processes, int index,
		int* number_of_processes) {
	int rc = 1;
	ChildProc temp = list_of_processes[index]; // safe- ChildProc does not contain pointers
	// close reader pipe for removed process
	if (close(temp.pipe_num) != 0) {
		// failed closing
		rc = 0;
		return rc;
	}
	list_of_processes[index] = list_of_processes[(*number_of_processes) - 1];
	list_of_processes[(*number_of_processes) - 1] = temp;
	(*number_of_processes)--;
	if (*number_of_processes == 0) { // last process going to be removed, main loop will be terminated
		return rc;
	}
	ChildProc* temp_arr = realloc(list_of_processes,
			(*number_of_processes) * sizeof(ChildProc));
	if (temp_arr == NULL) {
		printf("Failed to realloc memory\n");
		free_child_proc(list_of_processes);
		rc = 0;
	}
	list_of_processes = temp_arr;
	return rc;
}

void clean_up_remaining_processes(ChildProc* list_of_processes, int number_of_processes, int signal) {
	// kill and free all child processes
	int i = 0;
	for (; i < number_of_processes; i++) {
		close(list_of_processes[i].pipe_num); // close reader pipe of this process
		kill(list_of_processes[i].pid_num, signal);
	}
	free_child_proc(list_of_processes);
}

void signal_pipe_handler(int signum) {
	// exit gracefully
	clean_up_remaining_processes(list_of_processes, number_of_processes,
			SIGTERM);
	printf("SIGPIPE for Manager process %d. Leaving.\n", getpid());
	exit(EXIT_FAILURE);
}

int register_signal_pipe_handling() {
	struct sigaction new_pipe_action;
	memset(&new_pipe_action, 0, sizeof(new_pipe_action));
	new_pipe_action.sa_handler = signal_pipe_handler;
	return sigaction(SIGPIPE, &new_pipe_action, NULL);
}

int handle_error_and_exit(const char* error_msg) {
	clean_up_remaining_processes(list_of_processes, number_of_processes, SIGTERM);
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errno));
	return errno;
}

int main(int argc, char** argv) {
	// check number of input arguments
	if (argc < 3) {
		printf("Invalid input for the program, exiting\n");
		exit(EXIT_FAILURE);
	}

	if (register_signal_pipe_handling() != 0) {
		printf("Signal pipe handle registration failed\n");
		exit(EXIT_FAILURE);
	}

	// handle input & initialize variables
	char* path_to_file = argv[1];
	char* pattern = argv[2];
	number_of_processes = strlen(pattern);
	list_of_processes = (ChildProc*) calloc(number_of_processes, sizeof(ChildProc));
	if (NULL == list_of_processes) {
		printf("Failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}
	char* name_of_process = "./sym_count";
	int current_proc = 0;
	int pipefd[2];
	pipefd[0] = -1;
	pipefd[1] = -1;
	int father_pipefd;
	int child_pipefd;
	char child_pipe_str[SIZE_OF_FD];

	int i = 0;
	for (; i < number_of_processes; i++) {
		char current_symbol[] = { pattern[i], '\0' };
		// create the the pipe, pass it to the sym_count correctly
		if (pipe(pipefd) == -1) {
			return handle_error_and_exit("Failed to pipe");
		}

		father_pipefd = pipefd[0];  // mng is a reader
		child_pipefd = pipefd[1];  // count is a writer
		child_pipe_str[0] = '\0';
		sprintf(child_pipe_str, "%d", child_pipefd);
		char* exec_args[] = { name_of_process, path_to_file, current_symbol, child_pipe_str, NULL };
		if ((current_proc = fork()) == 0) {  // child proc
			if (close(father_pipefd) == -1) {
				printf("Failed to close pipe fd: %s\n", strerror(errno));
				return errno;
			}
			int rc = execvp(exec_args[0], exec_args);
			if (rc == -1) { // failed to execute
				printf("Failed to start execution: %s\n", strerror(errno));
				return errno;
			}
		} else if (current_proc == -1) { // fork failed
			return handle_error_and_exit("Failed to fork");
		} else {  // parent process
			list_of_processes[i].pid_num = current_proc;
			list_of_processes[i].pipe_num = father_pipefd;
			if (close(child_pipefd) == -1) {
				return handle_error_and_exit("Failed to close pipe fd");
			}
		}
	}
	sleep(1); // sleep for 1 sec

	char buffer[BUFFER_SIZE + 1];
	int read_bytes;

	int still_running = 1;
	while (still_running == 1) {
		int i = 0;
		for (; i < number_of_processes; i++) {
			int r_status = -1;
			int rc = waitpid(list_of_processes[i].pid_num, &r_status, WNOHANG);
			if (rc == -1) { // waitpid failed
				return handle_error_and_exit("Failed to waitpid");
			}
			// rc == pid_num
			if (WIFEXITED(r_status)) { // process finished
				if (r_status == 0) { // exit status is O.K
					// read to correct pipefd
					if ((read_bytes = read(list_of_processes[i].pipe_num, buffer, BUFFER_SIZE)) > 0) {
						buffer[read_bytes] = '\0';
					}
					if (read_bytes == -1) { // error in read
						return handle_error_and_exit("Failed to read from pipe");
					}
					// print to std
					printf("%s", buffer);
					// remove process
					if (!remove_process(list_of_processes, i, &number_of_processes)) {
						return handle_error_and_exit("Failed to remove process");
					}
					// clear buffers
					buffer[0] = '\0';
					read_bytes = 0;
				} else {
					// exit status is failure, remove process
					if (!remove_process(list_of_processes, i, &number_of_processes)) {
						return handle_error_and_exit("Failed to remove process");
					}
				}
			}
		}
		if (number_of_processes == 0) { // finished running
			still_running = 0;
		}
		sleep(1);
	}
	free_child_proc(list_of_processes);
	return EXIT_SUCCESS;
}

