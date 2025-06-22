# 🛡️ Border Guard Antivirus

Border Guard Antivirus es una solución inspirada en un universo de fantasía cibernética: un reino digital donde cada proceso es una criatura y cada puerto, un punto estratégico. Los virus y amenazas informáticas son plagas y ejércitos invasores que buscan corromper las tierras y saquear los recursos. **Border Guard Antivirus** es tu muralla y guardia real, un sistema diseñado para vigilar y proteger tu reino (máquina virtual).  

✨ Este proyecto combina la magia de los Sistemas Operativos con la lógica de la programación concurrente, empleando hilos de ejecución para dividir las tareas de patrullaje y defensa del sistema UNIX en tiempo real. Mientras un hilo vigila los procesos del sistema, otro escanea en busca de amenazas, todo bajo una fortaleza gráfica construida con GTK. Su arquitectura modular y sincronizada convierte este antivirus en una muralla digital activa y eficiente, capaz de detectar y responder ante intrusos antes de que invadan el reino.

🎯 **La misión:** Construir una fortaleza impenetrable que detecte y neutralice cualquier amenaza antes de que cause estragos.

---

## ⚔️ Funcionalidades

### 1. 🧭 Patrullas Fronterizas: Detección y Escaneo de Dispositivos Conectados

- **Descripción:** Los dispositivos USB son como mercaderes o viajeros que cruzan las fronteras del reino. Algunos pueden traer enfermedades o espías ocultos.
- **Funcionalidad:**
  - Monitorear periódicamente el directorio de montaje (las puertas de entrada del reino) para detectar nuevos dispositivos conectados.
  - Escanear recursivamente el sistema de archivos del dispositivo para identificar archivos agregados, eliminados o modificados, buscando "traiciones" o "plagas".
  - Emitir alertas en tiempo real si se detectan cambios sospechosos en el sistema de archivos, indicando

> ⚠️ Cambios sospechosos: Modificaciones no autorizadas en atributos, contenido o estructura de archivos en dispositivos USB montados, que puedan indicar malware, intrusiones o comportamientos anómalos.

---

### 2. 👑 Guardias del Tesoro Real: Monitoreo de Recursos de Procesos e Hilos

- **Descripción:** Como los tributos y recursos del reino, la CPU y la memoria deben ser vigiladas cuidadosamente. Los procesos descontrolados son como ladrones que roban el oro del tesoro real.
- **Funcionalidad:**
  - Leer información de procesos desde `/proc` para obtener datos como PID, nombre, uso de CPU y memoria.
  - Comparar el uso de recursos entre iteraciones para detectar picos inusuales, como "ladrones hambrientos".
  - Emitir alertas si algún proceso supera umbrales predefinidos para uso de CPU o memoria, señalando posibles traidores o espías.

---

### 3. 🏰 Defensores de las Murallas: Escaneo de Puertos Locales

- **Descripción:** Los puertos abiertos son como puertas en las murallas del castillo. Si no están bien custodiadas, pueden ser explotadas por ejércitos enemigos.
- **Funcionalidad:**
  - Escanear un rango de puertos (por ejemplo, 1-1024) utilizando sockets TCP para identificar puertos abiertos.
  - Asociar los puertos abiertos con servicios comunes (ej: 22 para SSH, 80 para HTTP), buscando anomalías o "puertas secretas".
  - Generar un informe con los puertos abiertos y sus servicios asociados, destacando puertos "potencialmente comprometidos".

---

### 4. 👑 El Gran Salón del Trono: Interfaz Gráfica

- **Descripción:** Una interfaz gráfica para visualizar los resultados del sistema, como el Gran Salón del Trono donde el monarca toma decisiones estratégicas.
- **Funcionalidad:**
  - Mostrar en tiempo real los dispositivos conectados, procesos monitoreados y puertos abiertos, como un mapa interactivo del reino.
  - Permitir al usuario interactuar con el sistema para iniciar escaneos o generar reportes, como un consejo de guerra.
  - **Herramientas recomendadas:** Bibliotecas como GTK+.

---

## 🧩 Función principal (`main`)

La función `main` es el **corazón** del antivirus, donde la magia de la concurrencia y la interfaz gráfica se unen para proteger tu reino digital. Aquí te explico su funcionamiento paso a paso:

1. 🧵 **Creación de Hilos Guardianes:**  
   - Se lanzan dos hilos:  
     - 👁️‍🗨️ Uno vigila constantemente los procesos del sistema (`hilo_leer_procesos`).  
     - 🕵️‍♂️ Otro realiza escaneos en busca de amenazas (`detection_scan_thread`).  
   - Ambos comparten recursos protegidos por un **escudo mágico** (mutex) 🛡️ para evitar conflictos.

