/**
 * ZENTHRA OS — DESKTOP PROTOTYPE & JARVIS AI HUB
 * Orquestador Multi-Agente, Shell Gráfico y Simulación de Entorno
 * 
 * NOTA DE SEGURIDAD:
 * Todas las métricas y respuestas externas son simuladas [DEMO].
 * Cero secretos o API keys embebidas en el cliente.
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

    // 3. Gestor de Ventanas (Drag, Focus, Minimize, Maximize, Close)
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

    windows.forEach(win => {
        const titlebar = win.querySelector('.window-titlebar');
        win.addEventListener('mousedown', () => bringToFront(win));

        // Drag & Drop
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
                    const dx = e.clientX - startX;
                    const dy = e.clientY - startY;
                    win.style.left = `${Math.max(10, initialX + dx)}px`;
                    win.style.top = `${Math.max(45, initialY + dy)}px`;
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
                updateDockDot(win.dataset.winId, false);
            });
        }

        if (btnMin) {
            btnMin.addEventListener('click', () => {
                win.style.display = 'none';
            });
        }

        if (btnMax) {
            let isMaximized = false;
            let prevRect = {};
            btnMax.addEventListener('click', () => {
                if (!isMaximized) {
                    prevRect = {
                        top: win.style.top,
                        left: win.style.left,
                        width: win.style.width,
                        height: win.style.height
                    };
                    win.style.top = '44px';
                    win.style.left = '10px';
                    win.style.width = 'calc(100vw - 20px)';
                    win.style.height = 'calc(100vh - 120px)';
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

    // 4. Dock Flotante
    const dockItems = document.querySelectorAll('.dock-item');
    dockItems.forEach(item => {
        item.addEventListener('click', () => {
            const appId = item.dataset.app;
            const targetWin = document.getElementById(`win-${appId}`);
            if (targetWin) {
                if (targetWin.style.display === 'none') {
                    targetWin.style.display = 'flex';
                    bringToFront(targetWin);
                    item.classList.add('active');
                } else {
                    if (targetWin.classList.contains('focused')) {
                        targetWin.style.display = 'none';
                    } else {
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

    // Modal de Aprobación
    const modal = document.getElementById('approval-modal');
    const btnModalApprove = document.getElementById('btn-modal-approve');
    const btnModalReject = document.getElementById('btn-modal-reject');
    let modalResolver = null;

    function requestUserApproval(opName, target, desc) {
        return new Promise((resolve) => {
            document.getElementById('modal-operation').textContent = opName;
            document.getElementById('modal-target').textContent = target;
            document.getElementById('modal-desc').innerHTML = desc;
            modal.style.display = 'flex';
            modalResolver = resolve;
        });
    }

    if (btnModalApprove) {
        btnModalApprove.addEventListener('click', () => {
            modal.style.display = 'none';
            if (modalResolver) modalResolver(true);
        });
    }
    if (btnModalReject) {
        btnModalReject.addEventListener('click', () => {
            modal.style.display = 'none';
            if (modalResolver) modalResolver(false);
        });
    }

    let isPlanExecuting = false;
    let cancelRequested = false;

    if (btnCancelPlan) {
        btnCancelPlan.addEventListener('click', () => {
            if (isPlanExecuting) {
                cancelRequested = true;
                addJarvisMessage('system', '⏹ Cancelación solicitada por el usuario. Abortando tareas activas...');
            }
        });
    }

    function addJarvisMessage(type, text, author = 'Jarvis') {
        const msg = document.createElement('div');
        msg.className = `message ${type}-msg`;
        if (type === 'agent') {
            msg.innerHTML = `<span class="msg-author">${author}:</span><span>${text}</span>`;
        } else if (type === 'user') {
            msg.textContent = text;
        } else {
            msg.innerHTML = `<span class="msg-author">Jarvis Core:</span><span>${text}</span>`;
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
            badge.textContent = 'En Ejecución';
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
            approvalDesc: 'El <strong>Agente de Ciberseguridad</strong> requiere auditar y validar los límites de permisos ACL en perfiles concurrentes.'
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
        planStepsList.innerHTML = '';

        // Generar lista de pasos en el panel
        planSteps.forEach((step, idx) => {
            const stepEl = document.createElement('div');
            stepEl.className = 'plan-step-item';
            stepEl.id = `step-item-${step.id}`;
            stepEl.innerHTML = `
                <span><strong>${idx + 1}. [${step.name}]</strong>: ${step.task}</span>
                <span class="step-status">Pendiente</span>
            `;
            planStepsList.appendChild(stepEl);
        });

        addJarvisMessage('system', `Plan estructurado en ${planSteps.length} fases coordinadas con los 6 agentes especializados.`);

        for (let i = 0; i < planSteps.length; i++) {
            if (cancelRequested) {
                addJarvisMessage('system', '⚠️ Plan abortado satisfactoriamente por el usuario. No se aplicaron cambios pendientes.');
                break;
            }

            const step = planSteps[i];
            const stepEl = document.getElementById(`step-item-${step.id}`);
            if (stepEl) {
                stepEl.classList.add('active');
                stepEl.querySelector('.step-status').textContent = 'Ejecutando...';
            }

            setAgentState(step.agent, 'busy', step.task);
            addJarvisMessage('agent', `Iniciando: ${step.task}`, step.name);

            // Requerimiento de aprobación humana en pasos críticos
            if (step.needsApproval) {
                const approved = await requestUserApproval('OP_SYSTEM_ADMIN', `/sys/acl/${step.id}`, step.approvalDesc);
                if (!approved) {
                    addJarvisMessage('system', `❌ Acción denegada por el operador para ${step.name}. Saltando paso crítico de forma segura.`);
                    if (stepEl) {
                        stepEl.classList.remove('active');
                        stepEl.querySelector('.step-status').textContent = 'Denegado';
                    }
                    setAgentState(step.agent, 'idle', 'Acción denegada por usuario');
                    continue;
                }
            }

            // Simulación de latencia de cómputo del modelo / agente
            await new Promise(r => setTimeout(r, 1200));

            if (cancelRequested) {
                setAgentState(step.agent, 'idle');
                break;
            }

            // Paso completado
            setAgentState(step.agent, 'idle', 'Completado');
            if (stepEl) {
                stepEl.classList.remove('active');
                stepEl.classList.add('completed');
                stepEl.querySelector('.step-status').textContent = '✓ Hecho';
            }

            const progressPct = Math.round(((i + 1) / planSteps.length) * 100);
            planProgressFill.style.width = `${progressPct}%`;
        }

        if (!cancelRequested) {
            addJarvisMessage('system', '🎉 ¡Plan completado con éxito! Todos los agentes retornaron a estado Idle.');
        }

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

    // 6. Mini Gráfico del Monitor de Sistema [DEMO]
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

    // 7. Simulación de Shell en Terminal
    const termInput = document.getElementById('term-input');
    const termOutput = document.getElementById('term-output');

    if (termInput && termOutput) {
        termInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                const cmd = termInput.value.trim();
                termInput.value = '';

                const line = document.createElement('div');
                line.innerHTML = `<span style="color: #22c55e;">zenthra@core:~$</span> ${cmd}`;
                termOutput.appendChild(line);

                handleTerminalCommand(cmd);
                termOutput.scrollTop = termOutput.scrollHeight;
            }
        });
    }

    function handleTerminalCommand(cmd) {
        const out = document.createElement('div');
        out.style.color = '#94a3b8';

        switch (cmd.toLowerCase()) {
            case 'help':
                out.innerHTML = `Comandos disponibles:<br>
  - tasks: Lista procesos del scheduler Round-Robin<br>
  - sysinfo: Información de memoria VMM e identidad 64 MiB<br>
  - agents: Estado de los 6 agentes autónomos<br>
  - clear: Limpiar pantalla<br>
  - version: Información de release`;
                break;
            case 'tasks':
                out.innerHTML = `PID  NAME             STATE    PROFILE     QUANTUM<br>
 1   kernel_shell     RUNNING  General     5 ticks<br>
 2   jarvis_runtime   READY    SysAdmin    5 ticks<br>
 3   iot_telemetry    SLEEP    Hardware    5 ticks`;
                break;
            case 'sysinfo':
                out.innerHTML = `Arquitectura: x86 (i686 Protected Mode, 32-bit)<br>
Paginación: VMM 2-niveles (64 MiB Identity Map activo)<br>
Scheduler: Round-Robin Preemptivo UP con PIT IRQ0<br>
Gráficos: VESA LFB 32 bpp con Double Buffering`;
                break;
            case 'agents':
                out.innerHTML = `Centro de 6 Agentes:<br>
  1. Backend Agent (Idle)<br>
  2. Cybersecurity Agent (Idle)<br>
  3. Kernel/OS Agent (Idle)<br>
  4. Data & IoT Agent (Idle)<br>
  5. Creative/UI Agent (Idle)<br>
  6. Automation Agent (Idle)`;
                break;
            case 'clear':
                termOutput.innerHTML = '';
                return;
            case 'version':
                out.textContent = 'Zenthra OS v16.0 — Multi-Agent Desktop Prototype [DEMO]';
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
