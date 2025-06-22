#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <gtk/gtk.h>

// Define constantes para eventos de inotify y límites de archivos
#define MAX_MENSAJES 300
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * EVENT_SIZE)
#define MAX_PATH_LENGTH PATH_MAX
#define MAX_WATCHES 1024
#define MAX_FILES 10000

// Estructura para almacenar alertas y su fecha y hora de llegada
typedef struct {
    char mensaje[256];
    struct tm tiempo;
} Mensaje;

// Estructuras para almacenar información de archivos y monitoreo
typedef struct {
    char path[MAX_PATH_LENGTH];
    unsigned char sha256[SHA256_DIGEST_LENGTH];
    off_t size;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    time_t mtime;
} file_info;

// Estructura para almacenar información de la línea base
typedef struct {
    file_info files[MAX_FILES];
    int count;
} baseline_info;

// Estructura para almacenar información de monitoreo
typedef struct {
    int wd;
    char path[MAX_PATH_LENGTH];
} watch_entry;

// Estructura para almacenar información del monitor
typedef struct {
    int fd;
    watch_entry watches[MAX_WATCHES];
    int watch_count;
} monitor_info;

baseline_info baseline;         // Estructura global para almacenar la línea base
Mensaje mensajes[MAX_MENSAJES]; // Arreglo para almacenar mensajes de alerta
int mensaje_count = 0;          // Contador de mensajes de alerta

// Función para mostrar un aviso en GTK
gboolean show_warning(gpointer data) {
    char *mensaje = (char *)data;
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_WARNING,
                                    GTK_BUTTONS_OK,
                                    "%s", mensaje);

    gtk_window_set_title(GTK_WINDOW(dialog), "Alerta");
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(dialog);
    return FALSE; // no repetir
}

// Función para agregar un mensaje
void add_message(Mensaje mensajes[], int *contador, const char *nuevoMensaje) {
    if (*contador < MAX_MENSAJES) {
        time_t ahora = time(NULL);  // Obtener tiempo actual
        mensajes[*contador].tiempo = *localtime(&ahora);  // Guardar fecha y hora

        strncpy(mensajes[*contador].mensaje, nuevoMensaje, sizeof(mensajes[*contador].mensaje) - 1);
        mensajes[*contador].mensaje[sizeof(mensajes[*contador].mensaje) - 1] = '\0';  // Asegurar terminación de string

        (*contador)++;  // Aumentar el contador
    } else {
        printf("El almacenamiento de mensajes está lleno.\n");
    }
}

// Función para calcular el hash SHA-256 de un archivo
int calc_sha256(const char* path, unsigned char* hash) {
    FILE* file = fopen(path, "rb");
    if (!file) return -1;
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) { fclose(file); return -1; }
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) { EVP_MD_CTX_free(mdctx); fclose(file); return -1; }
    unsigned char buf[4096];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), file)) != 0) {
        if (EVP_DigestUpdate(mdctx, buf, bytes) != 1) { EVP_MD_CTX_free(mdctx); fclose(file); return -1; }
    }
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) { EVP_MD_CTX_free(mdctx); fclose(file); return -1; }
    EVP_MD_CTX_free(mdctx);
    fclose(file);
    return 0;
}

// Función para obtener información de un archivo
void get_file_info(const char* path, file_info* fi) {
    struct stat st;
    if (stat(path, &st) == 0) {
        strncpy(fi->path, path, MAX_PATH_LENGTH);
        fi->size = st.st_size;
        fi->mode = st.st_mode;
        fi->uid = st.st_uid;
        fi->gid = st.st_gid;
        fi->mtime = st.st_mtime;
        calc_sha256(path, fi->sha256);
    }
}

// Función para construir la línea base de archivos en un directorio
void build_baseline(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                build_baseline(path);
            } else if (S_ISREG(st.st_mode) && baseline.count < MAX_FILES) {
                get_file_info(path, &baseline.files[baseline.count++]);
            }
        }
    }
    closedir(dir);
}

// Función para buscar un archivo en la línea base por su ruta
file_info* find_in_baseline(const char* path) {
    for (int i = 0; i < baseline.count; ++i) {
        if (strcmp(baseline.files[i].path, path) == 0) return &baseline.files[i];
    }
    return NULL;
}

// Busca en el baseline un archivo con el mismo nombre base y ruta, pero diferente extensión
file_info* find_same_base_diff_ext(const char* full_path) {
    char dir[MAX_PATH_LENGTH], base_name[MAX_PATH_LENGTH], *dot;
    strncpy(dir, full_path, MAX_PATH_LENGTH);
    char* slash = strrchr(dir, '/');
    if (!slash) return NULL;
    strcpy(base_name, slash + 1);
    dot = strrchr(base_name, '.');
    if (dot) *dot = '\0'; // quita extensión

    *slash = '\0'; // termina dir en el último slash

    for (int i = 0; i < baseline.count; ++i) {
        const char* base_file = baseline.files[i].path;
        char base_dir[MAX_PATH_LENGTH], base_file_name[MAX_PATH_LENGTH];
        strncpy(base_dir, base_file, MAX_PATH_LENGTH);
        char* base_slash = strrchr(base_dir, '/');
        if (!base_slash) continue;
        strcpy(base_file_name, base_slash + 1);
        char* base_dot = strrchr(base_file_name, '.');
        if (base_dot) *base_dot = '\0';

        *base_slash = '\0';

        if (strcmp(dir, base_dir) == 0 && strcmp(base_name, base_file_name) == 0) {
            // Mismo directorio y mismo nombre base, diferente extensión
            return &baseline.files[i];
        }
    }
    return NULL;
}

