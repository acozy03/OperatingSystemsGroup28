#include "chash.h"
#include <time.h>
#include <sys/time.h>

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
    fprintf(output_file, "%lld,READ LOCK ACQUIRED\n", get_timestamp());
}

void rwlock_release_readlock(rwlock_t *lock) {
    Sem_wait(&lock->lock);
    lock->readers--;
    if (lock->readers == 0)
        Sem_post(&lock->writelock);
    Sem_post(&lock->lock);
    
    lock_releases++;
    fprintf(output_file, "%lld,READ LOCK RELEASED\n", get_timestamp());
}

// Acquire and Release Write Lock
void rwlock_acquire_writelock(rwlock_t *lock) {
    Sem_wait(&lock->writelock);
    
    lock_acquisitions++;
    fprintf(output_file, "%lld,WRITE LOCK ACQUIRED\n", get_timestamp());
}

void rwlock_release_writelock(rwlock_t *lock) {
    Sem_post(&lock->writelock);
    
    lock_releases++;
    fprintf(output_file, "%lld,WRITE LOCK RELEASED\n", get_timestamp());
}

// Insert Record into Hash Table
void insert(const char *name, uint32_t salary) {
    // Log the command
    fprintf(output_file, "%lld,INSERT,%s,%u\n", get_timestamp(), name, salary);
    
    // Compute the hash for the given key.
    uint32_t hash = jenkins_hash(name);
    // Acquire the **write lock** for the list.
    rwlock_acquire_writelock(&table.locks[hash % TABLE_SIZE]);

    // Search the linked list for an existing node with the same hash:
    hashRecord *current = table.buckets[hash % TABLE_SIZE];
    while (current) {
        // If found, update its value.
        if (strcmp(current->name, name) == 0) {
            current->salary = salary;
            rwlock_release_writelock(&table.locks[hash % TABLE_SIZE]);
            return;
        }
        current = current->next;
    }

    // Otherwise, create and insert a new node.
    hashRecord *newNode = malloc(sizeof(hashRecord));
    strcpy(newNode->name, name);
    newNode->salary = salary;
    newNode->hash = hash;
    newNode->next = table.buckets[hash % TABLE_SIZE];
    table.buckets[hash % TABLE_SIZE] = newNode;

    // Release the write lock and return.
    rwlock_release_writelock(&table.locks[hash % TABLE_SIZE]);
}

// Global read/write lock instance for coordinating operations
rwlock_t cv_lock;
int inserts_in_progress = 0;
int deletes_waiting = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t inserts_done = PTHREAD_COND_INITIALIZER;
pthread_cond_t deletes_done = PTHREAD_COND_INITIALIZER;

// Delete Record from Hash Table
void delete(const char *name) {
    // Log the command
    fprintf(output_file, "%lld,DELETE,%s\n", get_timestamp(), name);
    
    // Wait for all inserts to complete
    pthread_mutex_lock(&mutex);
    if (inserts_in_progress > 0) {
        deletes_waiting++;
        fprintf(output_file, "%lld: WAITING ON INSERTS\n", get_timestamp());
        pthread_cond_wait(&inserts_done, &mutex);
        deletes_waiting--;
        fprintf(output_file, "%lld: DELETE AWAKENED\n", get_timestamp());
    }
    pthread_mutex_unlock(&mutex);
    
    // Compute the hash value for the given key.
    uint32_t hash = jenkins_hash(name);

    // Acquire the write lock for the appropriate bucket.
    rwlock_acquire_writelock(&table.locks[hash % TABLE_SIZE]);

    // Set up pointers to traverse the linked list.
    hashRecord *current = table.buckets[hash % TABLE_SIZE];
    hashRecord *prev = NULL;

    // Search for the record with the matching name.
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // If the record is found, remove it from the list.
            if (prev == NULL) {
                // Deleting the first node in the list.
                table.buckets[hash % TABLE_SIZE] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            rwlock_release_writelock(&table.locks[hash % TABLE_SIZE]);
            
            // Signal that a delete is complete
            pthread_mutex_lock(&mutex);
            pthread_cond_signal(&deletes_done);
            pthread_mutex_unlock(&mutex);
            
            return;
        }
        prev = current;
        current = current->next;
    }

    // If the record is not found, release the lock and return.
    rwlock_release_writelock(&table.locks[hash % TABLE_SIZE]);
    
    // Signal that a delete is complete
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&deletes_done);
    pthread_mutex_unlock(&mutex);
}

