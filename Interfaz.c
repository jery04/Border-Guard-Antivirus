#include <gtk/gtk.h>
#include "Prosesos.h"
#include "port_scanner.h" 
#include "guardian_frontera.h" // Asegúrate de que Mensaje y mensajes[] sean visibles

extern Mensaje mensajes[MAX_MENSAJES];
extern int mensaje_count;

typedef struct {
    GtkListStore      *store;
    GtkScrolledWindow *scrolled_window;
} UpdateData;

//Iniciar variables globales
pthread_mutex_t mutex_procesos;

// 🧩 Función que se ejecuta cada segundo para actualizar el modelo
gboolean actualizar_procesos(gpointer data) {
    UpdateData *upd = (UpdateData *) data;
    GtkListStore *store = upd->store;
    GtkScrolledWindow *scrolled = upd->scrolled_window;

    GtkTreeIter iter;
    gboolean valid;

    pthread_mutex_lock(&mutex_procesos);

    // Crear una copia de los IDs actuales del modelo
    GHashTable *vistos = g_hash_table_new(g_int_hash, g_int_equal);

    // 1. Recorrer la lista de procesos actual (Root) e intentar actualizar o insertar
    List *proc = Root;
    while (proc != NULL) {
        gboolean encontrado = FALSE;
        valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);

        while (valid) {
            gint pid_en_modelo;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 1, &pid_en_modelo, -1); // columna 1 = ID

            if (pid_en_modelo == proc->id) {
                // Proceso ya está en el modelo → actualizar valores
                gtk_list_store_set(store, &iter,
                                   0, proc->name,
                                   2, proc->memory,
                                   3, proc->cpu_por,
                                   -1);
                encontrado = TRUE;
                break;
            }

            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }

        if (!encontrado) {
            // Proceso no está en el modelo → insertar nuevo
            gtk_list_store_insert_with_values(store, NULL, -1,
                                              0, proc->name,
                                              1, proc->id,
                                              2, proc->memory,
                                              3, proc->cpu_por,
                                              -1);
        }

        // Marcar PID como visto
        gint *pid_key = g_new(gint, 1);
        *pid_key = proc->id;
        g_hash_table_insert(vistos, pid_key, pid_key);

        proc = proc->next;
    }

    // 2. Eliminar procesos del modelo que ya no existen en Root
    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        GtkTreeIter current_iter = iter;
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);

        gint pid_en_modelo;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &current_iter, 1, &pid_en_modelo, -1);

        if (!g_hash_table_contains(vistos, &pid_en_modelo)) {
            gtk_list_store_remove(store, &current_iter);
        }
    }

    g_hash_table_destroy(vistos);
    pthread_mutex_unlock(&mutex_procesos);

    return TRUE;
}

// 🧩 Función para mostrar ventana con tabla de procesos
void mostrar_procesos_window(GtkWidget *widget, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *treeview;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Lista de Procesos");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);

    /* 1) Creamos el GtkScrolledWindow y lo guardamos */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    /* 2) Creamos el ListStore y el TreeView como antes */
    store = gtk_list_store_new(4,
                               G_TYPE_STRING,
                               G_TYPE_INT,
                               G_TYPE_LONG,
                               G_TYPE_FLOAT);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    /* 3) Configuramos las columnas (idéntico a tu código original) */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Nombre", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("PID", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Memoria", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("% CPU", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    gtk_container_add(GTK_CONTAINER(scrolled_window), treeview);

    /* 4) Preparamos la estructura con store + scrolled_window */
    UpdateData *upd = g_malloc(sizeof(UpdateData));
    upd->store = store;
    upd->scrolled_window = GTK_SCROLLED_WINDOW(scrolled_window);

    /* 5) Ahora, pasamos &upd (puntero) en lugar de solo store */
    g_timeout_add(1000, actualizar_procesos, upd);

    gtk_widget_show_all(window);

    /* IMPORTANTE: al cerrar la ventana, liberar store, treeview, y la estructura */
    /* (esto puedes hacerlo conectando la señal "destroy" a un callback que
       haga: g_free(upd); g_object_unref(store); etc.) */
}

// 🧩 Función para mostrar ventana con las alertas
void mostrar_alertas_procesos(GtkWidget *widget, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    // Crear ventana
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Alertas de Procesos");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    // Crear contenedor con scroll
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    // Crear GtkTextView y su buffer
    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);

    // Insertar oraciones en el buffer
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

    ListAlert *actual = node_Alert;
    while (actual != NULL) {
        char linea[512];
        snprintf(linea, sizeof(linea),
                 "El proceso \"%s\" a las \"%s\" superó el umbral de %s.\n",
                 actual->nombre,
                 actual->fecha_hora,
                 actual->tipo ? "CPU" : "memoria");

        gtk_text_buffer_insert(buffer, &iter, linea, -1);
        actual = actual->next;
    }

    // Agregar TextView al contenedor con scroll
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // Mostrar todo
    gtk_widget_show_all(window);
}

