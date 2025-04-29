#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <algorithm>

using namespace std;

const int MAX_WORDS = 100000;
const int MAX_WORD_LENGTH = 255;
const int NUM_MAPPERS = 4;
const int NUM_REDUCERS = 2;

// Key-Value Pair Struct
struct KeyValuePair {
    char word[MAX_WORD_LENGTH];
    int count;
};

// Mapper Arguments Struct
struct MapperArgs {
    char** input_data;
    int data_size;
    int mapper_id;
};

// Reducer Arguments Struct
struct ReducerArgs {
    struct KeyValuePair* intermediate_data;
    int data_size;
    int reducer_id;
};

// Synchronization primitives
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mapper_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reducer_mutex = PTHREAD_MUTEX_INITIALIZER;

// Semaphores for synchronization
sem_t mapper_complete_sem;    // Signals when all mappers are done
sem_t shuffle_complete_sem;   // Signals when shuffling is done
sem_t reducer_complete_sem;   // Signals when all reducers are done

// Global synchronization flags
int mapper_count = 0;
int reducer_count = 0;
int mapper_done = 0;
int shuffle_done = 0;

// Global shared resources
struct KeyValuePair global_intermediate_results[MAX_WORDS];
int global_intermediate_count = 0;
struct KeyValuePair global_final_results[MAX_WORDS];
int global_final_count = 0;

// Function to remove punctuation and convert to lowercase
void clean_word(const char* input, char* output) {
    int j = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        if (isalnum(input[i])) {
            output[j++] = tolower(input[i]);
        }
    }
    output[j] = '\0';
}

// Map Function: Tokenize and count word occurrences
void* mapper(void* args) {
    struct MapperArgs* map_args = (struct MapperArgs*)args;
    
    pthread_mutex_lock(&mapper_mutex);
    
    cout << "Mapper " << map_args->mapper_id << " processing " 
         << map_args->data_size << " words\n";
    
    // Process each word in the input data
    for (int i = 0; i < map_args->data_size; i++) {
        char clean[MAX_WORD_LENGTH];
        clean_word(map_args->input_data[i], clean);
        
        if (strlen(clean) == 0) continue; // Skip empty strings after cleaning
        
        pthread_mutex_lock(&global_mutex);
        struct KeyValuePair kv;
        strncpy(kv.word, clean, MAX_WORD_LENGTH);
        kv.count = 1;
        
        // Add to global intermediate results
        cout << "Mapper " << map_args->mapper_id << " mapped word: " 
             << kv.word << endl;
        global_intermediate_results[global_intermediate_count++] = kv;
        pthread_mutex_unlock(&global_mutex);
    }
    
    cout << "Mapper " << map_args->mapper_id << " completed. "
         << "Mapped " << global_intermediate_count << " words\n";
    
    // Track mapper completion
    mapper_count++;
    if (mapper_count == NUM_MAPPERS) {
        mapper_done = 1;
        sem_post(&mapper_complete_sem);
    }
    
    pthread_mutex_unlock(&mapper_mutex);
    return NULL;
}

// Shuffle Phase: Group by key
void shuffle() {
    // Wait for all mappers to complete
    sem_wait(&mapper_complete_sem);
    
    pthread_mutex_lock(&global_mutex);
    
    // Sort intermediate results by word
    sort(global_intermediate_results, 
         global_intermediate_results + global_intermediate_count, 
         [](const KeyValuePair& a, const KeyValuePair& b) {
             return strcmp(a.word, b.word) < 0;
         });
    
    // Print shuffled and grouped results before reduction
    cout << "After Shuffle: ";
    for (int i = 0; i < global_intermediate_count - 1; i++) {
        cout << "(\"" << global_intermediate_results[i].word << "\", [1";
        
        // Check for subsequent identical words to show multiple 1's
        int duplicates = 0;
        for (int j = i + 1; j < global_intermediate_count; j++) {
            if (strcmp(global_intermediate_results[i].word, 
                       global_intermediate_results[j].word) == 0) {
                cout << ", 1";
                duplicates++;
            }
        }
        cout << "]) ";
    }
    cout << endl;
    
    // Group and aggregate counts
    for (int i = 0; i < global_intermediate_count - 1; i++) {
        if (strcmp(global_intermediate_results[i].word, 
                   global_intermediate_results[i+1].word) == 0) {
            global_intermediate_results[i].count += 
                global_intermediate_results[i+1].count;
            
            // Shift remaining elements
            for (int j = i+1; j < global_intermediate_count - 1; j++) {
                global_intermediate_results[j] = global_intermediate_results[j+1];
            }
            global_intermediate_count--;
            i--; // Recheck this index
        }
    }
    
    cout << "Shuffle phase completed. Grouped into " 
         << global_intermediate_count << " unique words\n";
    
    shuffle_done = 1;
    pthread_mutex_unlock(&global_mutex);
    
    // Signal that shuffle is complete
    sem_post(&shuffle_complete_sem);
}

