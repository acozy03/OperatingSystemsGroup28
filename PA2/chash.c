#include "chash.h"

// Global table instance
hashTable table;

// Jenkins Hash Function
uint32_t jenkins_hash(const char *key)
{
    uint32_t hash = 0;
    while (*key)
    {
        hash += *key++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % TABLE_SIZE;
}

// Initialize Hash Table and Locks
void initialize_table()
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        table.buckets[i] = NULL;
        rwlock_init(&table.locks[i]);
    }
}

// Initialize Read/Write Lock
void rwlock_init(rwlock_t *lock)
{
    lock->readers = 0;
    Sem_init(&lock->lock, 1);
    Sem_init(&lock->writelock, 1);
}

// Acquire and Release Read Lock
void rwlock_acquire_readlock(rwlock_t *lock)
{
    Sem_wait(&lock->lock);
    lock->readers++;
    if (lock->readers == 1)
        Sem_wait(&lock->writelock);
    Sem_post(&lock->lock);
}

void rwlock_release_readlock(rwlock_t *lock)
{
    Sem_wait(&lock->lock);
    lock->readers--;
    if (lock->readers == 0)
        Sem_post(&lock->writelock);
    Sem_post(&lock->lock);
}

// Acquire and Release Write Lock
void rwlock_acquire_writelock(rwlock_t *lock)
{
    Sem_wait(&lock->writelock);
}

void rwlock_release_writelock(rwlock_t *lock)
{
    Sem_post(&lock->writelock);
}

// Insert Record into Hash Table
void insert(const char *name, uint32_t salary)
{
    // Compute the hash for the given key.
    uint32_t hash = jenkins_hash(name);
    // Acquire the **write lock** for the list.
    rwlock_acquire_writelock(&table.locks[hash]);

    // Search the linked list for an existing node with the same hash:
    hashRecord *current = table.buckets[hash];
    while (current)
    {
        // If found, update its value.
        if (strcmp(current->name, name) == 0)
        {
            current->salary = salary;
            printf("Updated: %s with salary %u\n", name, salary);
            rwlock_release_writelock(&table.locks[hash]);
            return;
        }
        current = current->next;
    }

    // Otherwise, create and insert a new node.
    hashRecord *newNode = malloc(sizeof(hashRecord));
    strcpy(newNode->name, name);
    newNode->salary = salary;
    newNode->hash = hash;
    newNode->next = table.buckets[hash];
    table.buckets[hash] = newNode;
    printf("Inserted: %s with salary %u\n", name, salary);

    // Release the write lock and return.
    rwlock_release_writelock(&table.locks[hash]);
}

// Global read/write lock instance
rwlock_t rwlock;

// Delete Record from Hash Table
void delete(const char *name)
{
    // Compute the hash value for the given key.
    uint32_t hash = jenkins_hash(name);

    // Acquire the write lock for the appropriate bucket.
    rwlock_acquire_writelock(&table.locks[hash]);

    // Set up pointers to traverse the linked list.
    hashRecord *current = table.buckets[hash];
    hashRecord *prev = NULL;

    // Search for the record with the matching name.
    while (current != NULL)
    {
        if (strcmp(current->name, name) == 0)
        {
            // If the record is found, remove it from the list.
            if (prev == NULL)
            {
                // Deleting the first node in the list.
                table.buckets[hash] = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            printf("Deleted: %s\n", name);
            free(current);
            rwlock_release_writelock(&table.locks[hash]);
            return;
        }
        prev = current;
        current = current->next;
    }

    // If the record is not found, release the lock and return.
    rwlock_release_writelock(&table.locks[hash]);
}

// Searches for a Record in the Hash Table
void search(const char *name)
{
    uint32_t hash = jenkins_hash(name);          // Compute hash
    rwlock_acquire_readlock(&table.locks[hash]); // Acquire read lock

    hashRecord *current = table.buckets[hash];

    while (current != NULL)
    {
        if (strcmp(current->name, name) == 0)
        {
            printf("SEARCH FOUND: %s with salary %u\n", current->name, current->salary); // Print result
            rwlock_release_readlock(&table.locks[hash]); // Release read lock
            return;
        }
        current = current->next;
    }

    printf("SEARCH: NOT FOUND NOT FOUND\n"); // If key is not found
    rwlock_release_readlock(&table.locks[hash]); // Release read lock
}

// This was made to test the insert and delete commands. Print and search can be added to test implementation
void *process_command(void *arg)
{
    char *cmd = (char *)arg;

    // Make a copy because strtok modifies the string
    char *line_copy = strdup(cmd);
    char *token = strtok(line_copy, ",");

    if (strcmp(token, "insert") == 0)
    {
        char *name = strtok(NULL, ",");
        char *salary_str = strtok(NULL, ",");
        uint32_t salary = (uint32_t)atoi(salary_str);
        insert(name, salary);
    }
    else if (strcmp(token, "delete") == 0)
    {
        char *name = strtok(NULL, ",");
        delete (name);
    }
    else if (strcmp(token, "search") == 0)
    {
        char *name = strtok(NULL, ",");
        search(name);
    }
    else
    {
        // Ignore search and print for now
    }

    free(line_copy);
    free(cmd);
    return NULL;
}

/*
void* process_command(void *arg) {
    char *cmd = (char *)arg;
    // Acquire readlock before processing (since we're just reading tokens)
    rwlock_acquire_readlock(&rwlock);
    char *token = strtok(cmd, ",");
    while (token) {
        printf("Token: %s\n", token);
        token = strtok(NULL, ",");
    }
    rwlock_release_readlock(&rwlock);
    free(cmd);
    return NULL;
}
*/

int main(void)
{
    FILE *file = fopen("commands.txt", "r");
    if (!file)
    {
        perror("Error opening commands.txt");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH];
    if (!fgets(line, sizeof(line), file))
    {
        fclose(file);
        return EXIT_FAILURE;
    }

    // Parse the number of threads
    const char *token = strtok(line, ",");
    token = strtok(NULL, ",");
    const int num_threads = atoi(token);

    // Create an array of threads
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (!threads)
    {
        fclose(file);
        return EXIT_FAILURE;
    }

    // Initialize the rwlock
    rwlock_init(&rwlock);

    int thread_count = 0;

    // Loops until the specified number of threads is created or there are no more lines to read.
    while (thread_count < num_threads && fgets(line, sizeof(line), file))
    {
        // Remove the newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        // Create a copy of the line to pass to the thread
        char *cmd_copy = strdup(line);
        if (!cmd_copy)
            break;

        // Create thread at index thread_count, no attributes, process_command function, and pass cmd_copy as function argument
        if (pthread_create(&threads[thread_count], NULL, process_command, cmd_copy) != 0)
        {
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