// Función para eliminar un archivo de la línea base por su índice
void remove_from_baseline(int idx) {
    if (idx < 0 || idx >= baseline.count) return;
    for (int j = idx; j < baseline.count - 1; ++j) {
        baseline.files[j] = baseline.files[j + 1];
    }
    baseline.count--;
}

// Elimina de la base todos los archivos y subcarpetas bajo un directorio dado
void remove_dir_from_baseline(const char* dir_path) {
    int len = strlen(dir_path);
    for (int idx = 0; idx < baseline.count; ) {
        if (strncmp(baseline.files[idx].path, dir_path, len) == 0 &&
            (baseline.files[idx].path[len] == '/' || baseline.files[idx].path[len] == '\0')) {
            remove_from_baseline(idx);
            // No incrementar idx, ya que los elementos se recorren
        } else {
            idx++;
        }
    }
}

// Función para verificar si un archivo es una copia de otro existente en la línea base
int is_copy_of_existing(const file_info* fi) {
    for (int i = 0; i < baseline.count; ++i) {
        const char* name1 = strrchr(baseline.files[i].path, '/');
        const char* name2 = strrchr(fi->path, '/');
        name1 = name1 ? name1 + 1 : baseline.files[i].path;
        name2 = name2 ? name2 + 1 : fi->path;

        // Compara nombres de archivos(deben diferir) y verifica si el SHA256 y tamaño coinciden
        if (strcmp(name1, name2) != 0 &&
            memcmp(baseline.files[i].sha256, fi->sha256, SHA256_DIGEST_LENGTH) == 0 &&
            baseline.files[i].size == fi->size) {
            return 1;
        }
    }
    return 0;
}

// Función para obtener la ruta asociada a un watch descriptor
const char* get_path_by_wd(monitor_info* monitor, int wd) {
    for (int i = 0; i < monitor->watch_count; ++i) {
        if (monitor->watches[i].wd == wd) return monitor->watches[i].path;
    }
    return NULL;
}

// Función para imprimir el propietario de un archivo dado su uid y gid
void print_owner(uid_t uid, gid_t gid) {
    struct passwd *pw = getpwuid(uid);
    struct group *gr = getgrgid(gid);
    printf("Owner: %s:%s\n", pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");
}

// Función para agregar un watch recursivamente a un directorio
void add_watch_recursive(monitor_info* monitor, const char* dir_path) {
    if (monitor->watch_count >= MAX_WATCHES) return;
    int wd = inotify_add_watch(monitor->fd, dir_path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_ATTRIB | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd == -1) return;
    monitor->watches[monitor->watch_count].wd = wd;
    strncpy(monitor->watches[monitor->watch_count].path, dir_path, MAX_PATH_LENGTH);
    monitor->watch_count++;
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char sub_path[MAX_PATH_LENGTH];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(sub_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            add_watch_recursive(monitor, sub_path);
        }
    }
    closedir(dir);
}

