/**
 * ZENTHRA OS — DESKTOP PROTOTYPE & JARVIS AI HUB (v17)
 * Orquestador Multi-Agente, Shell Gráfico y Simulación de Entorno
 * 
 * NOTA DE SEGURIDAD (Auditoría Técnica v17):
 * - Cero uso de innerHTML con entradas dinámicas de usuario o modelo (Protección Anti-XSS).
 * - Modal de aprobación humana cancelable en todo momento.
 * - Todas las métricas y respuestas son simuladas [DEMO].
 * - Cero secretos o API keys embebidas en el cliente.
 */

document.addEventListener('DOMContentLoaded', () => {
    // 1. Reloj del Sistema
    function updateClock() {
        const timeEl = document.getElementById('system-time');
        if (timeEl) {
            const now = new Date();
            timeEl.textContent = now.toLocaleTimeString('es-ES', { hour12: false });
        }
    }
    setInterval(updateClock, 1000);
    updateClock();

    // 2. Gestión de Temas (Claro / Oscuro) y Modo Eco (Bajo Consumo)
    const btnThemeToggle = document.getElementById('btn-theme-toggle');
    const btnEcoToggle = document.getElementById('btn-eco-toggle');
    let isDarkMode = true;
    let isEcoMode = false;

    if (btnThemeToggle) {
        btnThemeToggle.addEventListener('click', () => {
            isDarkMode = !isDarkMode;
            document.documentElement.setAttribute('data-theme', isDarkMode ? 'dark' : 'light');
            btnThemeToggle.textContent = isDarkMode ? '🌓' : '☀️';
        });
    }

    if (btnEcoToggle) {
        btnEcoToggle.addEventListener('click', () => {
            isEcoMode = !isEcoMode;
            document.body.classList.toggle('eco-mode', isEcoMode);
            btnEcoToggle.style.color = isEcoMode ? '#22c55e' : '';
            btnEcoToggle.title = isEcoMode ? 'Modo Eco Activado (Sin Glassmorphism)' : 'Modo Eco / Bajo Consumo';
        });
    }

    // 3. Gestor de Ventanas (Drag, Focus, Minimize, Maximize, Close, Resize Clamp)
    const windowContainer = document.getElementById('window-container') || document.body;
    let highestZ = 10;
    const windows = document.querySelectorAll('.glass-window');

    function bringToFront(win) {
        highestZ++;
        win.style.zIndex = highestZ;
        windows.forEach(w => w.classList.remove('focused'));
        win.classList.add('focused');

        const activeTitleEl = document.getElementById('active-app-title');
        const winTitle = win.querySelector('.window-title span:last-child');
        if (activeTitleEl && winTitle) {
            activeTitleEl.textContent = winTitle.textContent.replace(' [DEMO]', '');
        }
    }

    function clampWindowToContainer(win) {
        const cRect = windowContainer.getBoundingClientRect();
        const wWidth = win.offsetWidth;
        const wHeight = win.offsetHeight;
        
        let left = win.offsetLeft;
        let top = win.offsetTop;

        const maxLeft = Math.max(0, cRect.width - wWidth);
        const maxTop = Math.max(0, cRect.height - wHeight - 70); // Descontar dock

        if (left > maxLeft) win.style.left = `${Math.max(10, maxLeft)}px`;
        if (top > maxTop) win.style.top = `${Math.max(10, maxTop)}px`;
    }

    window.addEventListener('resize', () => {
        windows.forEach(win => {
            if (win.style.display !== 'none') {
                clampWindowToContainer(win);
            }
        });
    });

    windows.forEach(win => {
        const titlebar = win.querySelector('.window-titlebar');
        win.addEventListener('mousedown', () => bringToFront(win));

        // Drag & Drop con delimitación estricta al contenedor
        if (titlebar) {
            let isDragging = false;
            let startX, startY, initialX, initialY;

            titlebar.addEventListener('mousedown', (e) => {
                if (e.target.classList.contains('win-btn')) return;
                isDragging = true;
                bringToFront(win);
                startX = e.clientX;
                startY = e.clientY;
                initialX = win.offsetLeft;
                initialY = win.offsetTop;

                function onMouseMove(e) {
                    if (!isDragging) return;
                    const cRect = windowContainer.getBoundingClientRect();
                    const dx = e.clientX - startX;
                    const dy = e.clientY - startY;

                    const newLeft = Math.max(0, Math.min(cRect.width - win.offsetWidth, initialX + dx));
                    const newTop = Math.max(0, Math.min(cRect.height - 60, initialY + dy));

                    win.style.left = `${newLeft}px`;
                    win.style.top = `${newTop}px`;
                }

                function onMouseUp() {
                    isDragging = false;
                    document.removeEventListener('mousemove', onMouseMove);
                    document.removeEventListener('mouseup', onMouseUp);
                }

                document.addEventListener('mousemove', onMouseMove);
                document.addEventListener('mouseup', onMouseUp);
            });
        }

        // Botones de Ventana (Close, Minimize, Maximize)
        const btnClose = win.querySelector('.win-btn.close');
        const btnMin = win.querySelector('.win-btn.minimize');
        const btnMax = win.querySelector('.win-btn.maximize');

        if (btnClose) {
            btnClose.addEventListener('click', () => {
                win.style.display = 'none';
                win.classList.remove('focused');
                updateDockDot(win.dataset.winId, false);
                const activeTitleEl = document.getElementById('active-app-title');
                if (activeTitleEl) activeTitleEl.textContent = 'Escritorio';
            });
        }

        if (btnMin) {
            btnMin.addEventListener('click', () => {
                win.style.display = 'none';
                win.classList.remove('focused');
                const activeTitleEl = document.getElementById('active-app-title');
                if (activeTitleEl) activeTitleEl.textContent = 'Escritorio';
            });
        }

        if (btnMax) {
            let isMaximized = false;
            let prevRect = {};
            btnMax.addEventListener('click', () => {
                const cRect = windowContainer.getBoundingClientRect();
                if (!isMaximized) {
                    prevRect = {
                        top: win.style.top,
                        left: win.style.left,
                        width: win.style.width,
                        height: win.style.height
                    };
                    win.style.top = '8px';
                    win.style.left = '8px';
                    win.style.width = `${Math.max(300, cRect.width - 16)}px`;
                    win.style.height = `${Math.max(200, cRect.height - 84)}px`;
                    isMaximized = true;
                } else {
                    win.style.top = prevRect.top;
                    win.style.left = prevRect.left;
                    win.style.width = prevRect.width;
                    win.style.height = prevRect.height;
                    isMaximized = false;
                }
            });
        }
    });

    // 4. Dock Flotante Centralizado
    const dockItems = document.querySelectorAll('.dock-item');
    dockItems.forEach(item => {
        item.addEventListener('click', () => {
            const appId = item.dataset.app;
            const targetWin = document.getElementById(`win-${appId}`);
            if (targetWin) {
                if (targetWin.style.display === 'none') {
                    // Restaurar desde cerrado o minimizado
                    targetWin.style.display = 'flex';
                    bringToFront(targetWin);
                    item.classList.add('active');
                } else {
                    // Si ya está visible y en primer plano -> minimizar
                    if (targetWin.classList.contains('focused')) {
                        targetWin.style.display = 'none';
                        targetWin.classList.remove('focused');
                        const activeTitleEl = document.getElementById('active-app-title');
                        if (activeTitleEl) activeTitleEl.textContent = 'Escritorio';
                    } else {
                        // Traer al frente
                        bringToFront(targetWin);
                    }
                }
            }
        });
    });

    function updateDockDot(appId, active) {
        const item = document.querySelector(`.dock-item[data-app="${appId}"]`);
        if (item) {
            item.classList.toggle('active', active);
        }
    }

    // 5. Orquestador Jarvis y los 6 Agentes
    const jarvisForm = document.getElementById('jarvis-form');
    const jarvisInput = document.getElementById('jarvis-prompt');
    const jarvisFeed = document.getElementById('jarvis-feed');
    const planPanel = document.getElementById('jarvis-plan-panel');
    const planTopic = document.getElementById('plan-topic');
    const planStepsList = document.getElementById('plan-steps-list');
    const planProgressFill = document.getElementById('plan-progress-fill');
    const btnCancelPlan = document.getElementById('btn-cancel-plan');

    // Modal de Aprobación Humana Cancelable
    const modal = document.getElementById('approval-modal');
    const btnModalApprove = document.getElementById('btn-modal-approve');
    const btnModalReject = document.getElementById('btn-modal-reject');
    let modalResolver = null;

    function requestUserApproval(opName, target, desc) {
        return new Promise((resolve) => {
            const opEl = document.getElementById('modal-operation');
            const targetEl = document.getElementById('modal-target');
            const descEl = document.getElementById('modal-desc');

            if (opEl) opEl.textContent = opName;
            if (targetEl) targetEl.textContent = target;
            if (descEl) descEl.textContent = desc;

            modal.style.display = 'flex';
            modalResolver = resolve;
        });
    }

    if (btnModalApprove) {
        btnModalApprove.addEventListener('click', () => {
            modal.style.display = 'none';
            if (modalResolver) {
                const res = modalResolver;
                modalResolver = null;
                res(true);
            }
        });
    }
    if (btnModalReject) {
        btnModalReject.addEventListener('click', () => {
            modal.style.display = 'none';
            if (modalResolver) {
                const res = modalResolver;
                modalResolver = null;
                res(false);
            }
        });
    }

    let isPlanExecuting = false;
    let cancelRequested = false;

    // Cancelación limpia e inmediata: desbloquea el modal si estaba abierto
    if (btnCancelPlan) {
        btnCancelPlan.addEventListener('click', () => {
            if (isPlanExecuting) {
                cancelRequested = true;
                addJarvisMessage('system', '⏹ Cancelación solicitada por el usuario. Abortando tareas activas...');

                // Cerrar modal y resolver la promesa pendiente si estaba en espera de confirmación
                if (modalResolver) {
                    modal.style.display = 'none';
                    const res = modalResolver;
                    modalResolver = null;
                    res(false); // Resuelve inmediatamente como rechazado/cancelado
                }
            }
        });
    }

    // Inserción segura de mensajes (Prevención Anti-XSS con textContent)
    function addJarvisMessage(type, text, author = 'Jarvis Core') {
        const msg = document.createElement('div');
        msg.className = `message ${type}-msg`;

        if (type === 'agent' || type === 'system') {
            const authorSpan = document.createElement('span');
            authorSpan.className = 'msg-author';
            authorSpan.textContent = `${author}: `;

            const textSpan = document.createElement('span');
            textSpan.textContent = text; // Asignación segura sin innerHTML

            msg.appendChild(authorSpan);
            msg.appendChild(textSpan);
        } else {
            msg.textContent = text; // Mensaje de usuario seguro
        }

        jarvisFeed.appendChild(msg);
        jarvisFeed.scrollTop = jarvisFeed.scrollHeight;
    }

    function setAgentState(agentId, state, currentTask = 'Idle') {
        const card = document.getElementById(`card-agent-${agentId}`);
        if (!card) return;
        const badge = card.querySelector('.agent-status-badge');
        const taskEl = card.querySelector('.agent-current-task');

        if (state === 'busy') {
            card.classList.add('busy');
            badge.className = 'agent-status-badge status-busy';
            badge.textContent = 'En Simulación';
        } else {
            card.classList.remove('busy');
            badge.className = 'agent-status-badge status-idle';
            badge.textContent = 'Idle';
        }
        if (taskEl) taskEl.textContent = currentTask;
    }

    // Definición de las etapas del Plan Multi-Agente
    const planSteps = [
        {
            id: 'kernel',
            agent: 'kernel',
            name: 'Kernel / OS Agent',
            task: 'Validar directivas VMM, límites de memoria y aislamiento de marcos',
            needsApproval: false
        },
        {
            id: 'security',
            agent: 'security',
            name: 'Cybersecurity Agent',
            task: 'Auditar privilegios ACL e inspeccionar llamada SYS_SET_PROFILE',
            needsApproval: true,
            approvalDesc: 'El Agente de Ciberseguridad requiere auditar y validar los límites de permisos ACL en perfiles concurrentes.'
        },
        {
            id: 'backend',
            agent: 'backend',
            name: 'Backend Agent',
            task: 'Generar microservicio de persistencia en RAMFS y endpoints seguros',
            needsApproval: false
        },
        {
            id: 'data',
            agent: 'data',
            name: 'Data & IoT Agent',
            task: 'Ingestar telemetría de temperatura y calibrar muestreo PIT',
            needsApproval: false
        },
        {
            id: 'ui',
            agent: 'ui',
            name: 'Creative / UI Agent',
            task: 'Optimizar renderizado de widgets y tokens Glassmorphism',
            needsApproval: false
        },
        {
            id: 'automation',
            agent: 'automation',
            name: 'Automation Agent',
            task: 'Programar cron job de validación y empaquetar binarios ELF',
            needsApproval: false
        }
    ];

    async function executeJarvisPlan(userPrompt) {
        if (isPlanExecuting) return;
        isPlanExecuting = true;
        cancelRequested = false;

        addJarvisMessage('user', userPrompt);
        planTopic.textContent = userPrompt.length > 35 ? userPrompt.substring(0, 35) + '...' : userPrompt;
        planPanel.style.display = 'flex';
        
        // Limpieza segura del contenedor de pasos
        while (planStepsList.firstChild) {
            planStepsList.removeChild(planStepsList.firstChild);
        }

        // Generar lista de pasos mediante DOM seguro
        planSteps.forEach((step, idx) => {
            const stepEl = document.createElement('div');
            stepEl.className = 'plan-step-item';
            stepEl.id = `step-item-${step.id}`;

            const labelSpan = document.createElement('span');
            labelSpan.textContent = `${idx + 1}. [${step.name}]: ${step.task}`;

            const statusSpan = document.createElement('span');
            statusSpan.className = 'step-status';
            statusSpan.textContent = 'Pendiente';

            stepEl.appendChild(labelSpan);
            stepEl.appendChild(statusSpan);
            planStepsList.appendChild(stepEl);
        });

        addJarvisMessage('system', `Plan estructurado en ${planSteps.length} fases simuladas con los 6 agentes especializados.`);

        for (let i = 0; i < planSteps.length; i++) {
            if (cancelRequested) {
                addJarvisMessage('system', '⚠️ Plan abortado satisfactoriamente por el operador. No se aplicaron cambios pendientes.');
                break;
            }

            const step = planSteps[i];
            const stepEl = document.getElementById(`step-item-${step.id}`);
            if (stepEl) {
                stepEl.classList.add('active');
                const st = stepEl.querySelector('.step-status');
                if (st) st.textContent = 'Simulando...';
            }

            setAgentState(step.agent, 'busy', step.task);
            addJarvisMessage('agent', `Iniciando: ${step.task}`, step.name);

            // Requerimiento de aprobación humana en pasos críticos
            if (step.needsApproval) {
                const approved = await requestUserApproval('OP_SYSTEM_ADMIN', `/sys/acl/${step.id}`, step.approvalDesc);
                if (cancelRequested) {
                    addJarvisMessage('system', '⚠️ Plan cancelado durante el diálogo de aprobación.');
                    setAgentState(step.agent, 'idle', 'Cancelado');
                    break;
                }
                if (!approved) {
                    addJarvisMessage('system', `❌ Acción denegada por el operador para ${step.name}. Saltando paso crítico de forma segura.`);
                    if (stepEl) {
                        stepEl.classList.remove('active');
                        const st = stepEl.querySelector('.step-status');
                        if (st) st.textContent = 'Denegado';
                    }
                    setAgentState(step.agent, 'idle', 'Acción denegada por usuario');
                    continue;
                }
            }

            // Simulación de latencia de cómputo del modelo / agente
            await new Promise(r => setTimeout(r, 1000));

            if (cancelRequested) {
                setAgentState(step.agent, 'idle', 'Cancelado');
                break;
            }

            // Paso completado de forma simulada
            setAgentState(step.agent, 'idle', 'Completado [SIMULADO]');
            if (stepEl) {
                stepEl.classList.remove('active');
                stepEl.classList.add('completed');
                const st = stepEl.querySelector('.step-status');
                if (st) st.textContent = '✓ Simulado';
            }

            const progressPct = Math.round(((i + 1) / planSteps.length) * 100);
            planProgressFill.style.width = `${progressPct}%`;
        }

        if (!cancelRequested) {
            addJarvisMessage('system', 'ℹ️ Simulación completada [DEMO]: no se modificaron archivos ni se ejecutaron agentes reales. Todos los agentes retornaron a estado Idle.');
        }

        // Restablecer estados de todos los agentes
        planSteps.forEach(s => setAgentState(s.agent, 'idle', 'Idle'));
        isPlanExecuting = false;
    }

    if (jarvisForm) {
        jarvisForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const text = jarvisInput.value.trim();
            if (text) {
                jarvisInput.value = '';
                executeJarvisPlan(text);
            }
        });
    }

    // 6. Mini Gráfico del Monitor de Sistema [SIMULADA]
    const miniChart = document.getElementById('mini-chart');
    if (miniChart) {
        for (let i = 0; i < 24; i++) {
            const bar = document.createElement('div');
            bar.className = 'chart-bar';
            bar.style.height = `${Math.floor(Math.random() * 65) + 15}%`;
            miniChart.appendChild(bar);
        }

        setInterval(() => {
            const bars = miniChart.querySelectorAll('.chart-bar');
            bars.forEach(b => {
                b.style.height = `${Math.floor(Math.random() * 70) + 12}%`;
            });
            const cpuEl = document.getElementById('metric-cpu');
            const ramEl = document.getElementById('metric-ram');
            if (cpuEl) cpuEl.textContent = `${(Math.random() * 6 + 4).toFixed(1)}%`;
            if (ramEl) ramEl.textContent = `${(Math.random() * 2 + 18).toFixed(1)} MiB`;
        }, 3000);
    }

    // 7. Simulación de Shell en Terminal (Segura contra XSS)
    const termInput = document.getElementById('term-input');
    const termOutput = document.getElementById('term-output');

    if (termInput && termOutput) {
        termInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                const cmd = termInput.value.trim();
                termInput.value = '';

                // Inserción segura mediante textContent (Auditoría v17)
                const line = document.createElement('div');
                const promptSpan = document.createElement('span');
                promptSpan.style.color = '#22c55e';
                promptSpan.textContent = 'zenthra@core:~$ ';

                const cmdSpan = document.createElement('span');
                cmdSpan.textContent = cmd; // Literal seguro sin interpretación HTML

                line.appendChild(promptSpan);
                line.appendChild(cmdSpan);
                termOutput.appendChild(line);

                handleTerminalCommand(cmd);
                termOutput.scrollTop = termOutput.scrollHeight;
            }
        });
    }

    function handleTerminalCommand(cmd) {
        const out = document.createElement('div');
        out.style.color = '#94a3b8';
        out.style.whiteSpace = 'pre-wrap';

        switch (cmd.toLowerCase()) {
            case 'help':
                out.textContent = `Comandos disponibles:
  - tasks: Lista procesos del scheduler Round-Robin
  - sysinfo: Información de memoria VMM e identidad 64 MiB
  - agents: Estado de los 6 agentes autónomos
  - clear: Limpiar pantalla
  - version: Información de release`;
                break;
            case 'tasks':
                out.textContent = `PID  NAME             STATE    PROFILE     QUANTUM
 1   kernel_shell     RUNNING  General     5 ticks
 2   jarvis_runtime   READY    SysAdmin    5 ticks
 3   iot_telemetry    SLEEP    Hardware    5 ticks`;
                break;
            case 'sysinfo':
                out.textContent = `Arquitectura: x86 (i686 Protected Mode, 32-bit)
Paginación: VMM 2-niveles (64 MiB Identity Map activo)
Scheduler: Round-Robin Preemptivo UP con PIT IRQ0
Gráficos: VESA LFB 32 bpp con Double Buffering`;
                break;
            case 'agents':
                out.textContent = `Centro de 6 Agentes:
  1. Backend Agent (Idle)
  2. Cybersecurity Agent (Idle)
  3. Kernel/OS Agent (Idle)
  4. Data & IoT Agent (Idle)
  5. Creative/UI Agent (Idle)
  6. Automation Agent (Idle)`;
                break;
            case 'clear':
                while (termOutput.firstChild) {
                    termOutput.removeChild(termOutput.firstChild);
                }
                return;
            case 'version':
                out.textContent = 'Zenthra OS Desktop Prototype DEMO v17 (i686-pc-zenthra)';
                break;
            case '':
                return;
            default:
                out.style.color = '#f43f5e';
                out.textContent = `zenthra: comando no encontrado: '${cmd}'. Escriba 'help'.`;
        }
        termOutput.appendChild(out);
    }
});
