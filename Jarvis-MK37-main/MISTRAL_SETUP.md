# JARVIS - Integración Mistral 7B

JARVIS ahora puede usar el modelo Mistral 7B localmente para codear sin depender de APIs externas.

## Requisitos

- macOS (o Linux/Windows con Homebrew)
- ~4GB de espacio libre
- RAM: 8GB+ recomendado

## Instalación

### 1. Instalar Ollama

```bash
brew install ollama
```

### 2. Descargar modelo Mistral 7B

```bash
cd /Users/cami/Desktop/Jarvis-MK37-main
python3 setup_model.py
```

O manualmente:
```bash
ollama pull mistral
```

### 3. Iniciar servidor Ollama

```bash
ollama serve
```

Dejar corriendo en una terminal. El servidor escucha en `http://localhost:11434`

## Uso

### Desde JARVIS

Cuando pidas a JARVIS que codee:
```
"Crea una función que valide emails en Python"
```

JARVIS usará automáticamente Mistral 7B si Ollama está corriendo.

### Test manual

```bash
python3 actions/mistral_helper.py
```

## Características

- ✅ Generación de código en Python, JavaScript, Go, Rust, etc.
- ✅ Código limpio y bien comentado
- ✅ Sin conexión a internet necesaria (offline)
- ✅ Rápido: 50-100 tokens/seg en CPU
- ✅ Gratuito: modelo open-source

## Rendimiento

- **CPU only**: ~8-15 seg por generación
- **Con GPU**: ~2-3 seg por generación

## Troubleshooting

**"Ollama no está corriendo"**
```bash
ollama serve  # En otra terminal
```

**"Timeout en generación"**
- El modelo está procesando, espera
- Reduce el tamaño del prompt si es muy largo

**"Memory error"**
- Cierra otras aplicaciones
- Reduce max_tokens en mistral_helper.py

## Alternativas

Si prefieres otro modelo:

```bash
ollama pull llama2       # Llama 2 7B (más preciso)
ollama pull neural-chat  # Neural Chat (optimizado)
ollama pull dolphin-mixtral  # Dolphin Mixtral (8x7B)
```

Luego actualiza `MODEL_NAME = "mistral"` en `mistral_helper.py`

---

**JARVIS ahora codea localmente y sin costos de API** 🚀
