# Multithreaded-MapReduce

# Basic MapReduce Framework

**Authors:** Muhammad Qasim, Ayaan Khan, Ahmad Luqman

## Overview
This project implements a simplified MapReduce framework in C/C++ to demonstrate parallel data processing using operating system concepts such as threading, synchronization, and inter-thread communication.

## Features
- **Map Phase:** Splits input into chunks and processes them in parallel using multiple mapper threads.
- **Shuffle Phase:** Groups and sorts intermediate key-value pairs by key.
- **Reduce Phase:** Aggregates counts for each unique key using multiple reducer threads.
- **Concurrency:** Uses POSIX threads (`pthread`), mutexes, and semaphores to coordinate work and ensure thread safety.
- **Flexible Input:** Supports both manual text entry and file-based inputs.

## Prerequisites
- GCC or any C/C++ compiler with pthread support
- POSIX-compliant OS (Linux/macOS)
- Make (optional, if using provided Makefile)

## Repository Structure
```
├── src/
│   ├── mapper.c        # Mapper implementation (threaded)
│   ├── shuffler.c      # Shuffle logic
│   ├── reducer.c       # Reducer implementation
│   ├── common.h        # Shared data structures and utilities
│   └── Makefile        # Build rules
├── tests/              # Sample input files and expected outputs
├── diagrams/           # Sequence diagrams and flowcharts
└── README.md           # Project documentation
```

## Building the Project
1. Clone the repository:
   ```bash
   git clone <repo_url>
   cd <repo_dir>/src
   ```
2. Compile using Make:
   ```bash
   make all
   ```
   Or compile manually:
   ```bash
   gcc -pthread mapper.c -o mapper
   gcc -pthread shuffler.c -o shuffler
   gcc -pthread reducer.c -o reducer
   ```

## Running the Framework
1. **Manual Input:**
   ```bash
   ./mapper            # Enter text manually
   ./shuffler          # Processes mapper output
   ./reducer           # Aggregates and displays final counts
   ```
2. **File Input:**
   ```bash
   ./mapper input.txt  # Processes file `input.txt`
   ./shuffler
   ./reducer
   ```

## Project Phases
1. **Mapping:**
   - Threads preprocess text (lowercase conversion, punctuation removal).
   - Emit `(word, 1)` pairs for each token.
2. **Shuffling:**
   - Sorts intermediate pairs by key.
   - Groups lists of counts under each unique word.
3. **Reducing:**
   - Reducer threads sum counts for each key.
   - Produce final frequency output.

## Concurrency & Synchronization
- **Threads:** Mappers and reducers use `pthread_create` for parallelism.
- **Mutexes:** Protect shared buffers during read/write.
- **Semaphores:** Coordinate phase transitions and ensure all mappers finish before shuffling, and shuffling completes before reducing.

## Input Preprocessing
- Converts all characters to lowercase.
- Strips punctuation characters.
- Tokenizes text on whitespace.

## Testing
- Sample test cases are provided in the `tests/` directory covering single words, repeated words, mixed case, numerics, special characters, long sentences, and large inputs.
- Run each phase sequentially against test files and compare outputs to expected results.

## Diagrams
- **Sequence Diagram:** `diagrams/sequence_mapreduce.png`
- **Flowchart:** `diagrams/flowchart_mapreduce.png`


