#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>
#include <gtk/gtk.h>
#include <glib.h> 
#include <pthread.h>


// Estructura para pasar datos de alertas entre hilos
typedef struct {
    int port;
    char service_name[50]; 
} PortAlertData;


// Variables globales compartidas entre hilos
GString *g_port_scan_report = NULL;   // Buffer global para reportes
pthread_mutex_t g_port_scan_mutex;    // Mutex global para sincronización
GtkTextBuffer *g_output_text_buffer = NULL;  // Buffer de texto GTK

// Sistema de alertas inteligente (detecta cambios reales)
static int previous_open_ports[1000] = {0};
static int previous_count = 0;
static int current_open_ports[1000] = {0};
static int current_count = 0;

// Lista de puertos conocidos como sospechosos
int suspicious_ports[] = {31337, 6667, 4444, 12345, 54321, 5555, 6969};
int num_suspicious = sizeof(suspicious_ports) / sizeof(int);

// Función para verificar si un puerto YA estaba abierto antes
int was_port_open_before(int port) {
    for (int i = 0; i < previous_count; i++) {
        if (previous_open_ports[i] == port) {
            return 1; // Ya estaba abierto
        }
    }
    return 0; // Es NUEVO
}

// Función para agregar puerto al estado actual
void add_current_port(int port) {
    if (current_count < 1000) {
        current_open_ports[current_count++] = port;
    }
}

// Función para actualizar el estado (
void update_port_states() {
    // Copiar estado actual → estado anterior
    for (int i = 0; i < current_count; i++) {
        previous_open_ports[i] = current_open_ports[i];
    }
    previous_count = current_count;
    
    // Limpiar estado actual para el próximo escaneo
    current_count = 0;
}

// Determina si un puerto es potencialmente peligroso
int is_suspicious(int port, const char *servname) {
    // Verifica si está en la lista de puertos sospechosos
    for (int i = 0; i < num_suspicious; i++) {
        if (port == suspicious_ports[i]) return 1;
    }
     // Puertos altos sin servicio conocido son sospechosos
    if (port > 1024 &&
        (!servname || (
            strcmp(servname, "http") != 0 &&
            strcmp(servname, "mysql") != 0 &&
            strcmp(servname, "postgresql") != 0 &&
            strcmp(servname, "https") != 0 &&
            strcmp(servname, "rdp") != 0 &&
            strcmp(servname, "vnc") != 0 &&
            strcmp(servname, "ftp") != 0 &&
            strcmp(servname, "ssh") != 0 &&
            strcmp(servname, "smtp") != 0 &&
            strcmp(servname, "pop3") != 0 &&
            strcmp(servname, "imap") != 0
        ))
    ) return 1;
    // Sin nombre de servicio es sospechoso
    if (!servname || strlen(servname) == 0) return 1;
    return 0;
}

// Muestra ventana de alerta GTK (ejecutada en hilo principal)
gboolean show_port_alert_callback(gpointer data) {
    PortAlertData *alert_data = (PortAlertData *)data;

    // Crea diálogo modal de advertencia
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK,
        "¡Puerto Comprometido Detectado!\n\nPuerto: %d\nServicio: %s",
        alert_data->port,
        alert_data->service_name
    );

    gtk_window_set_title(GTK_WINDOW(dialog), "Alerta de Seguridad Crítica");
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show(dialog);
    
    return G_SOURCE_REMOVE; // Remueve el callback después de ejecutarlo
}


// Envía una alerta de puerto sospechoso al hilo principal GTK
void send_port_alert(int port, const char *service_name) {
    // NUEVO: Agregar al estado actual
    add_current_port(port);
    
    // NUEVO: Solo alertar si es un puerto que NO estaba abierto antes
    if (was_port_open_before(port)) {
        return; // Ya estaba abierto, no alertar
    }
    
    PortAlertData *new_alert_data = g_new(PortAlertData, 1);
    new_alert_data->port = port;
    
    if (service_name && strlen(service_name) > 0) {
        strncpy(new_alert_data->service_name, service_name, sizeof(new_alert_data->service_name) - 1);
        new_alert_data->service_name[sizeof(new_alert_data->service_name) - 1] = '\0';
    } else {
        strcpy(new_alert_data->service_name, "desconocido");
    }

    g_main_context_invoke_full(
        NULL,
        G_PRIORITY_HIGH,
        show_port_alert_callback,
        new_alert_data,
        (GDestroyNotify)g_free
    );
}


