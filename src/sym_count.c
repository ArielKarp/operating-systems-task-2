/*
 * sym_count.c
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define NUM_TO_REDUCE 2

// Define globals
int sym_cnt = 0;
char in_symbol;
int file_desc = -1;
int pipe_fd = -1;
char* file_data = NULL;
int file_size;
char* msg_str = NULL;

int cnt_num(int number) {
	if (number == 0) {
		return 1;
	}
	int size = 0;
	while (number) {
		number /= 10;
		size++;
	}
	return size;
}

void release_resources() {
	if (file_desc != -1)
		close(file_desc); // close file
	if (pipe_fd != -1)
		close(pipe_fd);  // close pipe
	if (NULL != file_data)
		munmap(file_data, file_size);
	if (NULL != msg_str)
		free(msg_str);
}

void signal_term_handler(int signum) {
	// exit gracefully
	release_resources();
	exit(EXIT_SUCCESS);
}

int register_signal_term_handling() {
	struct sigaction new_tern_action;
	memset(&new_tern_action, 0, sizeof(new_tern_action));

	new_tern_action.sa_handler = signal_term_handler;

	return sigaction(SIGTERM, &new_tern_action, NULL);
}

void signal_pipe_handler(int signum) {
	// exit gracefully
	release_resources();
	printf("SIGPIPE for process %d. Symbol %c. Counter %d.Leaving.\n", getpid(),
			in_symbol, sym_cnt);
	exit(EXIT_SUCCESS);
}

int register_signal_pipe_handling() {
	struct sigaction new_pipe_action;
	memset(&new_pipe_action, 0, sizeof(new_pipe_action));

	new_pipe_action.sa_handler = signal_pipe_handler;

	return sigaction(SIGPIPE, &new_pipe_action, NULL);
}

int handle_error_exit(const char* error_msg) {
	release_resources();
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errno));
	return errno;
}

int main(int argc, char** argv) {
	if (argc != 3 && argc != 4) {
		return handle_error_exit("Invalid input for the program, exiting");
	}

	//register signals
	if (register_signal_term_handling() != 0) {
		return handle_error_exit("Signal term handle registration failed");
	}

	if (register_signal_pipe_handling() != 0) {
		return handle_error_exit("Signal pipe handle registration failed");
	}

	// check second argument is a single char
	if (strlen(argv[2]) != 1) {
		// more than a single char
		return handle_error_exit("Second argument is not of length 1");
	}

	// get search symbol
	in_symbol = argv[2][0];

	// set file params
	char* file_name = argv[1];
	struct stat file_stat;

	// try and open the file
	file_desc = open(file_name, O_RDWR, 0600);

	if (file_desc < 0) {
		return handle_error_exit("Error opening file");
	}

	if (stat(file_name, &file_stat) == -1) {
		return handle_error_exit("Failed to retrieve stat data");
	}

	// check if regular file
	if (!S_ISREG(file_stat.st_mode)) {
		return handle_error_exit("Not a regular file, exiting");
	}

	file_size = file_stat.st_size;

	// set file size
	if (lseek(file_desc, file_size - 1, SEEK_SET) == -1)
		return handle_error_exit("Failed stretching file to required size");

	// write last byte
	if (write(file_desc, "\0", 1) == -1)
		return handle_error_exit("Failed writing last byte");

	// load mmap
	file_data = (char*) mmap( NULL, file_size,
	PROT_READ | PROT_WRITE,
	MAP_SHARED, file_desc, 0);

	if (file_data == MAP_FAILED)
		return handle_error_exit("Failed mapping file to memory");

	//main loop
	int i = 0;
	char current_symbol;
	for (; i < file_size; i++) {
		current_symbol = file_data[i];  // get current char
		if (current_symbol == in_symbol) {
			sym_cnt++;
		}
	}

	// free data
	if (munmap(file_data, file_size) == -1)
		return handle_error_exit("Failed unmapping file");

	// final report
	if (argc == 4) {
		sscanf(argv[3], "%d", &pipe_fd);  // get pipe_fd
		char* out_str = "Process %d finishes. Symbol %c. Instances %d.\n";
		int base_out_str = strlen(out_str);
		int cnt_process = cnt_num(getpid());
		int cnt_sym = cnt_num(sym_cnt);
		int size_of_out_str = base_out_str + cnt_process + cnt_sym + 1
				- (3 * NUM_TO_REDUCE);
		msg_str = (char*) malloc(size_of_out_str * sizeof(char));
		if (NULL == msg_str) {
			return handle_error_exit("Failed to allocate memory");
		}
		sprintf(msg_str, "Process %d finishes. Symbol %c. Instances %d.\n",
				getpid(), in_symbol, sym_cnt);
		if (write(pipe_fd, msg_str, size_of_out_str) == -1) {
			return handle_error_exit("Failed to write message to pipe");
		}
		free(msg_str);
		if (close(pipe_fd) == -1) {
			return handle_error_exit("Failed to close pipe file descriptor");
		}
	} else {
		printf("Process %d finishes. Symbol %c. Instances %d.\n", getpid(),
				in_symbol, sym_cnt);
	}

	if (close(file_desc) == -1) {
		return handle_error_exit("Failed to close file descriptor");
	}

	exit(EXIT_SUCCESS);
}

