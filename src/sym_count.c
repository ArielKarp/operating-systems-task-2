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


// Define globals
int sym_cnt = 0;
char in_symbol;
int file_desc = -1;


void signal_term_handler(int signum) {
	printf("Process %d finishes. Symbol %c. Instances %d.\n", getpid(),
			in_symbol, sym_cnt);
	// exit gracefully
	close(file_desc); // close file
	exit(EXIT_SUCCESS);
}

int register_signal_term_handling() {
	struct sigaction new_tern_action;
	memset(&new_tern_action, 0, sizeof(new_tern_action));

	new_tern_action.sa_handler = signal_term_handler;

	return sigaction(SIGTERM, &new_tern_action, NULL);
}

int handle_error_exit(const char* error_msg) {
	// free fd
	if (file_desc != -1) {
		close(file_desc);
	}
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errno));
	return errno;
}

int main(int argc, char** argv) {
	if (argc != 3 && argc != 4) {
		//printf("Invalid input for the program, exiting...\n");
		//exit(EXIT_FAILURE);
		return handle_error_exit("Invalid input for the program, exiting");
	}

	//register signals
	if (register_signal_term_handling() != 0) {
		//printf("Signal term handle registration failed: %s\n", strerror(errno));
		//return errno;
		return handle_error_exit("Signal term handle registration failed");
	}

	// check second argument is a single char
	if (strlen(argv[2]) != 1) {
		// more than a single char
		//close(file_desc);
		//exit(EXIT_FAILURE);
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
		//printf("Error opening file: %s\n", strerror( errno));
		//return errno;
		return handle_error_exit("Error opening file");
	}


	if (stat(file_name, &file_stat)  == -1) {
		//printf("Failed to retrieve stat data: %s\n", strerror(errno));
		//close(file_desc);
		//return errno;
		return handle_error_exit("Failed to retrieve stat data");
	}

	// check if regular file
	if (!S_ISREG(file_stat.st_mode)) {
		//printf("Not a regular file, exiting...\n");
		//close(file_desc);
		//exit(EXIT_FAILURE);
		return handle_error_exit("Not a regular file, exiting");
	}

    // set file size
    if (lseek(file_desc, file_stat.st_size - 1, SEEK_SET) == -1)
    	return handle_error_exit("Failed stretching file to required size");

    // write last byte
    if (write(file_desc, "\0", 1) == -1)
    	return handle_error_exit("Failed writing last byte");

	// load mmap
    char* file_data = (char*) mmap( NULL, file_stat.st_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
						file_desc,
                        0 );

    if (file_data == MAP_FAILED)
    	return handle_error_exit("Failed mapping file to memory");

    //main loop
    int i = 0;
    char current_symbol;
    for(; i < file_stat.st_size; i++) {
    	current_symbol = file_data[i];  // get current char
    	if (current_symbol == in_symbol) {
    		sym_cnt++;
    	}
    }

    // free data
    if (munmap(file_data, file_stat.st_size) == -1)
    	return handle_error_exit("Failed unmapping file");


    // final print
    printf("Process %d finishes. Symbol %c. Instances %d.\n", getpid(), in_symbol, sym_cnt);
    close(file_desc);

    exit(EXIT_SUCCESS);
}