// Reduce Function: Aggregate word counts
void* reducer(void* args) {
    struct ReducerArgs* reduce_args = (struct ReducerArgs*)args;
    
    // Wait for shuffle to complete
    sem_wait(&shuffle_complete_sem);
    
    pthread_mutex_lock(&reducer_mutex);
    
    cout << "Reducer " << reduce_args->reducer_id << " processing " 
         << reduce_args->data_size << " word groups\n";
    
    // Aggregate counts for each unique word
    for (int i = 0; i < reduce_args->data_size; i++) {
        pthread_mutex_lock(&global_mutex);
        
        cout << "Reducer " << reduce_args->reducer_id 
             << " reducing word: " << reduce_args->intermediate_data[i].word 
             << endl;
        
        int found = 0;
        for (int j = 0; j < global_final_count; j++) {
            if (strcmp(global_final_results[j].word, 
                       reduce_args->intermediate_data[i].word) == 0) {
                global_final_results[j].count += 
                    reduce_args->intermediate_data[i].count;
                found = 1;
                break;
            }
        }
        
        if (!found) {
            global_final_results[global_final_count++] = 
                reduce_args->intermediate_data[i];
        }
        
        pthread_mutex_unlock(&global_mutex);
    }
    
    cout << "Reducer " << reduce_args->reducer_id << " completed. "
         << "Reduced to " << global_final_count << " final word groups\n";
    
    // Track reducer completion
    reducer_count++;
    if (reducer_count == NUM_REDUCERS) {
        sem_post(&reducer_complete_sem);
    }
    
    pthread_mutex_unlock(&reducer_mutex);
    return NULL;
}

int main() {
    // Initialize all semaphores
    sem_init(&mapper_complete_sem, 0, 0);
    sem_init(&shuffle_complete_sem, 0, 0);
    sem_init(&reducer_complete_sem, 0, 0);
    
    // User input for words
    char* input_data[MAX_WORDS];
    int input_size = 0;
    int input_choice;
    
    cout << "Choose input method:\n";
    cout << "1. Enter words manually\n";
    cout << "2. Read words from file\n";
    cout << "Enter choice (1 or 2): ";
    cin >> input_choice;

    if (input_choice == 1) {
        cout << "Enter words (type 'END' to finish):\n";
        char buffer[MAX_WORD_LENGTH];
        while (1) {
            cin >> buffer;
            if (strcmp(buffer, "END") == 0) break;
            input_data[input_size] = (char*)malloc(MAX_WORD_LENGTH * sizeof(char));
            strncpy(input_data[input_size++], buffer, MAX_WORD_LENGTH);
        }
    } else if (input_choice == 2) {
        char filename[100];
        cout << "Enter file name: ";
        cin >> filename;
        
        FILE* file = fopen(filename, "r");
        if (!file) {
            cerr << "Error opening file\n";
            return 1;
        }

        char buffer[MAX_WORD_LENGTH];
        while (fscanf(file, "%s", buffer) != EOF) {
            input_data[input_size] = (char*)malloc(MAX_WORD_LENGTH * sizeof(char));
            strncpy(input_data[input_size++], buffer, MAX_WORD_LENGTH);
        }
        fclose(file);
    } else {
        cout << "Invalid choice. Exiting.\n";
        return 1;
    }
    
    // Mapper threads
    pthread_t mapper_threads[NUM_MAPPERS];
    struct MapperArgs mapper_args[NUM_MAPPERS];
    
    // Distribute input data across mappers
    int chunk_size = input_size / NUM_MAPPERS;
    for (int i = 0; i < NUM_MAPPERS; i++) {
        mapper_args[i].input_data = &input_data[i * chunk_size];
        mapper_args[i].data_size = (i == NUM_MAPPERS - 1) ? 
            (input_size - i * chunk_size) : chunk_size;
        mapper_args[i].mapper_id = i;
        
        pthread_create(&mapper_threads[i], NULL, mapper, &mapper_args[i]);
    }
    
    // Wait for mappers to complete
    for (int i = 0; i < NUM_MAPPERS; i++) {
        pthread_join(mapper_threads[i], NULL);
    }
    
    // Shuffle Phase
    shuffle();
    
    // Reducer threads
    pthread_t reducer_threads[NUM_REDUCERS];
    struct ReducerArgs reducer_args[NUM_REDUCERS];
    
    // Distribute intermediate results across reducers
    chunk_size = global_intermediate_count / NUM_REDUCERS;
    for (int i = 0; i < NUM_REDUCERS; i++) {
        reducer_args[i].intermediate_data = &global_intermediate_results[i * chunk_size];
        reducer_args[i].data_size = (i == NUM_REDUCERS - 1) ? 
            (global_intermediate_count - i * chunk_size) : chunk_size;
        reducer_args[i].reducer_id = i;
        
        pthread_create(&reducer_threads[i], NULL, reducer, &reducer_args[i]);
    }
    
    // Wait for reducers to complete
    sem_wait(&reducer_complete_sem);
    
    for (int i = 0; i < NUM_REDUCERS; i++) {
        pthread_join(reducer_threads[i], NULL);
    }
    
    // Sort final results alphabetically
    sort(global_final_results, 
         global_final_results + global_final_count, 
         [](const KeyValuePair& a, const KeyValuePair& b) {
             return strcmp(a.word, b.word) < 0;
         });
    
    // Print final results
    cout << "\nFinal Results (Sorted Alphabetically):\n";
    for (int i = 0; i < global_final_count; i++) {
        cout << global_final_results[i].word << ": " 
             << global_final_results[i].count << endl;
    }
    
    // Free allocated memory
    for (int i = 0; i < input_size; i++) {
        free(input_data[i]);
    }
    
    // Cleanup
    pthread_mutex_destroy(&global_mutex);
    pthread_mutex_destroy(&mapper_mutex);
    pthread_mutex_destroy(&reducer_mutex);
    
    sem_destroy(&mapper_complete_sem);
    sem_destroy(&shuffle_complete_sem);
    sem_destroy(&reducer_complete_sem);
    
    return 0;
}
