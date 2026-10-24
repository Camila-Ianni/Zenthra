"""
Model Router - Orquesta la selección dinámica de modelos según la tarea
Features:
  • Selección automática por complejidad
  • "Thinking-before-Coding" planning
  • Fallback mechanism
  • Cost tracking ($0 guarantee)
  • Latency optimization
  • Soporte de Ollama local dinámico y providers remotos con manejo estricto de errores
"""

import json
import asyncio
import time
import logging
import os
from typing import Optional, Dict, Any, List, Tuple
from pathlib import Path
from dataclasses import dataclass, field, asdict
from enum import Enum
from abc import ABC, abstractmethod

import httpx

logger = logging.getLogger("ModelRouter")


class TaskComplexity(Enum):
    """Complejidad de la tarea."""
    SIMPLE = 0.2         # "Hello world"
    MODERATE = 0.5       # "Función básica con lógica"
    COMPLEX = 0.8        # "Arquitectura, paterns"
    VERY_COMPLEX = 1.0   # "Sistema distribuido"


@dataclass
class ModelMetadata:
    """Metadata de un modelo."""
    model_id: str
    name: str
    category: str
    cost_tier: str
    max_tokens: int
    supports_thinking: bool
    best_for: List[str]
    latency_ms: int
    enabled: bool


@dataclass
class RoutingDecision:
    """Decisión de enrutamiento tomada por el router."""
    primary_model: str
    secondary_model: str
    tertiary_model: str
    provider: str
    complexity_score: float
    thinking_enabled: bool
    max_context: int
    reasoning: str
    timestamp: float = field(default_factory=time.time)


@dataclass
class ExecutionTrace:
    """Rastro de ejecución para auditoría."""
    task_id: str
    routing_decision: RoutingDecision
    model_used: str
    provider_used: str
    start_time: float
    end_time: Optional[float] = None
    tokens_used: int = 0
    success: bool = False
    error: Optional[str] = None


class ProviderInterface(ABC):
    """Interface común para providers de modelos."""
    
    @abstractmethod
    async def health_check(self) -> bool:
        """Verificar si el provider está disponible."""
        pass
    
    @abstractmethod
    async def get_available_models(self) -> List[str]:
        """Obtener modelos disponibles."""
        pass

    @abstractmethod
    async def generate(self, model: str, prompt: str, system: Optional[str] = None,
                       temperature: float = 0.7, max_tokens: int = 2048) -> Dict[str, Any]:
        """Generar respuesta con el modelo."""
        pass


class OpenRouterProvider(ProviderInterface):
    """Provider para OpenRouter con gestión de clave por entorno."""
    
    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or self._get_api_key()
        self.base_url = "https://openrouter.ai/api/v1"
        self.available = False
        
    def _get_api_key(self) -> Optional[str]:
        return os.getenv("OPENROUTER_API_KEY")
    
    async def health_check(self) -> bool:
        self.api_key = self._get_api_key()
        if not self.api_key:
            logger.info("OPENROUTER_API_KEY no configurada")
            self.available = False
            return False
        try:
            async with httpx.AsyncClient(timeout=4.0) as client:
                res = await client.get(
                    f"{self.base_url}/models",
                    headers={"Authorization": f"Bearer {self.api_key}"}
                )
                self.available = (res.status_code == 200)
                return self.available
        except Exception as e:
            logger.warning(f"OpenRouter health check failed: {e}")
            self.available = False
            return False
    
    async def get_available_models(self) -> List[str]:
        if not self.api_key:
            return []
        try:
            async with httpx.AsyncClient(timeout=4.0) as client:
                res = await client.get(
                    f"{self.base_url}/models",
                    headers={"Authorization": f"Bearer {self.api_key}"}
                )
                if res.status_code == 200:
                    data = res.json()
                    return [m.get("id") for m in data.get("data", [])][:20]
        except Exception:
            pass
        return [
            "deepseek/deepseek-r1:free",
            "deepseek/deepseek-v4:free",
            "qwen/qwen3-coder:free"
        ]

    async def generate(self, model: str, prompt: str, system: Optional[str] = None,
                       temperature: float = 0.7, max_tokens: int = 2048) -> Dict[str, Any]:
        self.api_key = self._get_api_key()
        if not self.api_key:
            return {
                "success": False,
                "error": "OPENROUTER_API_KEY no está configurada en el entorno",
                "content": "",
                "model": model,
                "provider": "openrouter"
            }
        
        messages = []
        if system:
            messages.append({"role": "system", "content": system})
        messages.append({"role": "user", "content": prompt})
        
        start = time.time()
        try:
            async with httpx.AsyncClient(timeout=30.0) as client:
                res = await client.post(
                    f"{self.base_url}/chat/completions",
                    headers={
                        "Authorization": f"Bearer {self.api_key}",
                        "HTTP-Referer": "https://github.com/Camila-Ianni/Zenthra",
                        "X-Title": "Jarvis-MK37"
                    },
                    json={
                        "model": model,
                        "messages": messages,
                        "temperature": temperature,
                        "max_tokens": max_tokens
                    }
                )
                elapsed_ms = int((time.time() - start) * 1000)
                if res.status_code == 200:
                    data = res.json()
                    content = data["choices"][0]["message"]["content"]
                    return {
                        "success": True,
                        "content": content,
                        "model": model,
                        "provider": "openrouter",
                        "latency_ms": elapsed_ms
                    }
                else:
                    return {
                        "success": False,
                        "error": f"HTTP {res.status_code}: {res.text[:200]}",
                        "content": "",
                        "model": model,
                        "provider": "openrouter",
                        "latency_ms": elapsed_ms
                    }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "content": "",
                "model": model,
                "provider": "openrouter"
            }


