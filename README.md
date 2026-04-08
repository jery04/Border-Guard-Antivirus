# Border Guard Antivirus 🛡️💻🔐

Border Guard Antivirus is a cyber-defense project inspired by a digital fantasy kingdom 🏰, where processes are creatures 🧬, ports are gates 🚪, and malware is an invading army 🦠⚔️. The system works as an active defense wall 🧱 that monitors, detects, and reports suspicious behavior in real time on Linux and UNIX environments 🐧.

This project blends operating systems concepts 🧠, concurrent programming with threads 🧵, low-level networking 📡, file integrity analysis 🔍, and a GTK visual control panel 🖥️.

Mission: build a resilient, proactive, and educational security layer that detects anomalies before they become incidents 🚨.

## Main Features ⚙️🛰️

1. Border Patrol for files and mounted devices 📁🧭
- Monitors mounted paths (focused on /media) in near real time 👀.
- Builds and maintains a baseline of known files 🧱.
- Detects suspicious events such as:
  - file creation and deletion 📄➕📄➖
  - abnormal file growth 📈
  - extension changes 🧪
  - permission changes 🔐
  - ownership changes 👤
  - replicated files 🧬
- Sends real-time alerts to the interface 🔔.

2. Process and resource sentinels 🧠📊
- Reads process telemetry from /proc 📂.
- Tracks CPU and memory behavior over time 🧮.
- Detects sustained overuse and anomalous peaks 🚨.
- Generates alerts and can terminate processes that exceed thresholds for too long ⛔.

3. Local port defense scanner 📡🛜
- Scans local TCP ports on 127.0.0.1 🔌.
- Detects open ports and resolves service names when available 🧾.
- Classifies ports as expected or potentially compromised ⚠️.
- Produces a live scan report for the GUI 🗒️.

4. Control hall interface with GTK 🖥️🧰
- Presents security data in dedicated windows 🪟.
- Includes views for:
  - active process table 📋
  - process alerts 🚨
  - port scan alerts and report 📡
  - file integrity alerts 📁
- Keeps monitoring modules active while the interface is running 🔄.

## Runtime Architecture 🧩🧵

The application uses concurrent modules coordinated by mutexes 🔒:

- Process thread 🧠:
  - reads /proc continuously and updates the internal process state.

- File monitoring thread 📁:
  - runs recursive inotify watches and compares events against baseline data.

- Port scanning thread 📡:
  - executes periodic scans and updates the shared report buffer.

- GTK main thread 🖥️:
  - handles dialogs, windows, rendering, and user interaction.

During shutdown, all threads are canceled and joined safely, shared resources are released, and mutexes are destroyed cleanly 🧹.

## Project Structure From Root 🗂️🏗️

- .git/ 🧬
  - Repository history, objects, refs, and version control metadata.

- .gitignore 🚫
  - Rules for files that should not be committed.

- .vscode/ 🛠️
  - Local workspace/editor settings and tooling metadata.

- Interfaz.c 🖥️
  - Main application entry point and GTK initialization.
  - Thread creation and lifecycle coordination.
  - GUI actions for process, file, and port alert visualization.

- Prosesos.h 🧠
  - Process monitoring logic based on Linux /proc.
  - CPU and memory threshold evaluation.
  - Alert generation and optional process termination.

- List.h 🔗
  - Doubly linked list used as live process state storage.
  - Node creation, insertion, update, deletion, and cleanup operations.

- Alert.h 🚨
  - Linked list for process alert events.
  - Stores process name, alert type, and timestamp.

- guardian_frontera.h 📁🔍
  - inotify-based file and directory monitoring.
  - Baseline management, SHA-256 hashing, watch mapping, and event analysis.

- port_scanner.h 📡
  - Non-blocking localhost TCP scanner.
  - Suspicious port detection policy and GTK alert dispatch.
  - Shared report buffer and synchronization primitives.

- README.md 📘
  - Technical and functional project documentation.

## Technology Stack Explained 🧰🧪

This project is a combination of systems programming, security analysis, and GUI engineering. The stack is intentionally low-level to provide control, performance, and educational depth.

1. Programming language and systems style 💻
- C language is used for direct memory control, deterministic performance, and close interaction with operating system APIs.
- The codebase follows a modular design where each header encapsulates a security domain.

2. Operating system integration (Linux/UNIX) 🐧
- /proc provides process metrics (CPU usage, memory footprint, process identity).
- inotify provides real-time file system notifications for creation, deletion, modification, and metadata changes.
- POSIX calls like stat, select, read, and kill support low-level monitoring and response behavior.

3. Concurrency and synchronization 🧵🔒
- pthread is used to run process monitoring, file monitoring, and port scanning in parallel.
- Mutexes protect shared structures such as process state and scan reports, preventing race conditions.
- This model keeps the GUI responsive while security tasks run continuously.

4. Networking and socket scanning 📡🌐
- TCP sockets are created in non-blocking mode for efficient scanning.
- select and getsockopt are used to validate connection state without blocking the entire application.
- Localhost scanning is used to detect exposed services and suspicious communication surfaces.

5. Cryptography and integrity verification 🔐🧬
- OpenSSL EVP and SHA-256 are used to fingerprint file contents.
- Hash comparisons make it possible to detect real content changes, file replication, and integrity anomalies.

6. Graphical layer and event loop 🖥️🎛️
- GTK provides windows, dialogs, tables, text views, and interaction components.
- GLib utilities provide dynamic strings, memory helpers, and main-context callbacks for safe cross-thread UI notifications.
- Security findings are surfaced visually through alert dialogs and report windows.

7. Internal data models 🧱📚
- A custom doubly linked list stores evolving process telemetry.
- A dedicated alert list keeps chronological security notifications.
- File baseline structures maintain file metadata snapshots for differential analysis.

8. Security detection strategy 🛡️⚠️
- Rule-based heuristics classify suspicious ports.
- Resource threshold policies identify abusive processes.
- Baseline-delta logic detects suspicious filesystem drift.

## End-to-End Technical Flow 🔄🛰️

1. Startup initializes GTK, mutexes, and monitoring threads ⚙️.
2. Process monitor reads /proc and updates process telemetry continuously 📊.
3. File monitor receives inotify events and compares against baseline hashes and metadata 📁.
4. Port scanner runs periodic non-blocking scans and updates shared reports 📡.
5. GUI thread presents alerts and live information to the user in dedicated windows 🖥️.
6. Shutdown sequence cancels threads, joins them safely, and frees all shared resources 🧹.

## How to Run 🚀

This project is designed for Linux/UNIX environments because it depends on `/proc` and `inotify`.

1. Install build dependencies (Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-3-dev libssl-dev libx11-dev
```

2. Go to the project root and compile:

```bash
gcc Interfaz.c -o border_guard $(pkg-config --cflags --libs gtk+-3.0) -lssl -lcrypto -pthread
```

3. Run the application:

```bash
./border_guard
```

4. Optional (recommended for full monitoring permissions):

```bash
sudo ./border_guard
```

Quick notes:
- The GUI opens with buttons for process, port, and file alerts.
- File monitoring is oriented to mounted paths (especially `/media`).
- If `pkg-config` cannot find GTK, verify `libgtk-3-dev` is installed.

## Final Notes ✅🏰

Border Guard Antivirus is both a functional defensive platform and a practical systems-security lab 🧪. Its strength comes from combining process telemetry, filesystem integrity analysis, and network surface monitoring into one coordinated real-time application 🛡️📡📁🧠.