// Searches for a Record in the Hash Table
void search(const char *name) {
    // Log the command
    fprintf(output_file, "%lld,SEARCH,%s\n", get_timestamp(), name);
    
    uint32_t hash = jenkins_hash(name);
    rwlock_acquire_readlock(&table.locks[hash % TABLE_SIZE]);

    hashRecord *current = table.buckets[hash % TABLE_SIZE];

    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // Found the record, print it
            fprintf(output_file, "%u,%s,%u\n", current->hash, current->name, current->salary);
            rwlock_release_readlock(&table.locks[hash % TABLE_SIZE]);
            return;
        }
        current = current->next;
    }

    // Record not found
    fprintf(output_file, "No Record Found\n");
    rwlock_release_readlock(&table.locks[hash % TABLE_SIZE]);
}

// Compare function for qsort
int compare_hash_records(const void *a, const void *b) {
    hashRecord *record_a = *(hashRecord **)a;
    hashRecord *record_b = *(hashRecord **)b;
    return (record_a->hash > record_b->hash) - (record_a->hash < record_b->hash);
}

// Print all records in the hash table
void print() {
    // Log the command
    fprintf(output_file, "%lld,PRINT\n", get_timestamp());
    
    // Create an array to hold all records
    hashRecord **records = NULL;
    int record_count = 0;
    
    // First, count how many records we have
    for (int i = 0; i < TABLE_SIZE; i++) {
        rwlock_acquire_readlock(&table.locks[i]);
        hashRecord *current = table.buckets[i];
        while (current != NULL) {
            record_count++;
            current = current->next;
        }
        rwlock_release_readlock(&table.locks[i]);
    }
    
    // Allocate memory for the array
    records = malloc(record_count * sizeof(hashRecord *));
    if (records == NULL) {
        fprintf(output_file, "Memory allocation error\n");
        return;
    }
    
    // Fill the array with pointers to all records
    int index = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        rwlock_acquire_readlock(&table.locks[i]);
        hashRecord *current = table.buckets[i];
        while (current != NULL) {
            records[index++] = current;
            current = current->next;
        }
        rwlock_release_readlock(&table.locks[i]);
    }
    
    // Sort the array by hash value
    qsort(records, record_count, sizeof(hashRecord *), compare_hash_records);
    
    // Print all records in sorted order
    for (int i = 0; i < record_count; i++) {
        fprintf(output_file, "%u,%s,%u\n", records[i]->hash, records[i]->name, records[i]->salary);
    }
    
    // Free the array
    free(records);
}

// Print final table and statistics
void print_final_table() {
    // Create an array to hold all records
    hashRecord **records = NULL;
    int record_count = 0;
    
    // First, count how many records we have
    for (int i = 0; i < TABLE_SIZE; i++) {
        rwlock_acquire_readlock(&table.locks[i]);
        hashRecord *current = table.buckets[i];
        while (current != NULL) {
            record_count++;
            current = current->next;
        }
        rwlock_release_readlock(&table.locks[i]);
    }
    
    // Allocate memory for the array
    records = malloc(record_count * sizeof(hashRecord *));
    if (records == NULL) {
        fprintf(output_file, "Memory allocation error\n");
        return;
    }
    
    // Fill the array with pointers to all records
    int index = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        rwlock_acquire_readlock(&table.locks[i]);
        hashRecord *current = table.buckets[i];
        while (current != NULL) {
            records[index++] = current;
            current = current->next;
        }
        rwlock_release_readlock(&table.locks[i]);
    }
    
    // Sort the array by hash value
    qsort(records, record_count, sizeof(hashRecord *), compare_hash_records);
    
    // Print summary information
    fprintf(output_file, "\nNumber of lock acquisitions: %d\n", lock_acquisitions);
    fprintf(output_file, "Number of lock releases: %d\n", lock_releases);
    
    // Print all records in sorted order
    for (int i = 0; i < record_count; i++) {
        fprintf(output_file, "%u,%s,%u\n", records[i]->hash, records[i]->name, records[i]->salary);
    }
    
    // Free the array
    free(records);
}