2. 🖥️ **Inicio de la Fortaleza Visual:**  
   - Se inicializa la aplicación GTK 🏰, que permite mostrar la interfaz gráfica al usuario.
   - Se conecta el evento de activación de la ventana a la función que construye la interfaz real del antivirus.

3. 🔄 **Defensa en Tiempo Real:**  
   - El programa permanece activo mientras la ventana principal esté abierta, patrullando y defendiendo el reino.

4. 🧹 **Cierre Ordenado del Reino:**  
   - Al cerrar la ventana, se cancelan y sincronizan ambos hilos 🧵❌.
   - Se liberan todas las estructuras de datos y memoria utilizada 🗑️.
   - Se destruyen los mecanismos de sincronización 🔒.
   - Así, se garantiza un cierre limpio y sin pérdidas de recursos.

---

### 🛠️ Código de la función principal

```c
// 🧩 Función principal (main)
int main(int argc, char **argv) {
    // Declaración de hilos para lectura de procesos y escaneo de amenazas
    pthread_t hilo_leer_procesos;
    pthread_t detection_scan_thread;

    // Inicialización del mutex para controlar el acceso concurrente a recursos compartidos
    pthread_mutex_init(&mutex_procesos, NULL);

    // Creación del hilo que monitorea los procesos del sistema
    pthread_create(&hilo_leer_procesos, NULL, read_proc_thread, &mutex_procesos);

    // Creación del hilo que realiza el escaneo de amenazas
    pthread_create(&detection_scan_thread, NULL, detection_scan_thread, NULL);

    // Inicialización de la aplicación GTK
    gtk_init(&argc, &argv);

    // Conexión del evento de activación de la ventana a la función de construcción de la interfaz
    g_signal_connect(gtk_window_new(), "activate", G_CALLBACK(activate), NULL);

    // Ejecución del bucle principal de la aplicación GTK
    gtk_main();

    // Cancelación y sincronización de los hilos antes de salir
    pthread_cancel(hilo_leer_procesos);
    pthread_cancel(detection_scan_thread);
    pthread_join(hilo_leer_procesos, NULL);
    pthread_join(detection_scan_thread, NULL);

    // Destrucción del mutex y limpieza de recursos
    pthread_mutex_destroy(&mutex_procesos);

    return 0;
}
```

## 🕵️‍♂️ Centinela de Archivos: `handle_events`

La función `handle_events` es el **vigilante incansable** del reino digital. Se encarga de procesar todos los eventos del sistema de archivos generados por inotify 🛎️ en Linux. Analiza cada suceso y actúa según el tipo de evento: creación, eliminación, modificación, cambios de permisos, tamaño o propiedad de archivos y carpetas. Cada alerta es registrada y notificada al usuario 👑, permitiendo detectar cambios sospechosos y proteger la integridad del reino.

### ¿Qué hace el centinela?  
- 🗂️ **Detecta nuevas carpetas y archivos**  
- 🗑️ **Advierte sobre eliminaciones**  
- 📈 **Alerta sobre crecimientos inusuales**  
- 🌀 **Detecta cambios de extensión**  
- 🔒 **Vigila cambios de permisos**  
- 🧬 **Detecta archivos replicados**  
- ⏰ **Controla cambios de timestamp**  
- 👑 **Supervisa cambios de ownership**  
- 📝 **Registra modificaciones**  
- 📋 **Actualiza la base de datos de archivos vigilados**

Cada evento relevante genera una alerta visual y se registra en el sistema de mensajes, permitiendo al usuario actuar rápidamente ante cualquier amenaza o anomalía.

---

### 🛠️ Fragmento de código del centinela

