#define _GNU_SOURCE   
#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdio.h>
#include <dirent.h>
#include <signal.h>
#include <gtk/gtk.h>
#include "List.h"
#include "Alert.h"


#define MAX_CPU          70   // Porcentaje de CPU permitido
#define MAX_TIME_CPU     30    // Número de iteraciones consecutivas antes de alerta
static int prosses_max_cpu = 0;     // PID que más CPU está consumiendo en iteración actual
static int time_max_cpu    = 0;     // Conteo de iteraciones consecutivas
 
#define MAX_MEMORY       2000    // Memoria en MB (se compara con VmRSS en KB; se hace conversión)
#define MAX_TIME_MEMORY  10
static int prosses_max_memory = 0;  // PID que más memoria está consumiendo en iteración actual
static int time_max_memory    = 0;  // Conteo de iteraciones consecutivas
 
// “Lista blanca” de nombres de procesos que no deben “alertar” aunque excedan umbrales.
static const char *white_list[] = { "main" };
static const int tam_white_list = 1;

//Variable global para la ventana principal
GtkWindow *main_window = NULL;

//Prototipos de las funciones
char *leer_nombre(long id);
long leer_cpu_proc(long id);
long leer_cpu_total(void);
float calc_cpu(long proc_old, long proc_new, long total_old, long total_new);
long leer_memoria(long id);
void max_cpu(int id, char* nombre);
void max_memory(int id, char* nombre);
int matar_proceso(pid_t pid);
gboolean mostrar_alerta_callback(gpointer data);
void *read_proc_thread(void *arg);

// ------------------------------------------------------------
// Funciones para obtener información de /proc
// ------------------------------------------------------------

char *leer_nombre(long id) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/comm", id);
    FILE *file = fopen(path, "r");
    if (!file) {
        return NULL;  // No existe o sin permisos
    }

    static char name[256];
    if (fgets(name, sizeof(name), file)) {
        name[strcspn(name, "\n")] = '\0';  // Quitar salto de línea
    } else {
        name[0] = '\0';
    }
    fclose(file);
    return name;
}

long leer_cpu_proc(long id) {
    unsigned long utime = 0, stime = 0;
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/stat", id);

    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;  // Proceso pudo haber terminado
    }

    char line[1024];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    // Tokenizar la línea en espacios; interesa el campo 14 (utime) y 15 (stime)
    int count = 1;
    char *token = strtok(line, " ");
    while (token != NULL) {
        if (count == 14) {
            utime = strtoul(token, NULL, 10);
        } else if (count == 15) {
            stime = strtoul(token, NULL, 10);
            break;  // Ya leímos utime y stime
        }
        token = strtok(NULL, " ");
        count++;
    }
    return (long)(utime + stime);
}
 
long leer_cpu_total(void) {
    unsigned long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
    FILE *file = fopen("/proc/stat", "r");
    if (!file) {
        return 0;
    }

    char line[256];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    // La línea comienza con "cpu  %lu %lu %lu %lu %lu %lu %lu"
    sscanf(line, "cpu  %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);

    return (long)(user + nice + system + idle + iowait + irq + softirq);
}
 
float calc_cpu(long proc_old, long proc_new, long total_old, long total_new) {
    long proc_diff  = proc_new  - proc_old;
    long total_diff = total_new - total_old;
    if (proc_diff <= 0 || total_diff <= 0) {
        return 0.0f;
    }
    return ((float)proc_diff / (float)total_diff) * 100.0f;
}
 
long leer_memoria(long id) {
    long memoria = 0;
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/status", id);

    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            // Avanzar hasta el primer dígito
            char *ptr = line;
            while (*ptr && (*ptr < '0' || *ptr > '9')) {
                ptr++;
            }
            sscanf(ptr, "%ld", &memoria);
            break;
        }
    }
    fclose(file);
    return memoria;  // En KB
}
 
void max_cpu(int id, char *nombre) {
    if (id == prosses_max_cpu) {
        time_max_cpu++;
        if (time_max_cpu > MAX_TIME_CPU) {
            insertar_Alerta(nombre, 1);
            matar_proceso(id);
            char mensaje[256];
            snprintf(mensaje, sizeof(mensaje), "El proceso %s se ha finalizado\n por superar los umbrales de cpu.", nombre);
            char *parametro = g_strdup(mensaje);
            g_main_context_invoke_full(NULL, G_PRIORITY_HIGH, mostrar_alerta_callback, parametro, NULL);
            time_max_cpu = 0;
        }
    } else {
        prosses_max_cpu = id;
        time_max_cpu    = 1;
    }
}
 
void max_memory(int id, char *nombre) {
    if (id == prosses_max_memory) {
        time_max_memory++;
        if (time_max_memory > MAX_TIME_MEMORY) {
            insertar_Alerta(nombre, 0);
            matar_proceso(id);
            char mensaje[256];
            snprintf(mensaje, sizeof(mensaje), "El proceso %s se ha finalizado\n por superar los umbrales de memoria.", nombre);
            char *parametro = g_strdup(mensaje);
            g_main_context_invoke_full(NULL, G_PRIORITY_HIGH, mostrar_alerta_callback, parametro, NULL);
            time_max_memory = 0;
        }
    } else {
        prosses_max_memory = id;
        time_max_memory    = 1;
    }
}
 
int matar_proceso(int pid) {
   if (kill(pid, SIGKILL) == 0) {
       return 0;
   } else {
       return -1;
   }
}