// Función principal de escaneo de puertos
void scan_ports_to_buffer(int start, int end, GString *buffer) {
    for (int port = start; port <= end; port++) {
        // Crea socket TCP no bloqueante
        int sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
            close(sock);
            continue;
        }

        // Configura dirección de destino (localhost)
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
            close(sock);
            continue;
        }

        // Intenta conectar al puerto
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        if (result == 0) { // Conexión exitosa inmediata
            struct sockaddr_in peer_addr;
            socklen_t peer_len = sizeof(peer_addr);
            
            if (getpeername(sock, (struct sockaddr*)&peer_addr, &peer_len) == 0) {
                // Puerto realmente abierto - continuar con detección
                char servname[NI_MAXSERV] = "";
                // Obtiene nombre del servicio del puerto
                if (getnameinfo((struct sockaddr*)&addr, sizeof(addr), NULL, 0, servname, sizeof(servname), 0) == 0) {
                    if (is_suspicious(port, servname)) {
                        g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp (%s) abierto (potencialmente comprometido)\n", port, servname);
                        send_port_alert(port, servname); // Envía alerta GTK
                    } else {
                        g_string_append_printf(buffer, "[OK] Puerto %d/tcp (%s) abierto (esperado)\n", port, servname);
                    }
                } else {
                    // No se pudo resolver el nombre del servicio
                    if (is_suspicious(port, NULL)) {
                        g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp abierto (potencialmente comprometido)\n", port);
                        send_port_alert(port, NULL);
                    } else {
                        g_string_append_printf(buffer, "[OK] Puerto %d/tcp abierto (esperado)\n", port);
                    }
                }
            }
            close(sock);
            continue;
        }else if (errno != EINPROGRESS) {
            close(sock);
            continue;
        }

        // Espera a que la conexión se complete usando select()
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);
        struct timeval tv;
        tv.tv_sec = 0; 
        tv.tv_usec = 200000;

        int sel = select(sock + 1, NULL, &writefds, NULL, &tv);
        if (sel < 0) {
            close(sock);
            continue;
        }
        if (sel > 0 && FD_ISSET(sock, &writefds)) {
            // Verifica si la conexión fue exitosa
            int so_error = 0;
            socklen_t len = sizeof(so_error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
                close(sock);
                continue;
            }
            if (so_error == 0) { // Conexión exitosa después de select
                struct sockaddr_in peer_addr;
                socklen_t peer_len = sizeof(peer_addr);
                
                if (getpeername(sock, (struct sockaddr*)&peer_addr, &peer_len) == 0) {
                    // Puerto realmente abierto - continuar con detección
                    char servname[NI_MAXSERV] = "";
                    if (getnameinfo((struct sockaddr*)&addr, sizeof(addr), NULL, 0, servname, sizeof(servname), 0) == 0) {
                        if (is_suspicious(port, servname)) {
                            g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp (%s) abierto (potencialmente comprometido)\n", port, servname);
                            send_port_alert(port, servname);
                        } else {
                            g_string_append_printf(buffer, "[OK] Puerto %d/tcp (%s) abierto (esperado)\n", port, servname);
                        }
                    } else {
                        if (is_suspicious(port, NULL)) {
                            g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp abierto (potencialmente comprometido)\n", port);
                            send_port_alert(port, NULL);
                        } else {
                            g_string_append_printf(buffer, "[OK] Puerto %d/tcp abierto (esperado)\n", port);
                        }
                    }
                }
            }
        }
        close(sock);
    }
}

// Estructura simplificada para datos del hilo de escaneo
typedef struct {
    int start_port;
    int end_port;
} PortScanThreadData;

// Función que ejecuta el hilo de escaneo continuo
void *scan_ports_thread_func(void *arg) {
    PortScanThreadData *data = (PortScanThreadData *)arg;
    int ciclo = 1;

    while (TRUE) {
        
        pthread_mutex_lock(&g_port_scan_mutex);
        
        g_string_truncate(g_port_scan_report, 0);
        g_string_append_printf(g_port_scan_report, "--- Escaneo Iniciado (%d-%d) ---\n", data->start_port, data->end_port);

        // Ejecuta el escaneo principal
        scan_ports_to_buffer(data->start_port, data->end_port, g_port_scan_report);

        g_string_append_printf(g_port_scan_report, "--- Escaneo Finalizado ---\n\n");
        
        // NUEVO: Actualizar estados después del escaneo
        update_port_states();
        
        pthread_mutex_unlock(&g_port_scan_mutex);
        
        sleep(10);
    }
    g_free(data);
    pthread_exit(NULL);
}