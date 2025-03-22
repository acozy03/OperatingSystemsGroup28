#include <stdio.h>
#include <stdlib.h>
#include  "chash.h"

int main(void) {
    printf("Hello, World!!!\n");
    hashRecord *record = (hashRecord *) malloc(sizeof(hashRecord));
    record->hash = 0;
    record->salary = 0;
    record->next = NULL;
    printf("record->hash: %d\n", record->hash);
    printf("record->salary: %d\n", record->salary);
    printf("record->next: %p\n", record->next);
    free(record);


    return 0;
}