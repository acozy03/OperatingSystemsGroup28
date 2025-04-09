#ifndef CHASH_H
#define CHASH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <unistd.h>

#define TABLE_SIZE 100
#define MAX_NAME_LENGTH 50
#define MAX_LINE_LENGTH 256

// Define semaphore macros to match the reference code
#define Sem_init(sem, value) sem_init(sem, 0, value)
#define Sem_wait(sem) sem_wait(sem)
#define Sem_post(sem) sem_post(sem)

// Read/Write Lock Implementation (based on the reference)
typedef struct rwlock_t {
    sem_t writelock;
    sem_t lock;
    int readers;
} rwlock_t;

typedef struct hashRecord {
    uint32_t hash;
    char name[MAX_NAME_LENGTH];
    uint32_t salary;
    struct hashRecord *next;
} hashRecord;

typedef struct hashTable {
    hashRecord* buckets[TABLE_SIZE];
    rwlock_t locks[TABLE_SIZE];
} hashTable;

extern hashTable table;

// Hash table functions
void rwlock_init(rwlock_t *lock);
void rwlock_acquire_readlock(rwlock_t *lock);
void rwlock_release_readlock(rwlock_t *lock);
void rwlock_acquire_writelock(rwlock_t *lock);
void rwlock_release_writelock(rwlock_t *lock);
void initialize_table();
void insert(const char *name, uint32_t salary);
void delete_record(const char *name);
void search(const char *name);
void print_final_table();
uint32_t jenkins_hash(const char *key);
long long get_timestamp();

// For comparing hash records
int compare_hash_records(const void *a, const void *b);

#endif
