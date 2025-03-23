#include <stdio.h>
#include <stdlib.h>
#include  "chash.h"

#include <string.h>
#define MAX_LINE_LENGTH 256

int main(void) {
    FILE *file = fopen("commands.txt", "r");
    if (!file) {
        perror("Error opening commands.txt");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH];

    // Read the threads configuration line first
    if (fgets(line, sizeof(line), file) != NULL) {
        // Print out the threads config line
        printf("Threads config: %s", line);
    } else {
        printf("No threads configuration found.\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    // Process each subsequent command line
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove trailing newline character, if present
        line[strcspn(line, "\n")] = 0;

        // Tokenize the line by comma
        char *token = strtok(line, ",");
        while (token != NULL) {
            // Print each token from the command line
            printf("Token: %s\n", token);
            token = strtok(NULL, ",");
        }
        printf("\n");
    }

    fclose(file);
    return 0;
}
