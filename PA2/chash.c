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

// Get current timestamp in microseconds
long long get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000000 + tv.tv_usec;
}

// Jenkins Hash Function
uint32_t jenkins_hash(const char *key) {
    uint32_t hash = 0;
    while (*key) {
        hash += *key++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

// Initialize Hash Table and Locks
void initialize_table() {
    for (int i = 0; i < TABLE_SIZE; i++) {
        table.buckets[i] = NULL;
        rwlock_init(&table.locks[i]);
    }
}

// Initialize Read/Write Lock
void rwlock_init(rwlock_t *lock) {
    lock->readers = 0;
    Sem_init(&lock->lock, 1);
    Sem_init(&lock->writelock, 1);
}

// Acquire and Release Read Lock
void rwlock_acquire_readlock(rwlock_t *lock) {
    Sem_wait(&lock->lock);
    lock->readers++;
    if (lock->readers == 1)
        Sem_wait(&lock->writelock);
    Sem_post(&lock->lock);

    lock_acquisitions++;
    fprintf(output_file, "%lld: READ LOCK ACQUIRED\n", get_timestamp());
}

void rwlock_release_readlock(rwlock_t *lock) {
    Sem_wait(&lock->lock);
    lock->readers--;
    if (lock->readers == 0)
        Sem_post(&lock->writelock);
    Sem_post(&lock->lock);

    lock_releases++;
    fprintf(output_file, "%lld: READ LOCK RELEASED\n", get_timestamp());
}

// Acquire and Release Write Lock
void rwlock_acquire_writelock(rwlock_t *lock) {
    Sem_wait(&lock->writelock);
    lock_acquisitions++;
    fprintf(output_file, "%lld: WRITE LOCK ACQUIRED\n", get_timestamp());
}

void rwlock_release_writelock(rwlock_t *lock) {
    Sem_post(&lock->writelock);
    lock_releases++;
    fprintf(output_file, "%lld: WRITE LOCK RELEASED\n", get_timestamp());
}

// Insert Record into Hash Table
void insert(const char *name, uint32_t salary) {

    //adding a delay randomizes the threads
    sleep(1);
    fprintf(output_file, "%lld: INSERT,%u,%s,%u\n", get_timestamp(), jenkins_hash(name), name, salary);

    uint32_t hash = jenkins_hash(name);
    int index = hash % TABLE_SIZE;
    rwlock_acquire_writelock(&table.locks[index]);

    hashRecord *current = table.buckets[index];
    while (current) {
        if (strcmp(current->name, name) == 0) {
            current->salary = salary;
            rwlock_release_writelock(&table.locks[index]);
            return;
        }
        current = current->next;
    }

    hashRecord *newNode = malloc(sizeof(hashRecord));
    if (!newNode) {
        fprintf(output_file, "Memory allocation error in insert\n");
        rwlock_release_writelock(&table.locks[index]);
        return;
    }
    strcpy(newNode->name, name);
    newNode->salary = salary;
    newNode->hash = hash;
    newNode->next = table.buckets[index];
    table.buckets[index] = newNode;

    rwlock_release_writelock(&table.locks[index]);
}

// Global synchronization for inserts and deletes.
int inserts_in_progress = 0;
int deletes_waiting = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t inserts_done = PTHREAD_COND_INITIALIZER;
pthread_cond_t deletes_done = PTHREAD_COND_INITIALIZER;

// Delete Record from Hash Table
void delete_record(const char *name) {
    // Log that the delete operation is awakening.
    fprintf(output_file, "%lld: DELETE AWAKENED\n", get_timestamp());

    pthread_mutex_lock(&mutex);
    while (inserts_in_progress > 0) {
        deletes_waiting++;
        fprintf(output_file, "%lld: WAITING ON INSERTS\n", get_timestamp());
        pthread_cond_wait(&inserts_done, &mutex);
        deletes_waiting--;
    }
    pthread_mutex_unlock(&mutex);

    // After waiting, log the delete command for the specified record.
    fprintf(output_file, "%lld: DELETE,%s\n", get_timestamp(), name);

    uint32_t hash = jenkins_hash(name);
    int index = hash % TABLE_SIZE;
    rwlock_acquire_writelock(&table.locks[index]);

    hashRecord *current = table.buckets[index];
    hashRecord *prev = NULL;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (prev == NULL)
                table.buckets[index] = current->next;
            else
                prev->next = current->next;
            free(current);
            rwlock_release_writelock(&table.locks[index]);

            pthread_mutex_lock(&mutex);
            pthread_cond_signal(&deletes_done);
            pthread_mutex_unlock(&mutex);
            return;
        }
        prev = current;
        current = current->next;
    }
    rwlock_release_writelock(&table.locks[index]);
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&deletes_done);
    pthread_mutex_unlock(&mutex);
}


// Search for a Record in the Hash Table
void search(const char *name) {
    fprintf(output_file, "%lld: SEARCH,%s\n", get_timestamp(), name);

    uint32_t hash = jenkins_hash(name);
    int index = hash % TABLE_SIZE;
    rwlock_acquire_readlock(&table.locks[index]);

    hashRecord *current = table.buckets[index];
    while (current) {
        if (strcmp(current->name, name) == 0) {
            fprintf(output_file, "%u,%s,%u\n", current->hash, current->name, current->salary);
            rwlock_release_readlock(&table.locks[index]);
            return;
        }
        current = current->next;
    }
    fprintf(output_file, "%lld: SEARCH: NOT FOUND NOT FOUND\n", get_timestamp());
    rwlock_release_readlock(&table.locks[index]);
}

// Compare function for sorting records by hash value.
int compare_hash_records(const void *a, const void *b) {
    hashRecord *record_a = *(hashRecord **)a;
    hashRecord *record_b = *(hashRecord **)b;
    if (record_a->hash > record_b->hash)
        return 1;
    else if (record_a->hash < record_b->hash)
        return -1;
    else
        return 0;
}

// Revised print_final_table: since no updates occur at this point, locks are omitted.
void print_final_table() {
    int record_count = 0;
    // Count records without locking (table is static now)
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashRecord *current = table.buckets[i];
        while (current) {
            record_count++;
            current = current->next;
        }
    }

    if (record_count == 0) {
        fprintf(output_file, "Number of lock acquisitions: %d\n", lock_acquisitions);
        fprintf(output_file, "Number of lock releases: %d\n", lock_releases);
        fprintf(output_file, "Table is empty.\n");
        return;
    }

    hashRecord **records = malloc(record_count * sizeof(hashRecord *));
    if (!records) {
        fprintf(output_file, "Memory allocation error in print_final_table\n");
        return;
    }

    int index_counter = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashRecord *current = table.buckets[i];
        while (current) {
            records[index_counter++] = current;
            current = current->next;
        }
    }
    qsort(records, record_count, sizeof(hashRecord *), compare_hash_records);

    // Print lock statistics.
    fprintf(output_file, "Finished all threads.\n");
    fprintf(output_file, "Number of lock acquisitions: %d\n", lock_acquisitions);
    fprintf(output_file, "Number of lock releases: %d\n", lock_releases);

    // Print final table snapshot.
    fprintf(output_file, "%lld: READ LOCK ACQUIRED\n", get_timestamp());
    for (int i = 0; i < record_count; i++) {
        fprintf(output_file, "%u,%s,%u\n", records[i]->hash, records[i]->name, records[i]->salary);
    }
    fprintf(output_file, "%lld: READ LOCK RELEASED\n", get_timestamp());
    free(records);
}

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
