#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024
#define MORE 32
#define READ_END 0
#define WRITE_END 1
#define CHILD_NO 5

struct timespec difference(struct timespec start, struct timespec end);
char* gentime(char *buffer, int sec, int nsec);
char* itoa(int i, char output[]);
char* fillzero(char* input, int places);

int main(int argc, char** argv) {

	char buffer[BUFFER_SIZE + 1];
	char secondbuffer[BUFFER_SIZE + MORE + 1];

	int fd[CHILD_NO][2];

	//create the file descriptors for the pipes
	for (int j = 0; j < CHILD_NO; j++) {
		if (pipe(fd[j]) == -1) {
			printf("pipe() failed\n");
			perror("pipe() failed\n");
			exit(EXIT_FAILURE);
		}
	}

	// fork processes
	pid_t fork_result;
	int idx;
	for (idx = 0; idx < CHILD_NO; idx++) {
		fork_result = fork();
#ifdef DEBUG
		printf("forked; idx %d; fr %d\n", idx, fork_result);
#endif
		if (fork_result == 0) {
			break;
		} else if (fork_result == -1) {
			printf("fork failed");
			exit(EXIT_FAILURE);
		}
	}

	//calculate time since the start of program
	struct timespec starting_time;
	clock_gettime(CLOCK_MONOTONIC, &starting_time);
	struct timespec current_time;
	clock_gettime(CLOCK_MONOTONIC, &current_time);
	struct timespec diff = difference(starting_time, current_time);

	//if in child, write to file descriptor, if in parent, read from file descriptor
	if (fork_result == 0) {
		close(fd[idx][READ_END]);
		switch (idx) {

		//read input from stdin in child 5
		case 4:
			while (diff.tv_sec < 30) {
				int n = 0;
				n = read(0, buffer, BUFFER_SIZE);
				buffer[n] = '\0';
#ifdef DEBUG
				printf("from stdin: %s\n", buffer);
#endif
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				diff = difference(starting_time, current_time);
				gentime(secondbuffer, (int) diff.tv_sec,
						(int) (diff.tv_nsec / 1000000));
				strcat(secondbuffer, buffer);

#ifdef DEBUG
				printf("%s\n", secondbuffer);
				fflush(stdout);
#endif

				write(fd[idx][WRITE_END], secondbuffer, strlen(secondbuffer) + 1);
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				diff = difference(starting_time, current_time);
			}
			break;

		// send standard message if in child 1-4
		default:
			while (diff.tv_sec < 30) {
				int sleep_time = rand() % 3;
				sleep(sleep_time);
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				diff = difference(starting_time, current_time);
				gentime(buffer, (int) diff.tv_sec,
						(int) (diff.tv_nsec / 1000000));
				strcat(buffer, "Child ");

				char anotherbuffer[MORE];
				strcat(buffer, itoa(idx, anotherbuffer));
				strcat(buffer, " message\n");

#ifdef DEBUG
				printf("%s\n", buffer);
				fflush(stdout);
#endif
				write(fd[idx][WRITE_END], buffer, strlen(buffer) + 1);
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				diff = difference(starting_time, current_time);
			}
			break;
		}
	} else {

		//close unused file descriptors in parent
		for (int j = 0; j < CHILD_NO; j++) {
			close(fd[j][WRITE_END]);
		}

		//open file for writing
		FILE *f;
		f = fopen("output.txt", "w");

		//set length of timeout for select()
		struct timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		//calculate time since program began
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		struct timespec diff = difference(starting_time, current_time);

		fd_set input;

		//run loop for 30 seconds
		while (diff.tv_sec < 30) {
			FD_ZERO(&input);
			for (int j = 0; j < CHILD_NO; j++) {
				FD_SET(fd[j][READ_END], &input);
			}

			int result = select(FD_SETSIZE, &input, (fd_set*) 0, (fd_set*) 0,
					&timeout);
			switch (result) {
			//no activity on file descriptors
			case 0:
				break;

			//select error
			case -1:
				perror("error on switch");
				printf("error on switch");
				exit(EXIT_FAILURE);
				break;

			//activity on file descriptors
			default:
				for (int j = 0; j < CHILD_NO; j++) {
					if (FD_ISSET(fd[j][READ_END], &input)) {
						int n;
						n = read(fd[j][READ_END], secondbuffer, BUFFER_SIZE + MORE);
						secondbuffer[n + 1] = '\0';
						clock_gettime(CLOCK_MONOTONIC, &current_time);
						diff = difference(starting_time, current_time);
						fprintf(f, "0:%02d.%03d: %s", (int) diff.tv_sec,
								(int) (diff.tv_nsec / 1000000), secondbuffer);
						//printf("test\n");
						fflush(f);
					}

				}
				break;
			}

			//calculate time since program began
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			diff = difference(starting_time, current_time);

		}
	}

}

//calculate time difference between start and end
struct timespec difference(struct timespec start, struct timespec end) {
	struct timespec difference;
	difference.tv_sec = end.tv_sec - start.tv_sec - 1
			+ (end.tv_nsec - start.tv_nsec + 1000000000l) / 1000000000l;
	difference.tv_nsec = (end.tv_nsec - start.tv_nsec + 1000000000l)
			% 1000000000l;

	return difference;
}

//generate a timestamp without using sprintf
char* gentime(char *buffer, int sec, int msec) {
	buffer[0] = '\0';
	char sec_str[MORE];
	char msec_str[MORE];

	strcat(buffer, "0:");
	strcat(buffer, fillzero(itoa(sec, sec_str), 2));
	strcat(buffer, ".");
	strcat(buffer, fillzero(itoa(msec, msec_str), 3));
	strcat(buffer, ": ");

	return buffer;
}

//convert integer to character array
char* itoa(int i, char output[]) {
	char const digit[] = "0123456789";
	char* traverse = output;
	int shiftThis = i;

	do {
		traverse++;
		shiftThis = shiftThis / 10;
	} while (shiftThis);

	*traverse = '\0';

	do {
		*--traverse = digit[i % 10];
		i = i / 10;
	} while (i);

	return output;
}

//insert zeros before input if length requirement not satisfied
char* fillzero(char* input, int places) {
	char buffer[MORE];
	buffer[0] = '\0';
	for (int i = strlen(input); i < places; i++) {
		strcat(buffer, "0");
	}
	strcat(buffer, input);
	strcpy(input, buffer);

	return input;
}
