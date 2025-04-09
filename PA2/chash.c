#include "chash.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 256

// Global table instance
hashTable table;
FILE *output_file = NULL;
int lock_acquisitions = 0;
int lock_releases = 0;

// Global synchronization for inserts and deletes.
int inserts_in_progress = 0;
int deletes_waiting = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t inserts_done = PTHREAD_COND_INITIALIZER;
pthread_cond_t deletes_done = PTHREAD_COND_INITIALIZER;

// Thread function to process a command.
void *process_command(void *arg) {
    char *cmd = (char *)arg;
    char *line_copy = strdup(cmd);
    char *token = strtok(line_copy, ",");

    if (strcmp(token, "insert") == 0) {
        char *name = strtok(NULL, ",");
        char *salary_str = strtok(NULL, ",");
        uint32_t salary = (uint32_t)atoi(salary_str);

        pthread_mutex_lock(&mutex);
        inserts_in_progress++;
        pthread_mutex_unlock(&mutex);

        insert(name, salary);

        pthread_mutex_lock(&mutex);
        inserts_in_progress--;
        if (inserts_in_progress == 0 && deletes_waiting > 0)
            pthread_cond_broadcast(&inserts_done);
        pthread_mutex_unlock(&mutex);
    } else if (strcmp(token, "delete") == 0) {
        char *name = strtok(NULL, ",");
        delete_record(name);
    } else if (strcmp(token, "search") == 0) {
        char *name = strtok(NULL, ",");
        search(name);
    }
    // No "print" command is processed here.

    free(line_copy);
    free(cmd);
    return NULL;
}

int main(void) {
    output_file = fopen("output.txt", "w");
    if (!output_file) {
        perror("Error opening output.txt");
        return EXIT_FAILURE;
    }

    FILE *file = fopen("commands.txt", "r");
    if (!file) {
        perror("Error opening commands.txt");
        fclose(output_file);
        return EXIT_FAILURE;
    }

    initialize_table();

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&inserts_done, NULL);
    pthread_cond_init(&deletes_done, NULL);

    char line[MAX_LINE_LENGTH];

    // Read header line (e.g., "threads,10")
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        fclose(output_file);
        return EXIT_FAILURE;
    }
    line[strcspn(line, "\n")] = '\0';
    char *token = strtok(line, ",");
    if (strcmp(token, "threads") != 0) {
        fprintf(stderr, "Error: First line must start with 'threads'\n");
        fclose(file);
        fclose(output_file);
        return EXIT_FAILURE;
    }
    token = strtok(NULL, ",");
    int num_threads = atoi(token);
    fprintf(output_file, "Running %d threads\n", num_threads);

    // Partition commands into insert and non-insert lists.
    char **insert_commands = NULL;
    int insert_count = 0;
    char **noninsert_commands = NULL;
    int noninsert_count = 0;
    int capacity_inserts = num_threads, capacity_noninserts = num_threads;
    insert_commands = malloc(capacity_inserts * sizeof(char *));
    noninsert_commands = malloc(capacity_noninserts * sizeof(char *));
    if (!insert_commands || !noninsert_commands) {
        fprintf(stderr, "Memory allocation error\n");
        fclose(file);
        fclose(output_file);
        return EXIT_FAILURE;
    }

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        char *cmd_copy = strdup(line);
        if (!cmd_copy)
            continue;
        if (strncmp(cmd_copy, "insert", 6) == 0) {
            if (insert_count >= capacity_inserts) {
                capacity_inserts *= 2;
                insert_commands = realloc(insert_commands, capacity_inserts * sizeof(char *));
            }
            insert_commands[insert_count++] = cmd_copy;
        } else {
            if (noninsert_count >= capacity_noninserts) {
                capacity_noninserts *= 2;
                noninsert_commands = realloc(noninsert_commands, capacity_noninserts * sizeof(char *));
            }
            noninsert_commands[noninsert_count++] = cmd_copy;
        }
    }
    fclose(file);

    // Phase 1: Process all insert commands.
    pthread_t *threads = malloc(insert_count * sizeof(pthread_t));
    for (int i = 0; i < insert_count; i++) {
        if (pthread_create(&threads[i], NULL, process_command, insert_commands[i]) != 0) {
            fprintf(stderr, "Error creating thread for insert command\n");
            free(insert_commands[i]);
        }
    }
    for (int i = 0; i < insert_count; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);

    // Phase 2: Process all non-insert commands (delete, search).
    threads = malloc(noninsert_count * sizeof(pthread_t));
    for (int i = 0; i < noninsert_count; i++) {
        if (pthread_create(&threads[i], NULL, process_command, noninsert_commands[i]) != 0) {
            fprintf(stderr, "Error creating thread for non-insert command\n");
            free(noninsert_commands[i]);
        }
    }
    for (int i = 0; i < noninsert_count; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);

    // Now that all worker threads have finished, print the final table.
    print_final_table();

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&inserts_done);
    pthread_cond_destroy(&deletes_done);

    fclose(output_file);
    free(insert_commands);
    free(noninsert_commands);

    return 0;
}
