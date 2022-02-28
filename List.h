#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef int (*eq)(void*, void*);
typedef void (*TFree) (void*);

typedef struct TList {
	void* info;
	struct TList* next;
} TList;

void cons(void* info, TList** head);

void destroy(TList** head, TFree destroy);

void* head(TList* list);

void remove_elem(TList** list, void* data, eq f, TFree destroy);

int size(TList* list);