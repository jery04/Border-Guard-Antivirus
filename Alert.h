#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// Estructura para almacenar información de cada proceso
typedef struct ListAlert {
    char nombre[256];     // Nombre del proceso
    char fecha_hora[64];  // Fecha y hora de inserción
    bool tipo;            // 1 cpu o 0 memoria
    struct ListAlert *next;
} ListAlert;

// Variable global que apunta al primer elemento de la lista
ListAlert *node_Alert = NULL;

void obtener_fecha_hora_actual(char *buffer, size_t size) {
    time_t tiempo = time(NULL);
    struct tm *tm_info = localtime(&tiempo);
    strftime(buffer, size, "%d-%m-%Y %H:%M:%S", tm_info);
}

void insertar_Alerta(char *nombre_proceso, bool tipo) {
    ListAlert *nuevo = (ListAlert *) malloc(sizeof(ListAlert));
    if (nuevo == NULL) {
        fprintf(stderr, "Error: no se pudo asignar memoria para el nodo.\n");
        return;
    }

    strncpy(nuevo->nombre, nombre_proceso, sizeof(nuevo->nombre));
    nuevo->nombre[sizeof(nuevo->nombre) - 1] = '\0';  // Seguridad

    nuevo->tipo = tipo;

    obtener_fecha_hora_actual(nuevo->fecha_hora, sizeof(nuevo->fecha_hora));
    nuevo->next = NULL;

    if (node_Alert == NULL) {
        node_Alert = nuevo;
    } else {
        ListAlert *tmp = node_Alert;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = nuevo;
    }
}

void mostrar_lista() {
    ListAlert *actual = node_Alert;
    while (actual != NULL) {
        printf("Proceso: %-20s  Fecha: %s\n", actual->nombre, actual->fecha_hora);
        actual = actual->next;
    }
}

void destruir_Alerta() {
    ListAlert *actual = node_Alert;
    while (actual != NULL) {
        ListAlert *tmp = actual;
        actual = actual->next;
        free(tmp);
    }
    node_Alert = NULL;
}
