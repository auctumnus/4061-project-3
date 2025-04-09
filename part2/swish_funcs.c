#include "swish_funcs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "string_vector.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline. You should make
 * make use of the provided 'run_command' function here.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: Index of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: Index of the file descriptor in the array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or -1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
    int fret = fork();
    if (fret == -1) {
	    perror("fork");
	    return -1;
    } else if (fret == 0) {
	    //child
	    for (int i = 0; i < n_pipes; i++) {
		    if (i != in_idx && i != out_idx) {
			    if (close(pipes[i])) {
				    perror("close");
				    exit(1);
			    }
		    }
	    }

	    int pidx = strvec_find(tokens, "|");
	    if (pidx > 0) {
		    strvec_take(tokens, pidx);
	    }

	    if (in_idx != -1) {
		    if (dup2(pipes[in_idx], STDIN_FILENO)) {
			    perror("pipe");
			    exit(1);
		    }
	    }

	    if (out_idx != -1) {
		    if (dup2(pipes[out_idx], STDOUT_FILENO)) {
			    perror("pipe");
			    exit(1);
		    }
	    }

	    if (run_command(tokens)) {
		    fprintf(stderr, "run_command failed\n");
		    exit(1);
	    }

    } else if (fret > 0) {
	    //parent
	    return 0;
    }
    return 0;
}

int run_pipelined_commands(strvec_t *tokens) {
    int n_pipes = strvec_num_occurrences(tokens, "|");
    int *pfds = malloc(sizeof(int) * n_pipes * 2);
    if (pfds == NULL && n_pipes > 0) { // malloc might return NULL if the length is 0, which is fine
	    perror("malloc");
	    return -1;
    }
    for (int i = 0; i < n_pipes; i++) {
	    if (pipe(pfds + (i*2))) {
		    perror("pipe");
		    for (int j = 0; j < i*2; j++) {
			    close(pfds[j]);
			    close(pfds[j+1]);
		    }
		    free(pfds);
		    return -1;
	    }
    }

    strvec_t cur;
    if (strvec_slice(tokens, &cur, 0, tokens->length)) {
	    fprintf(stderr, "strvec slice failed\n");
	    for (int i = 0; i < n_pipes * 2; i++) {
		    close(pfds[i]);
	    }
	    free(pfds);
	    return -1;
    }

    for (int i = 0; i <= n_pipes; i++) {
	    int in_idx = (i == 0) ? -1 : (i-1) * 2;
	    int out_idx = (i == n_pipes) ? -1 : i * 2 + 1;
	    int length = n_pipes * 2;

	    if (run_piped_command(tokens, pfds, length, in_idx, out_idx)) {
		    fprintf(stderr, "failed to run one command in chain\n");
		    for (int j = 0; j < length; j++) {
			    close(pfds[j]);
		    }
		    free(pfds);
		    return -1;
	    }

	    if (i != n_pipes) {
		    strvec_t temp;
		    int pidx = strvec_find(&cur, "|");
		    if (strvec_slice(&cur, &temp, pidx, cur.length)) {
			    fprintf(stderr, "strvec slice failed\n");
			    for (int j = 0; j < length; j++) {
				    close(pfds[i]);
			    }
			    free(pfds);
			    return -1;
		    }
		    strvec_clear(&cur);
		    cur = temp;
	    }
    }

    for (int i = 0; i < n_pipes * 2; i++) {
	    if (close(pfds[i])) {
		    perror("close");
		    for (int j = i+1; j < n_pipes * 2; j++) {
			    close(pfds[j]);
		    }
		    free(pfds);
		    return -1;
	    }
    }

    free(pfds);

    return 0;
}