// Muestra ventana con el reporte de escaneo de puertos en tiempo real
void escanear_puertos_window(GtkWidget *widget, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;

    // Verifica que el reporte global esté inicializado
    if (g_port_scan_report == NULL) {
        g_print("Error: El reporte global de escaneo de puertos no ha sido inicializado.\n");
        return;
    }

    // Crear ventana principal para el reporte
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Reporte de Escaneo de Puertos");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Conectar señal de destrucción de ventana
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), window);

    // Crear contenedor con scroll para texto largo
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 6);
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    // Crear área de texto no editable
    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // Acceso seguro al reporte global usando mutex
    pthread_mutex_lock(&g_port_scan_mutex);

    // Obtener contenido actual del reporte de escaneo
    const char *report_content = g_port_scan_report->str;

    // Mostrar el reporte en el área de texto
    gtk_text_buffer_set_text(buffer, report_content, -1);

    // Liberar acceso al reporte global
    pthread_mutex_unlock(&g_port_scan_mutex);

    // Mostrar ventana completa
    gtk_widget_show_all(window);
}

// Muestra la lista de alertas de archivos (guardian_frontera)
void show_alert_list(GtkWidget *widget, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Lista de Alertas de Archivos");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);

    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

    for (int i = 0; i < mensaje_count; ++i) {
        char linea[512];
        snprintf(linea, sizeof(linea),
                 "[%02d/%02d/%04d %02d:%02d:%02d] %s\n",
                 mensajes[i].tiempo.tm_mday,
                 mensajes[i].tiempo.tm_mon + 1,
                 mensajes[i].tiempo.tm_year + 1900,
                 mensajes[i].tiempo.tm_hour,
                 mensajes[i].tiempo.tm_min,
                 mensajes[i].tiempo.tm_sec,
                 mensajes[i].mensaje);
        gtk_text_buffer_insert(buffer, &iter, linea, -1);
    }

    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_widget_show_all(window);
}

// 🧩 Función principal de activación de la app
static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *button[4];

    // Crear ventana principal
    window = gtk_application_window_new(app);

    //Se le asigna la ventana principal
    main_window = GTK_WINDOW(window);

    gtk_window_set_title(GTK_WINDOW(window), "Matcom Guard");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 450);

    // Crear grid
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_container_add(GTK_CONTAINER(window), grid);

    // Crear etiqueta centrada
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<span size='20000'><b>Opciones del antivirus</b></span>");
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    // Crear botones 1 a 6 (en 3 filas x 2 columnas)
    for (int i = 0; i < 4; i++) {
        char button_label[50];
        if (i == 0) {
            snprintf(button_label, sizeof(button_label), "Mostrar Procesos");
        } else if (i == 1) {
            snprintf(button_label, sizeof(button_label), "Alertas de Procesos");
        } else if (i == 2) {
            snprintf(button_label, sizeof(button_label), "Alertas de Puertos");
        } else if (i == 3) {
            snprintf(button_label, sizeof(button_label), "Alertas de Archivos");
        }

        button[i] = gtk_button_new_with_label(button_label);
        // gtk_widget_set_size_request(button[i], 120, 40);
        gtk_grid_attach(GTK_GRID(grid), button[i], 0, i + 1, 1, 1);

        // Conectar botón 3 (índice 2) a la función mostrar_procesos_window
        if (i == 0) {
            g_signal_connect(button[i], "clicked", G_CALLBACK(mostrar_procesos_window), NULL);
        }
        else if (i == 1) {
            g_signal_connect(button[i], "clicked", G_CALLBACK(mostrar_alertas_procesos), NULL);
        }else if (i == 2) {
            g_signal_connect(button[i], "clicked", G_CALLBACK(escanear_puertos_window), NULL);
        } else if (i == 3){
            g_signal_connect(button[i], "clicked", G_CALLBACK(show_alert_list), NULL);
        }

    }

    // Botón 7 centrado
    // button[6] = gtk_button_new_with_label("Botón 7");
    // gtk_widget_set_size_request(button[6], 120, 40);
    // gtk_widget_set_halign(button[6], GTK_ALIGN_CENTER);
    // gtk_grid_attach(GTK_GRID(grid), button[6], 0, 4, 2, 1);

    gtk_widget_show_all(window);
}

// 🧩 Función principal (main)
int main(int argc, char **argv) {

    //Definir hilo
    pthread_t hilo_leer_procesos;
    pthread_t detection_scan_thread;
    pthread_t port_scanner_thread;   

    pthread_mutex_init(&mutex_procesos, NULL);
    pthread_mutex_init(&g_port_scan_mutex, NULL); 

    g_port_scan_report = g_string_new("");

    PortScanThreadData *port_scan_data = g_new(PortScanThreadData, 1);
    port_scan_data->start_port = 1;
    port_scan_data->end_port = 32767;
    pthread_create(&port_scanner_thread, NULL, scan_ports_thread_func, port_scan_data);


    pthread_create(&hilo_leer_procesos, NULL, read_proc_thread, &mutex_procesos);
    pthread_create(&detection_scan_thread, NULL, detection_scan_thread_func, NULL);

    GtkApplication *app;
    int status;

    app = gtk_application_new("com.matcom.guard", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    pthread_cancel(hilo_leer_procesos);
    pthread_join(hilo_leer_procesos, NULL);

    pthread_cancel(detection_scan_thread);
    pthread_join(detection_scan_thread, NULL);

    pthread_cancel(port_scanner_thread);
    pthread_join(port_scanner_thread, NULL);
   if (g_port_scan_report != NULL) {
    g_string_free(g_port_scan_report, TRUE);
    g_port_scan_report = NULL;  
   }
    FreeList();

    pthread_mutex_destroy(&mutex_procesos);
    pthread_mutex_destroy(&g_port_scan_mutex);

    free(Root);
    return status;
}
