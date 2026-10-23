# Zenthra OS — Prototipo de Escritorio & Centro Multi-Agente Jarvis [DEMO]

Este directorio contiene el **prototipo visual interactivo e independiente** del entorno de escritorio de Zenthra OS, diseñado con estética **Glassmorphism auténtica**, dock flotante, terminal gráfica integrada y el **Centro de 6 Agentes Especializados coordinados por Jarvis**.

---

## 🎯 Separación Arquitectónica: Kernel vs. Userland

En estricto cumplimiento de los principios de diseño de sistemas operativos y las recomendaciones de la **Auditoría Técnica v16**:
1. **El Kernel Bare-Metal permanece puro y autocontenido**:
   - El núcleo x86 i686 (ubicado en `kernel/`) mantiene su estricto aislamiento sin dependencias externas, librerías de IA ni credenciales.
   - Las llamadas al sistema (`SYS_READ`, `SYS_WRITE`, `SYS_SET_PROFILE`, etc.) operan mediante interrupciones de software `int 0x80` con buffers privados en la pila de kernel y validaciones estrictas *fail-closed*.
2. **El Entorno Gráfico y Jarvis operan en Userland**:
   - Este prototipo simula la capa superior de usuario (Userland / Ring 3) a la que eventualmente compilará el frontend del sistema operativo.
   - Se ejecuta independientemente mediante navegadores o runtimes ligeros locales (ej. WebKit embedded o Electron/Tauri en host).

---

## 🛡️ Arquitectura de Seguridad: Gateway Híbrido

Para garantizar que el sistema opere sin filtrar datos ni comprometer claves de acceso:
- **Cero Secretos Hardcodeados**: Ninguna API key o credencial está incrustada en el código fuente.
- **Modelo de Gateway Híbrido**:
  ```
  +-------------------------------------------------------------+
  |              Zenthra Desktop & Jarvis UI                    |
  +-------------------------------------------------------------+
                               |
                               v
  +-------------------------------------------------------------+
  |              Local Gateway / IPC Broker                     |
  |     - Autenticación por variables de entorno (.env)          |
  |     - Auditoría de permisos ACL                             |
  |     - Solicitud de aprobación interactiva para ops críticas |
  +-------------------------------------------------------------+
             /                                     \
            v                                       v
  +------------------------+             +----------------------+
  | Local LLM (Ollama)     |             | Remote API (Kimi)    |
  | (Qwen / DeepSeek R1)   |             | (Vía HTTPS Proxy)    |
  +------------------------+             +----------------------+
  ```
- **Rótulos [DEMO]**: Toda métrica de rendimiento, telemetría de temperatura IoT y respuestas simuladas están explícitamente etiquetadas con `[DEMO]` para no confundir datos simulados con mediciones directas de bare-metal.

---

## 👥 Los 6 Agentes Especializados

| Agente | Rol | Permisos ACL Asignados |
| :--- | :--- | :--- |
| **1. Backend Agent** | Microservicios, endpoints y persistencia RAMFS | `OP_FS_READ`, `OP_FS_WRITE` |
| **2. Cybersecurity Agent** | Auditorías de credenciales, límites Ring 3/0 | `OP_SYSTEM_ADMIN`, ACL Inspector |
| **3. Kernel / OS Agent** | Paginación VMM/PMM, scheduler y drivers | `OP_SYSTEM_ADMIN`, Ring 0 Bridge |
| **4. Data & IoT Agent** | Telemetría de sensores, muestreo PIT | `OP_ARDUINO_READ`, Telemetry Log |
| **5. Creative / UI Agent** | Tokens Glassmorphism, widgets y accesibilidad | `UI Shell`, `CSS Tokens` |
| **6. Automation Agent** | Cron jobs, pipelines CI/CD y tareas en background | `OP_FS_READ`, `OP_FS_WRITE`, Scheduler |

---

## 🚀 Cómo Ejecutar el Prototipo

No se requieren herramientas de compilación cruzada ni emuladores:

### Opción 1: Abrir directamente en el navegador
Haga doble clic en el archivo `index.html` o use la terminal:
```bash
# En macOS:
open desktop_prototype/index.html

# En Linux:
xdg-open desktop_prototype/index.html
```

### Opción 2: Servidor local ligero (Python)
```bash
cd desktop_prototype
python3 -m http.server 8080
# Abra http://localhost:8080 en su navegador
```

---

## ✨ Características Interactivas Disponibles

1. **Orquestación Multi-Agente con Jarvis**:
   - Escriba una solicitud en la barra de Jarvis (ej. *"Auditar seguridad ACL y preparar despliegue"*).
   - Observe la descomposición en tiempo real en un plan de 6 fases asignadas a los agentes.
   - **Aprobación Crítica**: En pasos de alto riesgo, el sistema detiene la ejecución y despliega un modal exigiendo confirmación humana explícita.
   - **Abortar Plan**: En cualquier momento puede presionar *"⏹ Abortar Plan"* para cancelar la ejecución de forma limpia.
2. **Modo Eco / Bajo Consumo**:
   - Al presionar el botón `🌱` en la barra superior, se desactivan los filtros pesados `backdrop-filter: blur()`, optimizando el consumo para netbooks o CPUs/GPUs limitadas.
3. **Modo Claro / Oscuro**:
   - Alternancia fluida con el botón `🌓` en la barra superior.
4. **Ventanas Arrastrables y Flotantes**:
   - Arrastre las ventanas desde la barra de título, minimice, maximice o cierre con los controles superiores.
   - Dock flotante interactivo para enfocar o restaurar aplicaciones.
5. **Terminal Interactiva**:
   - Soporta comandos: `help`, `tasks`, `sysinfo`, `agents`, `version`, `clear`.
