PA2 - Concurrent Hash Table

Files:
  - chash.c        : Main file. Reads the command file, creates threads, and manages the overall execution.
  - chash_ops.c    : Contains the core hash table operations (insertion, deletion, search, final print, locking functions, etc.).
  - chash.h        : Header file with declarations for data structures, globals, and functions used in the project.
  - Makefile       : Build instructions. The project compiles with: "make"
  - output.txt     : Output file generated after running the executable (do not edit manually).
  - README.txt     : This file.

Compilation & Execution:
  1. Open a terminal in the project directory.
  2. Run "make" to build the executable (requires gcc and pthread support).
  3. Ensure the "commands.txt" file is in the same directory.
  4. Run the executable "./chash" and check the generated "output.txt" for results.

AI Attribution:
  I used ChatGPT as an AI assistant to help refactor the original code, create the modular file structure, and resolve compilation issues related to shared globals. The prompts included requests to split the code into separate files and to provide a simple README. All AI assistance was used as a supplementary tool for organization and clarity without altering the core logic of the program.