class OllamaProvider(ProviderInterface):
    """Provider para Ollama (local) con detección y ejecución real."""
    
    def __init__(self, base_url: Optional[str] = None):
        self.base_url = base_url or os.getenv("OLLAMA_BASE_URL", "http://localhost:11434")
        self.available = False
        self.models_cache: List[str] = []
        
    async def health_check(self) -> bool:
        """Verificar si Ollama está corriendo en localhost."""
        try:
            async with httpx.AsyncClient(timeout=3.0) as client:
                res = await client.get(f"{self.base_url}/api/tags")
                if res.status_code == 200:
                    self.available = True
                    data = res.json()
                    self.models_cache = [
                        m.get("name") or m.get("model") for m in data.get("models", [])
                    ]
                    return True
        except Exception as e:
            logger.info(f"Ollama local no disponible en {self.base_url}: {e}")
        self.available = False
        return False
    
    async def get_available_models(self) -> List[str]:
        """Obtener modelos instalados dinámicamente en Ollama."""
        try:
            async with httpx.AsyncClient(timeout=3.0) as client:
                res = await client.get(f"{self.base_url}/api/tags")
                if res.status_code == 200:
                    data = res.json()
                    models = [
                        m.get("name") or m.get("model") for m in data.get("models", [])
                    ]
                    self.models_cache = models
                    return models
        except Exception:
            pass
        return self.models_cache if self.available else []

    async def generate(self, model: str, prompt: str, system: Optional[str] = None,
                       temperature: float = 0.7, max_tokens: int = 2048) -> Dict[str, Any]:
        """Genera respuesta usando el endpoint /api/chat o /api/generate de Ollama."""
        start = time.time()
        try:
            messages = []
            if system:
                messages.append({"role": "system", "content": system})
            messages.append({"role": "user", "content": prompt})

            async with httpx.AsyncClient(timeout=60.0) as client:
                res = await client.post(
                    f"{self.base_url}/api/chat",
                    json={
                        "model": model,
                        "messages": messages,
                        "stream": False,
                        "options": {
                            "temperature": temperature,
                            "num_predict": max_tokens
                        }
                    }
                )
                elapsed_ms = int((time.time() - start) * 1000)
                if res.status_code == 200:
                    data = res.json()
                    reply = data.get("message", {}).get("content", "")
                    return {
                        "success": True,
                        "content": reply,
                        "model": model,
                        "provider": "ollama",
                        "latency_ms": elapsed_ms
                    }
                else:
                    return {
                        "success": False,
                        "error": f"Ollama HTTP {res.status_code}: {res.text[:200]}",
                        "content": "",
                        "model": model,
                        "provider": "ollama",
                        "latency_ms": elapsed_ms
                    }
        except Exception as e:
            return {
                "success": False,
                "error": f"Error conectando a Ollama ({self.base_url}): {str(e)}",
                "content": "",
                "model": model,
                "provider": "ollama"
            }


