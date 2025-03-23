#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chash.h"
#include <pthread.h>

#define MAX_LINE_LENGTH 256

void* process_command(void *arg) {
    char *cmd = (char *)arg;
    char *token = strtok(cmd, ",");
    while (token) {
        printf("Token: %s\n", token);
        token = strtok(NULL, ",");
    }
    free(cmd);
    return NULL;
}

int main(void) {
    FILE *file = fopen("commands.txt", "r");
    if (!file) {
        perror("Error opening commands.txt");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return EXIT_FAILURE;
    }

    // Parse the number of threads
    const char *token = strtok(line, ",");
    token = strtok(NULL, ",");
    const int num_threads = atoi(token);

    // Create an array of threads
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (!threads) {
        fclose(file);
        return EXIT_FAILURE;
    }

    int thread_count = 0;

    // Loops until the specified number of threads is created or there are no more lines to read.
    while (thread_count < num_threads && fgets(line, sizeof(line), file)) {
        // Remove the newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        // Create a copy of the line to pass to the thread
        char *cmd_copy = strdup(line);
        if (!cmd_copy) break;

        // Create a new thread to process the command
        // Create thread at index thread_count, no attributes, process_command function, and pass cmd_copy as function argument
        if (pthread_create(&threads[thread_count], NULL, process_command, cmd_copy) != 0) {
            free(cmd_copy);
            break;
        }
        thread_count++;
    }

    for (int i = 0; i < thread_count; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    fclose(file);
    return 0;
}