```c
// Procesa los eventos recibidos por inotify para tomar acciones sobre archivos o directorios
void handle_events(monitor_info* monitor) {
    char buffer[EVENT_BUF_LEN];

    // Lee los eventos del descriptor de inotify
    int length = read(monitor->fd, buffer, EVENT_BUF_LEN);
    if (length < 0) return;

    int i = 0;

    // Itera por cada evento contenido en el buffer
    while (i < length) {
        struct inotify_event* event = (struct inotify_event*)&buffer[i];
        const char* dir_path = get_path_by_wd(monitor, event->wd);

        // Verifica que haya un nombre asociado al evento
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

            // Si se creó un directorio nuevo
            if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Nueva carpeta creada: %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
                add_watch_recursive(monitor, full_path); // Agrega watch a subdirectorios
            }

            // Si se eliminó un directorio
            if ((event->mask & IN_DELETE) && (event->mask & IN_ISDIR)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Carpeta eliminada: %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
                remove_dir_from_baseline(full_path);
            }

            // Si un archivo ha crecido excesivamente
            if (base && is_file && (fi.size - base->size) > (100 * 1024 * 1024)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Crecimiento inusual de tamaño en %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
            }

            // Detecta si el archivo es nuevo pero con misma base y distinta extensión
            if (!base && is_file) {
                file_info* base_ext = find_same_base_diff_ext(full_path);
                if (base_ext) {
                    const char* ext_old = strrchr(base_ext->path, '.');
                    const char* ext_new = strrchr(full_path, '.');
                    if (ext_old && ext_new && strcmp(ext_old, ext_new) != 0) {
                        snprintf(alerta, sizeof(alerta), "ALERTA: Cambio de extensión en %s", full_path);
                        show_warning(alerta);
                        add_message(mensajes, &mensaje_count, alerta);
                        strncpy(base_ext->path, full_path, MAX_PATH_LENGTH);
                    }
                }
            }

            // Cambios en permisos del archivo
            if (base && is_file) {
                if ((base->mode & 0777) != (fi.mode & 0777)) {
                    snprintf(alerta, sizeof(alerta), "ALERTA: Cambio de permisos en %s", full_path);
                    show_warning(alerta);
                    add_message(mensajes, &mensaje_count, alerta);
                    base->mode = fi.mode;
                }
            }

            // Detecta archivo copiado de otro existente
            if ((event->mask & IN_CREATE) && is_file && is_copy_of_existing(&fi)) {
                snprintf(alerta, sizeof(alerta), "ALERTA: Archivo replicado detectado: %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
                if (!base && baseline.count < MAX_FILES) {
                    baseline.files[baseline.count++] = fi;
                    base = &baseline.files[baseline.count - 1];
                }
            }

            // Cambios en atributos como timestamp o ownership
            if (base && is_file) {
                if (base->mtime != fi.mtime) {
                    snprintf(alerta, sizeof(alerta), "ALERTA: Timestamp modificado en %s", full_path);
                    show_warning(alerta);
                    add_message(mensajes, &mensaje_count, alerta);
                }
                if (base->uid != fi.uid || base->gid != fi.gid) {
                    snprintf(alerta, sizeof(alerta), "ALERTA: Cambio de ownership en %s", full_path);
                    show_warning(alerta);
                    add_message(mensajes, &mensaje_count, alerta);
                }
            }

            // Manejo de eventos básicos de creación, eliminación o modificación
            if ((event->mask & IN_CREATE) && is_file && !base && baseline.count < MAX_FILES) {
                baseline.files[baseline.count++] = fi;
                snprintf(alerta, sizeof(alerta), "Nuevo archivo: %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
            }

            if ((event->mask & IN_DELETE) && base) {
                snprintf(alerta, sizeof(alerta), "Archivo eliminado: %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
                remove_from_baseline(base - baseline.files);
            }

            if ((event->mask & IN_MODIFY) && is_file && base && memcmp(base->sha256, fi.sha256, SHA256_DIGEST_LENGTH) != 0) {
                snprintf(alerta, sizeof(alerta), "Archivo modificado: %s", full_path);
                show_warning(alerta);
                add_message(mensajes, &mensaje_count, alerta);
                *base = fi;
            }
        }

        // Avanza al siguiente evento en el buffer
        i += EVENT_SIZE + event->len;
    }
}
```

## 🏰 Centinela de Puertos: `scan_ports_to_buffer`

El siguiente método actúa como un **guardia fronterizo digital** 👮‍♂️, patrullando los puertos de tu reino (localhost) para detectar puertas abiertas que puedan ser usadas por aliados... o enemigos. Realiza un escaneo dentro de un rango definido, usando sockets no bloqueantes para identificar rápidamente qué puertos están abiertos en la dirección 127.0.0.1. Cada puerto abierto es evaluado:  
- Si es un puerto esperado, se marca como seguro ✅  
- Si es sospechoso, se reporta como potencialmente comprometido 🚨  

Toda la información se agrega en tiempo real a un búfer de texto, permitiendo generar un reporte de seguridad del sistema. Así, este centinela digital vigila la frontera de comunicación del reino, detectando cualquier comportamiento anómalo o intruso.

---

### 🛠️ Fragmento de código del centinela de puertos