class ZhipuAIProvider(ProviderInterface):
    """Provider para Zhipu AI (GLM)."""
    
    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or self._get_api_key()
        self.base_url = "https://open.bigmodel.cn/api/paas/v4"
        self.available = False
        
    def _get_api_key(self) -> Optional[str]:
        return os.getenv("ZHIPU_API_KEY")
    
    async def health_check(self) -> bool:
        self.api_key = self._get_api_key()
        if not self.api_key:
            logger.info("ZHIPU_API_KEY no configurada - GLM remoto inactivo")
            self.available = False
            return False
        self.available = True
        return True
    
    async def get_available_models(self) -> List[str]:
        if not self.available:
            return []
        return ["glm-4.5-air", "glm-5-code"]

    async def generate(self, model: str, prompt: str, system: Optional[str] = None,
                       temperature: float = 0.7, max_tokens: int = 2048) -> Dict[str, Any]:
        self.api_key = self._get_api_key()
        if not self.api_key:
            return {
                "success": False,
                "error": "ZHIPU_API_KEY no está configurada en el entorno",
                "content": "",
                "model": model,
                "provider": "zhipu_ai"
            }
        return {
            "success": False,
            "error": "Proveedor Zhipu AI no conectado (requiere ZHIPU_API_KEY activa)",
            "content": "",
            "model": model,
            "provider": "zhipu_ai"
        }


class GeminiProvider(ProviderInterface):
    """Provider para Google Gemini con soporte de variables de entorno."""
    
    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or self._get_api_key()
        self.available = False

    def _get_api_key(self) -> Optional[str]:
        env_key = os.getenv("GOOGLE_API_KEY")
        if env_key and env_key.strip():
            return env_key.strip()
        try:
            from memory.config_manager import get_gemini_key
            return get_gemini_key()
        except Exception:
            return None

    async def health_check(self) -> bool:
        self.api_key = self._get_api_key()
        self.available = bool(self.api_key and len(self.api_key) > 15)
        return self.available

    async def get_available_models(self) -> List[str]:
        return ["gemini-2.5-flash", "models/gemini-2.5-flash"]

    async def generate(self, model: str, prompt: str, system: Optional[str] = None,
                       temperature: float = 0.7, max_tokens: int = 2048) -> Dict[str, Any]:
        self.api_key = self._get_api_key()
        if not self.api_key:
            return {
                "success": False,
                "error": "GOOGLE_API_KEY no está configurada en el entorno",
                "content": "",
                "model": model,
                "provider": "gemini"
            }
        start = time.time()
        try:
            from google import genai
            client = genai.Client(api_key=self.api_key)
            full_prompt = f"{system}\n\n{prompt}" if system else prompt
            response = await asyncio.to_thread(
                client.models.generate_content,
                model=model if "gemini" in model else "gemini-2.5-flash",
                contents=full_prompt
            )
            elapsed_ms = int((time.time() - start) * 1000)
            return {
                "success": True,
                "content": response.text,
                "model": model,
                "provider": "gemini",
                "latency_ms": elapsed_ms
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "content": "",
                "model": model,
                "provider": "gemini"
            }


