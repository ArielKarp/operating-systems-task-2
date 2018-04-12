/*
 * sym_count.c
 *
 *  Created on: Mar 28, 2018
 *      Author: ariel
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#define BUFFSIZE 512

// Define globals
int sym_cnt = 0;
char in_symbol;
int file_desc;
char* buffer;

int read_to_buffer(int fd, char* buffer, int* out_len) {
	if (NULL == buffer) {
		return 1;
	}
	ssize_t len = read(fd, buffer, BUFFSIZE);
	if (len < 0) {  //error reading from file, exit
		printf("Error reading from file: %s\n", strerror(errno));
		if (NULL != buffer) {
			free(buffer);
		}
		exit(EXIT_FAILURE);
	}
	// check if reached EOF
	if (len == 0) {
		return 2;
	}
	buffer[len] = '\0'; // string closer
	*out_len = len; // update length
	return 0;

}

void signal_term_handler(int signum) {
	printf("Process %d finishes. Symbol %c. Instances %d.\n", getpid(),
			in_symbol, sym_cnt);
	// exit gracefully
	if (NULL != buffer) { 	// release buffer
		free(buffer);
		buffer = NULL;
	}
	close(file_desc); // close file
	exit(EXIT_SUCCESS);
}

int register_signal_term_handling() {
	struct sigaction new_tern_action;
	memset(&new_tern_action, 0, sizeof(new_tern_action));

	new_tern_action.sa_handler = signal_term_handler;

	return sigaction(SIGTERM, &new_tern_action, NULL);
}

void signal_cont_handler(int signum) {
	printf("Process %d continues\n", getpid());
}

int register_signal_cont_handling() {
	struct sigaction new_cont_action;
	memset(&new_cont_action, 0, sizeof(new_cont_action));

	new_cont_action.sa_handler = signal_cont_handler;

	return sigaction(SIGCONT, &new_cont_action, NULL);
}

int main(int argc, char** argv) {
	if (argc < 3) {
		printf("Invalid input for the program, exiting...\n");
		exit(EXIT_FAILURE);
	}

	//register signals
	if (register_signal_term_handling() != 0) {
		printf("Signal term handle registration failed: %s\n", strerror(errno));
		return errno;
	}
	if (register_signal_cont_handling() != 0) {
		printf("Signal cont handle registration failed: %s\n", strerror(errno));
		return errno;
	}

	int curr_pid = getpid();

	// try and open the file
	file_desc = open(argv[1], O_RDONLY);

	if (file_desc < 0) {
		printf("Error opening file: %s\n", strerror( errno));
		return errno;
	}

	// check second argument is a single char
	if (strlen(argv[2]) != 1) {
		// more than a single char
		close(file_desc);
		exit(EXIT_FAILURE);
	}

	// get search symbol
	in_symbol = argv[2][0];

	// allocated a buffer
	char* buffer = (char*) calloc((BUFFSIZE + 1), sizeof(char));
	if (NULL == buffer) {
		printf("Could not allocated required data\n");
		close(file_desc);
		exit(EXIT_FAILURE);
	}

	// initialize variables for the main loop
	char curr_symbol;
	int curr_len = 0;
	int status_of_read = read_to_buffer(file_desc, buffer, &curr_len);
	while (status_of_read == 0) {
		// iterate over the buffer
		int i = 0;
		for (; i < curr_len; i++) {
			curr_symbol = buffer[i];
			if (curr_symbol == in_symbol) {
				sym_cnt++;
				printf("Process %d, symbol %c, going to sleep\n", curr_pid,
						in_symbol);
				// going to sleep
				raise(SIGSTOP);
				// program continued
			}
		}
		status_of_read = read_to_buffer(file_desc, buffer, &curr_len);
	}
	// got EOF
	if (status_of_read == 2) {
		// raise sigterm
		raise(SIGTERM);
	}
	return EXIT_SUCCESS;
}
