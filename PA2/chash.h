#ifndef CHASH_H
#define CHASH_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef struct hash_struct
{
    uint32_t hash; // Value produced by hashing name
    char name[50]; // String Max Length 50
    uint32_t salary; // Non-negative value representing annual salary
    struct hash_struct *next; // linked list next node
} hashRecord;

#endif //CHASH_H