class ModelRouter:
    """
    Enrutador de Modelos - Selecciona y ejecuta el mejor modelo según la tarea.
    
    Estrategia:
    1. Calcula complejidad de la tarea
    2. Selecciona provider basado en routing rules y candidatos
    3. Verifica disponibilidad con health checks reales
    4. Ejecuta llamada al LLM con fallback transparente
    5. Registra auditoría en global state
    """
    
    def __init__(self, config_path: Optional[Path] = None):
        self.config_path = config_path or Path(__file__).parent.parent / "config" / "model_providers.json"
        self.config = self._load_config()
        self.providers = self._init_providers()
        self.execution_traces: List[ExecutionTrace] = []
        self.model_stats = {}
        
    def _load_config(self) -> Dict[str, Any]:
        try:
            with open(self.config_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            logger.error(f"Failed to load config: {e}")
            return {
                "providers": {},
                "routing_rules": {}
            }
    
    def _init_providers(self) -> Dict[str, ProviderInterface]:
        return {
            "openrouter": OpenRouterProvider(),
            "ollama": OllamaProvider(),
            "zhipu_ai": ZhipuAIProvider(),
            "gemini": GeminiProvider(),
        }
    
    async def calculate_complexity(self, 
                                   task_description: str,
                                   language: str,
                                   code_length: int = 0) -> float:
        complexity = 0.3
        high_complexity_keywords = ["architecture", "design", "optimize", "refactor", "pattern"]
        medium_keywords = ["fix", "debug", "improve"]
        
        task_lower = task_description.lower()
        for keyword in high_complexity_keywords:
            if keyword in task_lower:
                complexity += 0.3
        
        for keyword in medium_keywords:
            if keyword in task_lower:
                complexity += 0.15
        
        lang_multiplier = self.config.get("language_specific", {}).get(
            language.lower(), {}
        ).get("complexity_multiplier", 1.0)
        
        complexity *= lang_multiplier
        
        if code_length > 500:
            complexity += 0.2
        elif code_length > 200:
            complexity += 0.1
        
        return min(complexity, 1.0)
    
    async def route(self,
                   task_type: str,
                   task_description: str,
                   language: str,
                   code_length: int = 0,
                   sensitive: bool = False) -> RoutingDecision:
        complexity = await self.calculate_complexity(task_description, language, code_length)
        routing_rules = self.config.get("routing_rules", {})
        default_rule = {
            "primary": "qwen3.5:latest",
            "secondary": "qwen/qwen3-coder:free",
            "tertiary": "gemini-2.5-flash",
            "fallback": "http://localhost:11434/api"
        }
        
        if sensitive:
            rule = routing_rules.get("sensitive_operations", default_rule)
        else:
            rule = routing_rules.get(task_type, routing_rules.get("fallback_default", default_rule))
        
        provider = "ollama" if sensitive else "openrouter"
        max_ctx = self.config.get("default_settings", {}).get("max_context_tokens", 8000)

        decision = RoutingDecision(
            primary_model=rule.get("primary", default_rule["primary"]),
            secondary_model=rule.get("secondary", default_rule["secondary"]),
            tertiary_model=rule.get("tertiary", default_rule["tertiary"]),
            provider=provider,
            complexity_score=complexity,
            thinking_enabled=complexity > 0.6,
            max_context=max_ctx,
            reasoning=f"Complexity: {complexity:.2f}, Task: {task_type}, Language: {language}"
        )
        
        logger.info(f"Routing decision: {decision.reasoning}")
        return decision
    
    async def _check_provider_health(self) -> Dict[str, bool]:
        health = {}
        for name, provider in self.providers.items():
            try:
                health[name] = await asyncio.wait_for(provider.health_check(), timeout=3.5)
            except Exception as e:
                logger.debug(f"Health check error for {name}: {e}")
                health[name] = False
        return health
    
    async def get_recommended_model(self, routing_decision: RoutingDecision) -> str:
        candidates = [
            routing_decision.primary_model,
            routing_decision.secondary_model,
            routing_decision.tertiary_model,
        ]
        
        for model in candidates:
            if model and await self._is_model_available(model):
                logger.info(f"Using model: {model}")
                return model
        
        # Fallback local o Gemini
        ollama_models = await self.providers["ollama"].get_available_models()
        if ollama_models:
            for pref in ["qwen3.5:latest", "gemma4:latest", "qwen2.5-coder:1.5b-base", "llama3:latest"]:
                if pref in ollama_models:
                    return pref
            return ollama_models[0]

        logger.warning("All preferred models unavailable, falling back to Gemini")
        return "gemini-2.5-flash"
    
    async def _is_model_available(self, model_id: str) -> bool:
        if not model_id:
            return False
            
        # Detección para Ollama local
        if "ollama" in model_id or "localhost" in model_id or ":" in model_id:
            clean_name = model_id.split("/")[-1]
            models = await self.providers["ollama"].get_available_models()
            if any(clean_name in m or m in clean_name for m in models):
                return True

        if "deepseek" in model_id or "qwen" in model_id:
            # Primero chequear si existe equivalente en Ollama local
            models = await self.providers["ollama"].get_available_models()
            clean_id = model_id.lower()
            if any("qwen" in m for m in models) and "qwen" in clean_id:
                return True
            if any("deepseek" in m for m in models) and "deepseek" in clean_id:
                return True
            return await self.providers["openrouter"].health_check()
        elif "glm" in model_id:
            return await self.providers["zhipu_ai"].health_check()
        elif "gemini" in model_id:
            return await self.providers["gemini"].health_check()
        return False
    
    async def execute_model(self, model_id: str, prompt: str, system: Optional[str] = None,
                            temperature: float = 0.7, max_tokens: int = 2048) -> Dict[str, Any]:
        """Ejecuta una inferencia con el modelo seleccionado o mediante fallback."""
        start_time = time.time()
        
        # Determinar provider primario
        provider_name = "ollama"
        if "gemini" in model_id:
            provider_name = "gemini"
        elif "openrouter" in model_id or "/" in model_id:
            provider_name = "openrouter"
        elif "glm" in model_id:
            provider_name = "zhipu_ai"
            
        provider = self.providers.get(provider_name, self.providers["ollama"])
        result = await provider.generate(model_id, prompt, system, temperature, max_tokens)
        
        # Si falló y tenemos Ollama local disponible, fallback transparente a Ollama
        if not result.get("success"):
            logger.warning(f"Provider {provider_name} failed: {result.get('error')}. Checking local Ollama...")
            ollama_models = await self.providers["ollama"].get_available_models()
            if ollama_models:
                target_local = ollama_models[0]
                for m in ["qwen2.5-coder:1.5b-base", "gemma4:latest", "llama3:latest", "qwen3.5:latest"]:
                    if m in ollama_models:
                        target_local = m
                        break
                logger.info(f"Fallback to local Ollama model: {target_local}")
                result = await self.providers["ollama"].generate(target_local, prompt, system, temperature, max_tokens)

        # Si aún falla y Gemini está disponible, fallback a Gemini
        if not result.get("success") and await self.providers["gemini"].health_check():
            logger.warning("Falling back to Gemini...")
            result = await self.providers["gemini"].generate("gemini-2.5-flash", prompt, system, temperature, max_tokens)

        # Registrar trace
        trace = ExecutionTrace(
            task_id=f"exec_{int(time.time() * 1000)}",
            routing_decision=RoutingDecision(
                primary_model=model_id,
                secondary_model="fallback_local",
                tertiary_model="gemini",
                provider=result.get("provider", provider_name),
                complexity_score=0.5,
                thinking_enabled=False,
                max_context=max_tokens,
                reasoning="Automated execution dispatch"
            ),
            model_used=result.get("model", model_id),
            provider_used=result.get("provider", provider_name),
            start_time=start_time,
            end_time=time.time(),
            tokens_used=len(result.get("content", "").split()),
            success=result.get("success", False),
            error=result.get("error")
        )
        self.record_execution(trace)
        return result

    def record_execution(self, trace: ExecutionTrace) -> None:
        self.execution_traces.append(trace)
        model = trace.model_used
        if model not in self.model_stats:
            self.model_stats[model] = {
                "count": 0,
                "total_latency": 0,
                "success_count": 0,
                "error_count": 0
            }
        
        stats = self.model_stats[model]
        stats["count"] += 1
        
        if trace.end_time:
            latency = trace.end_time - trace.start_time
            stats["total_latency"] += latency
        
        if trace.success:
            stats["success_count"] += 1
        else:
            stats["error_count"] += 1
        
        logger.info(f"Execution recorded: {model} - Success: {trace.success}")
    
    def get_stats(self) -> Dict[str, Any]:
        """Obtener estadísticas de uso de modelos de forma segura."""
        return {
            "total_executions": len(self.execution_traces),
            "model_stats": self.model_stats
        }


# Singleton instance
_router_instance: Optional[ModelRouter] = None


def get_model_router() -> ModelRouter:
    """Obtener instancia singleton del router."""
    global _router_instance
    if _router_instance is None:
        _router_instance = ModelRouter()
    return _router_instance


def is_ollama_available(timeout: float = 0.8) -> bool:
    """Verificación rápida de disponibilidad local de Ollama en localhost:11434."""
    try:
        import urllib.request
        base_url = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434")
        req = urllib.request.Request(f"{base_url}/api/tags", headers={"User-Agent": "pytest-check"})
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status == 200
    except Exception:
        return False