```c
// Escanea puertos dentro del rango [start, end] e imprime información en el buffer
void scan_ports_to_buffer(int start, int end, GString *buffer) {
    // Itera sobre el rango de puertos especificado
    for (int port = start; port <= end; port++) {
        // Crea un socket TCP para intentar la conexión al puerto
        int sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue; // Si falla la creación, pasa al siguiente puerto

        // Configura el socket como no bloqueante para evitar bloqueos en la conexión
        if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
            close(sock);
            continue;
        }

        // Prepara la estructura de dirección para la conexión a localhost y el puerto actual
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // Convierte la IP 127.0.0.1 a formato binario y la asigna a la estructura de dirección
        if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
            close(sock);
            continue;
        }

        // Intenta conectarse al puerto actual
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        if (result == 0) {
            // Si la conexión es inmediata, el puerto está abierto
            char servname[32] = "";
            // Intenta obtener el nombre del servicio asociado al puerto
            if (getnameinfo((struct sockaddr*)&addr, sizeof(addr), NULL, 0, servname, sizeof(servname), 0) == 0) {
                // Clasifica el puerto como esperado o potencialmente comprometido
                if (is_suspicious(port, servname)) {
                    g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp (%s) abierto (potencialmente comprometido)\n", port, servname);
                } else {
                    g_string_append_printf(buffer, "[OK] Puerto %d/tcp (%s) abierto (esperado)\n", port, servname);
                }
            } else {
                // Si no se puede identificar el servicio, igual se evalúa el riesgo
                if (is_suspicious(port, NULL)) {
                    g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp abierto (potencialmente comprometido)\n", port);
                } else {
                    g_string_append_printf(buffer, "[OK] Puerto %d/tcp abierto (esperado)\n", port);
                }
            }
            close(sock);
            continue;
        } else if (errno != EINPROGRESS) {
            // Si la conexión no está en progreso, el puerto no está abierto
            close(sock);
            continue;
        }

        // Usa select para esperar hasta 1 segundo si el puerto está disponible para escritura
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int sel = select(sock + 1, NULL, &writefds, NULL, &tv);
        if (sel < 0) {
            close(sock);
            continue;
        }

        if (sel > 0 && FD_ISSET(sock, &writefds)) {
            // Verifica si la conexión fue exitosa usando getsockopt
            int so_error = 0;
            socklen_t len = sizeof(so_error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
                close(sock);
                continue;
            }

            if (so_error == 0) {
                // El puerto está abierto, se repite la lógica de clasificación
                char servname[32] = "";
                if (getnameinfo((struct sockaddr*)&addr, sizeof(addr), NULL, 0, servname, sizeof(servname), 0) == 0) {
                    if (is_suspicious(port, servname)) {
                        g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp (%s) abierto (potencialmente comprometido)\n", port, servname);
                    } else {
                        g_string_append_printf(buffer, "[OK] Puerto %d/tcp (%s) abierto (esperado)\n", port, servname);
                    }
                } else {
                    if (is_suspicious(port, NULL)) {
                        g_string_append_printf(buffer, "[ALERTA] Puerto %d/tcp abierto (potencialmente comprometido)\n", port);
                    } else {
                        g_string_append_printf(buffer, "[OK] Puerto %d/tcp abierto (esperado)\n", port);
                    }
                }
            }
        }

        // Cierra el socket antes de pasar al siguiente puerto
        close(sock);
    }
}
```
---

## 🏁 Conclusiones

Como conclusión, **Border Guard Antivirus** es una auténtica muralla digital 🏰, construida con una arquitectura defensiva moderna y eficaz. Gracias al uso estratégico de **hilos de ejecución** 🧵, el sistema logra protección en tiempo real en sistemas UNIX, permitiendo ejecutar tareas críticas como la vigilancia continua de procesos y el escaneo independiente de amenazas, todo sin bloquear la interfaz gráfica ni sacrificar rendimiento. Esta paralelización garantiza una vigilancia constante del entorno, con respuestas ágiles ante cualquier comportamiento sospechoso. Los hilos actúan como centinelas del reino digital 👁️‍🗨️, distribuyendo la carga de defensa y asegurando que ninguna amenaza cruce las fronteras sin ser detectada.

**Border Guard es tu legado como señor feudal de este reino digital.**  
Construye una fortaleza impenetrable, entrena a tus guardias y defiende tus tierras de plagas y ejércitos invasores.  
Tu sabiduría y habilidad decidirán el destino de tu reino.

¡Defiende tu reino digital con Border Guard Antivirus! 🛡️👑  
