#ifndef CHASH_H
#define CHASH_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/**
 * @brief Structure to store hash records.
 *
 * This structure represents a record in a hash table, containing
 * a hash value, a name, a salary, and a pointer to the next record
 * in the linked list.
 */
typedef struct hash_struct
{
    uint32_t hash; /**< Value representing the hash of the name */
    char name[50]; /**< String with a maximum length of 50 characters */
    uint32_t salary; /**< Unsigned integer representing the annual salary */
    struct hash_struct *next; /**< Pointer to the next node in the linked list */
} hashRecord;

#endif //CHASH_H