gboolean mostrar_alerta_callback(gpointer data) {
    char *mensaje = (char *)data;

    GtkWidget *dialog = gtk_message_dialog_new(
        main_window, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", mensaje);

    gtk_window_set_title(GTK_WINDOW(dialog), "Alerta");

    // 🔑 Conectar la señal de respuesta (OK, cerrar ventana) a gtk_widget_destroy
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    gtk_widget_show_all(dialog);
    return FALSE; // no repetir
}

void *read_proc_thread(void *arg) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    char *nombre;
    long cpu_proc, mem_kb, cpu_tot;
    float cpu_pct;
    int is_white;

    while (1) {
        DIR *dir = opendir("/proc");
        if (!dir) {
            perror("opendir(/proc)");
            pthread_exit(NULL);
        }

        struct dirent *entry;

        pthread_mutex_lock(mutex);

        // Puntero “cursor” para recorrer la lista
        List *cursor = Root;
        // Creamos un puntero auxiliar a la “cabeza real” de la lista
        // Con esto, modificamos Root dentro del bucle (si es necesario)
        // Al entrar, asumimos que todos los nodos en Root son “potenciales” para ser eliminados.
        // El BUCLE1 recorre /proc e inserta/actualiza nodos.
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_DIR) {
                continue;
            }
            // ¿El nombre del directorio es un número? (PID)
            char *endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr != '\0') {
                continue;  // No es un PID válido
            }
            // Leemos datos del proceso:
            nombre   = leer_nombre(pid);
            if (!nombre) {
                continue;  // Proceso pudo haber finalizado antes de leer nombre
            }
            cpu_proc = leer_cpu_proc(pid);
            mem_kb   = leer_memoria(pid);
            cpu_tot  = leer_cpu_total();

            // Verificar si el nombre está en white_list
            is_white = 0;
            for (int i = 0; i < tam_white_list; i++) {
                if (strcmp(nombre, white_list[i]) == 0) {
                    is_white = 1;
                    break;
                }
            }

            // Conversión de umbral de memoria: MAX_MEMORY MB a KB
            long umbral_mem_kb = (long)MAX_MEMORY * 1024;
            

            // Caso: lista vacía → insertar directamente
            if (Root == NULL) {
                Root = InsertEnd((int)pid, nombre, mem_kb, cpu_proc, cpu_tot);
                continue;
            }

            // Recorremos la lista (ordenada por PID) buscando la posición de `pid`.
            // Sacamos la lógica de “avanzo o borro o recargo o inserto antes”:
            cursor = Root;
            while (cursor != NULL && cursor->id < pid) {
                cursor = cursor->next;
            }

            if (cursor != NULL && cursor->id == pid) {
                // El proceso ya existía en la lista: recalculamos %CPU, recargamos datos
                cpu_pct = calc_cpu(cursor->cpu_use, cpu_proc,
                                   cursor->cpu_total, cpu_tot);
                // Si supera umbral de CPU y no está en white_list → llamar max_cpu
                if (cpu_pct > MAX_CPU && !is_white) {
                    max_cpu((int)pid, nombre);
                }
                // Si supera umbral de memoria y no está en white_list → llamar max_memory
                if (mem_kb > umbral_mem_kb && !is_white) {
                    max_memory((int)pid, nombre);
                }
                // Recargar campos en el nodo actual
                cursor = Reload(cursor, nombre, mem_kb, cpu_proc, cpu_tot, cpu_pct);
                // cursor pasa a apuntar al siguiente nodo en la lista
            } else {
                // No existe nodo con PID == pid. Hay dos casos:
                //    - cursor == NULL       → pid es mayor que todos en la lista → InsertEnd
                //    - cursor->id > pid      → Debemos insertar “antes” de cursor
                if (cursor == NULL) {
                    // Insertar al final
                    List *nuevo = InsertEnd((int)pid, nombre, mem_kb, cpu_proc, cpu_tot);
                    // nada más que hacer con "cursor"
                } else {
                    // Insertar antes de cursor
                    List *nuevo = InsertBefore(cursor, (int)pid, nombre, mem_kb, cpu_proc, cpu_tot);
                    // El InsertBefore devuelve el nodo que seguía a "cursor" (que era cursor),
                    // pero no necesitamos reasignar cursor porque vamos a avanzar en /proc por distintos PIDs.
                }
            }
        }

        // Al terminar de leer TODOS los PIDs de /proc, puede haber en la lista “antiguos” PIDs ya no existentes.
        // Para identificarlos rápidamente, basta con recorrer la lista y verificar si cada PID sigue existiendo
        // (con acceso a /proc/[pid]). Una forma sencilla: volvemos a recorrer la lista y, para cada nodo,
        // intentamos abrir "/proc/[pid]". Si falla, significa que el proceso ya no existe → borrarlo.
        List *iter = Root;
        while (iter != NULL) {
            List *siguiente = iter->next;
            char proc_path[64];
            snprintf(proc_path, sizeof(proc_path), "/proc/%d", iter->id);
            DIR *chk = opendir(proc_path);
            if (!chk) {
                // El proceso terminó → eliminar nodo
                iter = Erase(iter);  // Erase devuelve el nodo siguiente
                if (!iter) {
                    break;  // Llegamos al final de la lista
                }
            } else {
                closedir(chk);
                iter = siguiente;
            }
        }

        pthread_mutex_unlock(mutex);
        closedir(dir);

        sleep(1);
    }

    pthread_exit(NULL);
}
 