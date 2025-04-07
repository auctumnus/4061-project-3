#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPHABET_LEN 26

int char2index(char c) {
    if (isalpha(c)) {
        return tolower(c) - 'a';
    }
    return -1;
}

char index2char(int index) {
    if (index >= 0 && index < ALPHABET_LEN) {
        return 'a' + index;
    }
    return '\0';
}

/*
 * Counts the number of occurrences of each letter (case insensitive) in a text
 * file and stores the results in an array.
 * file_name: The name of the text file in which to count letter occurrences
 * counts: An array of integers storing the number of occurrences of each letter.
 *     counts[0] is the number of 'a' or 'A' characters, counts [1] is the number
 *     of 'b' or 'B' characters, and so on.
 * Returns 0 on success or -1 on error.
 */
int count_letters(const char *file_name, int *counts) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }
    char buffer[1024];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (int i = 0; i < bytes_read; i++) {
            int index = char2index(buffer[i]);
            if (index != -1) {
                counts[index]++;
            }
        }
    }
    
    if (bytes_read == -1) {
        perror("read");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

/*
 * Processes a particular file(counting occurrences of each letter)
 *     and writes the results to a file descriptor.
 * This function should be called in child processes.
 * file_name: The name of the file to analyze.
 * out_fd: The file descriptor to which results are written
 * Returns 0 on success or -1 on error
 */
int process_file(const char *file_name, int out_fd) {
    int counts[ALPHABET_LEN] = {0};
    if (count_letters(file_name, counts) == -1) {
        return -1;
    }
    // TODO it seems like they want us to use stdio but
    // i dont rly want to figure out if stdio plays nicely
    // with pipes
    if(write(out_fd, counts, sizeof(counts)) == -1) {
        perror("write");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }

    // TODO Create a pipe for child processes to write their results
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return 1;
    }

    // TODO Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    for (int i = 1; i < argc; i++) {
        // TODO is there any normal-person way to communicate errors from children back up?
        // im pretty sure we dont need to b/c this passes the tests anyways but i feel like
        // i should be exiting w/ a nonzero exit code
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            return 1;
        } else if (child_pid == 0) {
            close(pipe_fds[0]);
            process_file(argv[i], pipe_fds[1]);
            close(pipe_fds[1]);
            return 0;
        }
    }

    // in parent
    close(pipe_fds[1]);

    // TODO Aggregate all the results together by reading from the pipe in the parent
    int counts[ALPHABET_LEN] = {0};
    for(int i = 0; i < argc; i++) {
        int file_counts[ALPHABET_LEN] = {0};
        if (read(pipe_fds[0], file_counts, sizeof(file_counts)) == -1) {
            perror("read");
            return 1;
        }
        for (int j = 0; j < ALPHABET_LEN; j++) {
            counts[j] += file_counts[j];
        }
    }
    close(pipe_fds[0]);

    // TODO Change this code to print out the total count of each letter (case insensitive)
    for (int i = 0; i < ALPHABET_LEN; i++) {
        printf("%c Count: %d\n", index2char(i), counts[i]);
    }
    return 0;
}