// Process commands from the input file
void *process_command(void *arg) {
    char *cmd = (char *)arg;

    // Make a copy because strtok modifies the string
    char *line_copy = strdup(cmd);
    char *token = strtok(line_copy, ",");

    if (strcmp(token, "insert") == 0) {
        char *name = strtok(NULL, ",");
        char *salary_str = strtok(NULL, ",");
        uint32_t salary = (uint32_t)atoi(salary_str);
        
        // Mark that an insert is in progress
        pthread_mutex_lock(&mutex);
        inserts_in_progress++;
        pthread_mutex_unlock(&mutex);
        
        // Perform the insert
        insert(name, salary);
        
        // Mark that the insert is complete
        pthread_mutex_lock(&mutex);
        inserts_in_progress--;
        if (inserts_in_progress == 0 && deletes_waiting > 0) {
            pthread_cond_broadcast(&inserts_done);
        }
        pthread_mutex_unlock(&mutex);
    } else if (strcmp(token, "delete") == 0) {
        char *name = strtok(NULL, ",");
        delete(name);
    } else if (strcmp(token, "search") == 0) {
        char *name = strtok(NULL, ",");
        search(name);
    } else if (strcmp(token, "print") == 0) {
        print();
    }

    free(line_copy);
    free(cmd);
    return NULL;
}

int main(void) {
    // Open the output file
    output_file = fopen("output.txt", "w");
    if (!output_file) {
        perror("Error opening output.txt");
        return EXIT_FAILURE;
    }

    // Open the commands file
    FILE *file = fopen("commands.txt", "r");
    if (!file) {
        perror("Error opening commands.txt");
        fclose(output_file);
        return EXIT_FAILURE;
    }

    // Initialize the hash table
    initialize_table();
    
    // Initialize the condition variable lock
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&inserts_done, NULL);
    pthread_cond_init(&deletes_done, NULL);

    char line[MAX_LINE_LENGTH];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        fclose(output_file);
        return EXIT_FAILURE;
    }

    // Parse the number of threads
    char *token = strtok(line, ",");
    if (strcmp(token, "threads") != 0) {
        fprintf(stderr, "Error: First line must start with 'threads'\n");
        fclose(file);
        fclose(output_file);
        return EXIT_FAILURE;
    }
    
    token = strtok(NULL, ",");
    int num_threads = atoi(token);

    // Create an array of threads
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (!threads) {
        fclose(file);
        fclose(output_file);
        return EXIT_FAILURE;
    }

    int thread_count = 0;

    // Loops until the specified number of threads is created or there are no more lines to read.
    while (thread_count < num_threads && fgets(line, sizeof(line), file)) {
        // Remove the newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        // Create a copy of the line to pass to the thread
        char *cmd_copy = strdup(line);
        if (!cmd_copy)
            break;

        // Create thread at index thread_count, no attributes, process_command function, and pass cmd_copy as function argument
        if (pthread_create(&threads[thread_count], NULL, process_command, cmd_copy) != 0) {
            free(cmd_copy);
            break;
        }
        thread_count++;
    }

    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++)
        pthread_join(threads[i], NULL);

    // Print the final table and statistics
    print_final_table();

    // Clean up
    free(threads);
    fclose(file);
    fclose(output_file);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&inserts_done);
    pthread_cond_destroy(&deletes_done);
    
    return 0;
}