// Maneja los eventos de inotify y realiza las acciones correspondientes
void handle_events(monitor_info* monitor) {
    char buffer[EVENT_BUF_LEN];
    int length = read(monitor->fd, buffer, EVENT_BUF_LEN);
    if (length < 0) return;
    int i = 0;

    // Procesar cada evento en el buffer
    while (i < length) {
        struct inotify_event* event = (struct inotify_event*)&buffer[i];
        const char* dir_path = get_path_by_wd(monitor, event->wd);
        if (dir_path && event->len) {
            char full_path[MAX_PATH_LENGTH];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, event->name);
            struct stat st;
            int stat_ok = (stat(full_path, &st) == 0);
            int is_file = stat_ok && S_ISREG(st.st_mode);
            int is_dir = stat_ok && S_ISDIR(st.st_mode);
            file_info fi;
            char alerta[5000];
            if (is_file) get_file_info(full_path, &fi);

            file_info* base = find_in_baseline(full_path);

            // Detectar nuevas carpetas
            if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Nueva carpeta creada: %s", full_path);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
                add_watch_recursive(monitor, full_path);
            }

            // Detectar carpetas borradas
            if ((event->mask & IN_DELETE) && (event->mask & IN_ISDIR)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Carpeta eliminada: %s", full_path);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
                remove_dir_from_baseline(full_path); // Borra de la línea base
            }

            // Crecimiento inusual de tamaño
            if (base && is_file && (fi.size - base->size) > (100 * 1024 * 1024)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Crecimiento inusual de tamaño en %s (%.2f KB -> %.2f KB)", full_path, base->size / 1024.0, fi.size / 1024.0);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
            }

            // Cambio de extensión
            if (!base && is_file) {
                file_info* base_ext = find_same_base_diff_ext(full_path);
                if (base_ext) { 
                    const char* ext_old = strrchr(base_ext->path, '.');
                    const char* ext_new = strrchr(full_path, '.');
                    if (ext_old && ext_new && strcmp(ext_old, ext_new) != 0) {
                        snprintf(alerta, sizeof(alerta), "ALERTA: Cambio de extensión en %s (de %s a %s)", full_path, ext_old, ext_new);
                        char *parametro = g_strdup(alerta);
                        g_main_context_invoke_full(
                            NULL,
                            G_PRIORITY_HIGH,
                            show_warning,
                            parametro,
                            NULL
                        );
                        add_message(mensajes, &mensaje_count, alerta);
                        strncpy(base_ext->path, full_path, MAX_PATH_LENGTH);
                    }
                }
            }

            // Permisos modificados
            if (base && is_file) {
                if ((base->mode & 0777) != (fi.mode & 0777)) {
                    snprintf(alerta, sizeof(alerta), "ALERTA: Cambio de permisos en %s (de %o a %o)", full_path, base->mode & 0777, fi.mode & 0777);
                    char *parametro = g_strdup(alerta);
                    g_main_context_invoke_full(
                        NULL,
                        G_PRIORITY_HIGH,
                        show_warning,
                        parametro,
                        NULL
                    );
                    add_message(mensajes, &mensaje_count, alerta);
                    base->mode = fi.mode; // Actualiza los permisos en el baseline
                }
            }

            // Replicación de archivos
            if ((event->mask & IN_CREATE) && is_file && is_copy_of_existing(&fi)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Archivo replicado detectado: %s", full_path);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
                if (!base && baseline.count < MAX_FILES) {
                    baseline.files[baseline.count++] = fi;
                    base = &baseline.files[baseline.count - 1]; // Actualiza el puntero base
                }
            }

            // Atributos modificados
            if (base && is_file) {
                // Verificar si el archivo ha cambiado de propietario o timestamp
                if (base->mtime != fi.mtime) { 
                    snprintf(alerta, sizeof(alerta), "ALERTA: Timestamp modificado en %s", full_path);
                    char *parametro = g_strdup(alerta);
                    g_main_context_invoke_full(
                        NULL,
                        G_PRIORITY_HIGH,
                        show_warning,
                        parametro,
                        NULL
                    );
                    add_message(mensajes, &mensaje_count, alerta);
                }
                // Verificar si el archivo ha cambiado de propietario
                if (base->uid != fi.uid || base->gid != fi.gid) {
                    snprintf(alerta, sizeof(alerta), "ALERTA: Cambio de ownership en %s", full_path);
                    char *parametro = g_strdup(alerta);
                    g_main_context_invoke_full(
                        NULL,
                        G_PRIORITY_HIGH,
                        show_warning,
                        parametro,
                        NULL
                    );
                    add_message(mensajes, &mensaje_count, alerta);
                }
            }

            // Manejo de eventos de creación, eliminación y modificación de archivos
            if ((event->mask & IN_CREATE) && is_file && !base && baseline.count < MAX_FILES) {
                baseline.files[baseline.count++] = fi;
                snprintf(alerta, sizeof(alerta), "Nuevo archivo: %s", full_path);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
            }
            if ((event->mask & IN_DELETE) && base) {
                snprintf(alerta, sizeof(alerta), "Archivo eliminado: %s", full_path);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
                remove_from_baseline(base - baseline.files);
            }
            if ((event->mask & IN_MODIFY) && is_file && base && memcmp(base->sha256, fi.sha256, SHA256_DIGEST_LENGTH) != 0) {
                snprintf(alerta, sizeof(alerta), "Archivo modificado: %s", full_path);
                char *parametro = g_strdup(alerta);
                g_main_context_invoke_full(
                    NULL,
                    G_PRIORITY_HIGH,
                    show_warning,
                    parametro,
                    NULL
                );
                add_message(mensajes, &mensaje_count, alerta);
                *base = fi;
            }
        }
        i += EVENT_SIZE + event->len; // Avanzar al siguiente evento
    }
}

// Método Principal
void *detection_scan_thread_func(void *arg) {
    monitor_info monitor;                       // Inicializar el monitor de inotify y construir la línea base
    monitor.fd = inotify_init();
    monitor.watch_count = 0;
    if (monitor.fd == -1) {
        perror("inotify");
    }
    add_watch_recursive(&monitor, "/media");    // Agregar watch recursivamente a /media
    baseline.count = 0;
    build_baseline("/media");                    // Construir la línea base de archivos en /media

    while (1) {                                 // Bucle principal para manejar eventos de inotify
        fd_set fds;
        struct timeval timeout;
        FD_ZERO(&fds);
        FD_SET(monitor.fd, &fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int sel = select(monitor.fd + 1, &fds, NULL, NULL, &timeout);
        if (sel > 0 && FD_ISSET(monitor.fd, &fds)) {
            handle_events(&monitor);
        }
        sleep(2);                               // Esperar 2 segundos antes de procesar nuevos eventos
    }
    close(monitor.fd);
}