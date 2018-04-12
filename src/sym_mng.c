/*
 * sym_mng.c
 *
 *  Created on: Mar 31, 2018
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

#define NUM_OF_ELEM 2

typedef struct {
	int pid_num;
	int stop_cnt;
} ChildProc;

int check_third_argument(char* termination_bount) {
	int rc = 1;
	int len_of_term = strlen(termination_bount);
	int i = 0;
	for (; i < len_of_term; i++) {
		if (!isdigit(termination_bount[i])) {
			printf("Third argument is not a number\n");
			rc = 0;
		}
	}
	return rc;
}

void free_child_proc(ChildProc* ptr) {
	if (NULL != ptr) {
		free(ptr);
		ptr = NULL;
	}
}

int remove_process(ChildProc* list_of_processes, int index, int* number_of_processes) {
	int rc = 1;
	ChildProc temp = list_of_processes[index]; // safe- ChildProc does not contain points
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

void clean_up_remaining_processes(ChildProc* list_of_processes, int number_of_process) {
	// kill and free all child processes
	int i = 0;
	for (; i < number_of_process; i++) {
		kill(list_of_processes[i].pid_num, SIGKILL);
	}
	free_child_proc(list_of_processes);
}

int main(int argc, char** argv) {
	// check number of input arguments
	if (argc < 4) {
		printf("Invalid input for the program, exiting\n");
		exit(EXIT_FAILURE);
	}
	//check third input is a number
	if (!check_third_argument(argv[3])) {
		exit(EXIT_FAILURE);
	}

	char* path_to_file = argv[1];
	char* pattern = argv[2];
	int termination_bound = atoi(argv[3]);
	int number_of_processes = strlen(pattern);
	ChildProc* list_of_processes = (ChildProc*) calloc(number_of_processes,
			sizeof(ChildProc));
	if (NULL == list_of_processes) {
		printf("Failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}
	char* name_of_process = "./sym_count";
	int current_proc = 0;

	int i = 0;
	for (; i < number_of_processes; i++) {
		char current_symbol[] = {pattern[i], '\0'};
		char* exec_args[] = { name_of_process, path_to_file, current_symbol, NULL };
		if ((current_proc = fork()) == 0) {
			int rc = execvp(exec_args[0], exec_args);
			if (rc == -1) { // failed to execute
				printf("Failed to start execution: %s\n", strerror(errno));
				return errno;
			}
		} else if (current_proc == -1) { // fork failed
			free_child_proc(list_of_processes);
			printf("Failed to fork: %s\n", strerror(errno));
			return errno;
		} else {  // parent process
			list_of_processes[i].pid_num = current_proc;
			list_of_processes[i].stop_cnt = 0;
		}
	}

	sleep(1); // sleep for 1 sec

	int still_running = 1;
	while (still_running == 1) {
		int i = 0;
		for (; i < number_of_processes; i++) {
			int r_status = -1;
			int rc = waitpid(list_of_processes[i].pid_num, &r_status, WCONTINUED | WUNTRACED | WNOHANG);
			if (rc == -1) { // waitpid failed
				printf("Failed to waitpid: %s\n", strerror(errno));
				clean_up_remaining_processes(list_of_processes, number_of_processes);
				return errno;
			}
			// rc == pid_num
			if (WIFSTOPPED(r_status)) {
				list_of_processes[i].stop_cnt++;
				if (list_of_processes[i].stop_cnt == termination_bound) {
					// kill process
					if (kill(list_of_processes[i].pid_num, SIGTERM) < 0) { // failed to send sigterm
						printf("Failed to seng SIGTERM: %s\n", strerror(errno));
						clean_up_remaining_processes(list_of_processes, number_of_processes);
						return errno;
					}
					if (kill(list_of_processes[i].pid_num, SIGCONT) < 0) { // failed to send sigcont
						printf("Failed to seng SIGCONT: %s\n", strerror(errno));
						clean_up_remaining_processes(list_of_processes, number_of_processes);
						return errno;
					}
					if (!remove_process(list_of_processes, i, &number_of_processes)) {
						clean_up_remaining_processes(list_of_processes, number_of_processes);
						exit(EXIT_FAILURE);
					}

				} else { // stop_cnt is only less-than termination_bount
					// send SIGCONT
					if (kill(list_of_processes[i].pid_num, SIGCONT) < 0) { // failed to send sigcont
						printf("Failed to seng SIGCONT: %s\n", strerror(errno));
						clean_up_remaining_processes(list_of_processes, number_of_processes);
						return errno;
					}
				}
			}
			if (WIFEXITED(r_status)) { // process finished
				// remove process
				if (!remove_process(list_of_processes, i,
						&number_of_processes)) {
					clean_up_remaining_processes(list_of_processes, number_of_processes);
					exit(EXIT_FAILURE);
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

