#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ------------------------------------------------------------
// 1. Definición de la lista doblemente enlazada (List.h)
// ------------------------------------------------------------
typedef struct List {
    int id;                  // PID del proceso
    char name[256];          // Nombre del proceso ("/proc/[pid]/comm")
    long memory;             // Uso de memoria en KB (VmRSS)
    long cpu_use;            // Ticks de CPU del proceso en la última lectura
    long cpu_total;          // Ticks totales de CPU del sistema en la última lectura
    float cpu_por;           // % de CPU calculado entre lecturas

    struct List *next;       // Puntero al siguiente nodo
    struct List *back;       // Puntero al nodo anterior
} List;

// Cabecera (global) de la lista; apunta siempre al primer nodo
static List *Root = NULL;

// Prototipos de funciones para la lista:
List *CreateNode(int id, const char name[256], long memory, long cpu_use, long cpu_total);
List *InsertEnd(int id, const char name[256], long memory, long cpu_use, long cpu_total);
List *InsertBefore(List *target, int id, const char name[256], long memory, long cpu_use, long cpu_total);
List *Reload(List *node, const char name[256], long memory, long cpu_use, long cpu_total, float cpu_por);
List *Erase(List *node);
void   PrintList(void);
void   FreeList(void);



List *CreateNode(int id, const char name[256], long memory, long cpu_use, long cpu_total) {
    List *node = (List *)malloc(sizeof(List));
    if (!node) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    node->id        = id;
    strncpy(node->name, name, 255);
    node->name[255] = '\0';
    node->memory    = memory;
    node->cpu_use   = cpu_use;
    node->cpu_total = cpu_total;
    node->cpu_por   = 0.0f;
    node->next      = NULL;
    node->back      = NULL;
    return node;
}

List *InsertEnd(int id, const char name[256], long memory, long cpu_use, long cpu_total) {
    List *new_node = CreateNode(id, name, memory, cpu_use, cpu_total);
    if (Root == NULL) {
        Root = new_node;
        return new_node;
    }
    List *current = Root;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = new_node;
    new_node->back = current;
    return new_node;
}

List *InsertBefore(List *target, int id, const char name[256], long memory, long cpu_use, long cpu_total) {
    if (!target) return NULL;
    List *new_node = CreateNode(id, name, memory, cpu_use, cpu_total);
    new_node->next = target;
    new_node->back = target->back;
    if (target->back != NULL) {
        target->back->next = new_node;
    } else {
        // El nodo `target` era el primero; actualizamos Root
        Root = new_node;
    }
    target->back = new_node;
    return new_node;
}

List *Reload(List *node, const char name[256], long memory, long cpu_use, long cpu_total, float cpu_por) {
    if (!node) return NULL;
    strncpy(node->name, name, 255);
    node->name[255] = '\0';
    node->memory    = memory;
    node->cpu_use   = cpu_use;
    node->cpu_total = cpu_total;
    node->cpu_por   = cpu_por;
    return node->next;
}

List *Erase(List *node) {
    if (!node) return NULL;
    List *next_node = node->next;
    if (node->back) {
        node->back->next = node->next;
    } else {
        // Si no hay “back”, el nodo era el primer elemento
        Root = node->next;
    }
    if (node->next) {
        node->next->back = node->back;
    }
    free(node);
    return next_node;
}

void PrintList(void) {
    List *current = Root;
    printf("===== Estado de la lista de procesos =====\n");
    while (current != NULL) {
        printf("PID: %d | Nombre: %-20s | Memoria: %ld KB | CPU_uso: %ld | CPU_tot: %ld | CPU_%%: %.2f\n",
               current->id, current->name, current->memory,
               current->cpu_use, current->cpu_total, current->cpu_por);
        current = current->next;
    }
    printf("===========================================\n");
}

void FreeList(void) {
    List *current = Root;
    while (current != NULL) {
        List *siguiente = current->next;
        free(current);
        current = siguiente;
    }
    Root = NULL;
}