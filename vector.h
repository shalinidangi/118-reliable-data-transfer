#ifndef VECTOR_H
#define VECTOR_H

#include <stdio.h>
#include <stdlib.h>
#define VECTOR_INIT_CAPACITY 4

#define VECTOR_INIT(vec) vector vec; vector_init(&vec)
#define VECTOR_ADD(vec, item) vector_add(&vec, (void *) item)
#define VECTOR_SET(vec, id, item) vector_set(&vec, id, (void *) item)
#define VECTOR_GET(vec, type, id) (type) vector_get(&vec, id)
#define VECTOR_EXISTS(vec, item) vector_exists(&vec, (void *) item)
#define VECTOR_DELETE(vec, id) vector_delete(&vec, id)
#define VECTOR_TOTAL(vec) vector_total(&vec)
#define VECTOR_FREE(vec) vector_free(&vec)

/**
 * Implementation taken from Edd Mann with minor bug fixes made by Breanna 
 * https://eddmann.com/posts/implementing-a-dynamic-vector-array-in-c/
 */

typedef struct vector {
    void **items;
    int capacity;
    int total;
} vector;

void vector_init(vector *v)
{
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->items = malloc(sizeof(void *) * v->capacity);
}

int vector_total(vector *v)
{
    return v->total;
}

static void vector_resize(vector *v, int capacity)
{
    #ifdef DEBUG_ON
    printf("vector_resize: %d to %d\n", v->capacity, capacity);
    #endif

    void **items = realloc(v->items, sizeof(void *) * capacity);
    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}

void vector_add(vector *v, void *item)
{
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);
    v->items[v->total++] = item;
}

void vector_set(vector *v, int index, void *item)
{
    if (index >= 0 && index < v->total)
        v->items[index] = item;
}

void *vector_get(vector *v, int index)
{
    if (index >= 0 && index < v->total)
        return v->items[index];
    return NULL;
}

// Note: Vector exists can only check items of type int
int vector_exists(vector *v, void *item)
{
  int i; 
  for (i = 0; i < v->total; i++) {
    if (*((int*) v->items[i]) == *((int*) item)) {
      return i;
    }
  }
  return -1;
}

void vector_delete(vector *v, int index)
{
    if (index < 0 || index >= v->total)
        return;

    int i, j;
    for (i = index + 1, j = index; i < v->total; i++) {
        v->items[j] = v->items[i];
        ++j;
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
}

void vector_free(vector *v)
{
    free(v->items);
}

#endif