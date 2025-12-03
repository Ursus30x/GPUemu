#ifndef VECTOR_H
#define VECTOR_H
#define MAX_TOKEN_SIZE 10

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>


typedef char Str10[10];
typedef struct {
    Str10* items;   
    size_t size;     
    size_t capacity;  
} Vector;


static inline void vector_init(Vector* vec) {
    vec->items = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

static inline bool vector_push(Vector* vec, const char* str) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        Str10* new_items = (Str10*)realloc(vec->items, new_capacity * sizeof(Str10));
        
        if (new_items == NULL) {
            return false; 
        }
        
        vec->items = new_items;
        vec->capacity = new_capacity;
    }

    strncpy(vec->items[vec->size], str, 9);
    vec->items[vec->size][9] = '\0';
    
    vec->size++;
    return true;
}


static inline char* vector_get(Vector* vec, size_t index) {
    if (index >= vec->size) {
        return NULL;
    }
    return vec->items[index];
}


static inline bool vector_set(Vector* vec, size_t index, const char* str) {
    if (index >= vec->size) {
        return false;
    }
    
    strncpy(vec->items[index], str, 9);
    vec->items[index][9] = '\0';
    return true;
}


static inline void vector_free(Vector* vec) {
    if (vec->items != NULL) {
        free(vec->items);
        vec->items = NULL;
    }
    vec->size = 0;
    vec->capacity = 0;
}


#endif // VECTOR_IMPLEMENTATION
