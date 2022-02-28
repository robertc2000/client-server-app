#include "List.h"

void cons(void* info, TList** head) {
	TList* new_cell = malloc(sizeof(TList));
	if (!new_cell) {
		return;
	}
	new_cell->info = info;
	new_cell->next = *head;
	*head = new_cell;
}

void destroy(TList** head, TFree destroy) {
	while (*head != NULL) {
		TList* tmp = *head;
		*head = (*head)->next;
		destroy(tmp->info);
		free(tmp);
	}
}

void* head(TList* list) {
	if (list != NULL) {
		return list->info;
	}
	return NULL;
}

void remove_elem(TList** list, void* data, eq f, TFree destroy) {
	void* a = (*list)->info;
	TList* aux;
	if (f(data, a)) {
		aux = *list;
		*list = aux->next;
		destroy(aux->info);
		free(aux);
		return;
	}

	TList* pred = *list;
	aux = pred->next;
	while (aux) {
		if (f(aux->info, data)) {
			pred->next = aux->next;
			destroy(aux->info);
			free(aux);
			return;
		}
		pred = pred->next;
		aux = aux->next;
	}
}

int size(TList* list) {
	int size = 0;
	while (list != NULL) {
		size++;
		list = list->next;
	}
	return size;
}