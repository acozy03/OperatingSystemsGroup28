#include "chash.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
