"""
Local Intent Classifier - The Reflex Protocol

Classifies user intents into FAST_TRACK or COMPLEX_REASONING using a local SLM.
Enables sub-300ms response times for simple commands.

Technology: Ollama (local LLM inference)
Models: Llama-3-8B, Phi-3-mini (low-latency optimized)

Features:
  - Intent classification via local SLM
  - Confidence scoring
  - Automatic fallback to cloud if Ollama unavailable
  - Caching for repeated intents
"""

import asyncio
import json
import logging
import re
from typing import Optional, Dict, Any, List
from dataclasses import dataclass
from enum import Enum
from datetime import datetime, timedelta

try:
    import httpx
    HTTPX_AVAILABLE = True
except ImportError:
    HTTPX_AVAILABLE = False


logger = logging.getLogger(__name__)


class IntentType(Enum):
    """Classification of user intents"""
    FAST_TRACK = "fast_track"           # Hardware, memory, simple queries
    COMPLEX_REASONING = "complex"       # Coding, analysis, deep reasoning
    VISION_CONTEXT = "vision_context"   # Debugging, screenshot analysis
    UNKNOWN = "unknown"


class IntentCategory(Enum):
    """Sub-categories for routing"""
    HARDWARE_CONTROL = "hardware"       # Brightness, volume, system
    MEMORY_QUERY = "memory"             # Graph queries, search
    QUICK_TASK = "quick_task"           # Simple todos, notes
    CODE_GENERATION = "code_gen"        # Write/fix code
    CODE_ANALYSIS = "code_analysis"     # Understand code
    DEBUGGING = "debugging"             # Fix errors
    RESEARCH = "research"               # Learn/investigate
    BUSINESS = "business"               # Analysis/planning
    MUSIC_MEDIA = "media"               # Play/control media


@dataclass
class IntentClassification:
    """Result of intent classification"""
    intent_type: IntentType
    category: IntentCategory
    confidence: float  # 0.0 to 1.0
    reasoning: str
    suggested_tool: Optional[str]
    requires_vision: bool
    response_time_ms: float
    processed_at: str


class LocalIntentClassifier:
    """
    Classifies user intents using local SLM (Ollama).
    Falls back to COMPLEX_REASONING if Ollama unavailable.
    """
    
    def __init__(
        self,
        ollama_url: str = "http://localhost:11434",
        model: str = "phi3:3.8b-instruct-q4_K_M",
        timeout_ms: int = 300,
    ):
        self.ollama_url = ollama_url
        self.model = model
        self.timeout_seconds = timeout_ms / 1000.0
        self.ollama_available = False
        self._cache: Dict[str, IntentClassification] = {}
        self._cache_ttl = timedelta(hours=1)
        self._last_cache_clean = datetime.now()
    
    async def initialize(self) -> bool:
        """Check if Ollama is available"""
        try:
            if not HTTPX_AVAILABLE:
                logger.warning("httpx not installed. Ollama support disabled.")
                self.ollama_available = False
                return False
            
            async with httpx.AsyncClient(timeout=2.0) as client:
                response = await client.get(f"{self.ollama_url}/api/tags")
                self.ollama_available = response.status_code == 200
                
                if self.ollama_available:
                    logger.info(f"✅ Ollama available at {self.ollama_url}")
                else:
                    logger.warning(f"⚠️ Ollama unavailable at {self.ollama_url}")
                
                return self.ollama_available
        except Exception as e:
            logger.warning(f"⚠️ Ollama connection failed: {e}")
            self.ollama_available = False
            return False
    
    async def classify(self, user_input: str) -> IntentClassification:
        """
        Classify user input into intent type and category.
        
        Args:
            user_input: The user's text input
        
        Returns:
            IntentClassification with type, category, and confidence
        """
        start_time = datetime.now()
        
        # Check cache
        if user_input in self._cache:
            cached = self._cache[user_input]
            if datetime.fromisoformat(cached.processed_at) + self._cache_ttl > datetime.now():
                logger.debug(f"🎯 Cache hit for: {user_input[:50]}")
                return cached
        
        # Clean old cache entries if needed
        if datetime.now() - self._last_cache_clean > timedelta(minutes=5):
            self._clean_cache()
        
        # Classify via local or heuristic
        if self.ollama_available:
            classification = await self._classify_via_ollama(user_input)
        else:
            classification = self._classify_via_heuristic(user_input)
        
        # Calculate response time
        elapsed_ms = (datetime.now() - start_time).total_seconds() * 1000
        classification.response_time_ms = elapsed_ms
        
        # Cache result
        self._cache[user_input] = classification
        
        return classification
    
    async def _classify_via_ollama(self, user_input: str) -> IntentClassification:
        """Classify using Ollama local model"""
        try:
            prompt = self._build_classification_prompt(user_input)
            
            async with httpx.AsyncClient(timeout=self.timeout_seconds) as client:
                response = await client.post(
                    f"{self.ollama_url}/api/generate",
                    json={
                        "model": self.model,
                        "prompt": prompt,
                        "stream": False,
                        "temperature": 0.3,
                    }
                )
                
                if response.status_code != 200:
                    logger.warning(f"Ollama request failed: {response.status_code}")
                    return self._classify_via_heuristic(user_input)
                
                result = response.json()
                response_text = result.get("response", "")
                
                # Parse response
                return self._parse_ollama_response(response_text, user_input)
        
        except asyncio.TimeoutError:
            logger.warning("Ollama classification timeout, using heuristic")
            return self._classify_via_heuristic(user_input)
        except Exception as e:
            logger.warning(f"Ollama classification error: {e}, using heuristic")
            return self._classify_via_heuristic(user_input)
    
    def _classify_via_heuristic(self, user_input: str) -> IntentClassification:
        """
        Fast heuristic-based classification (fallback).
        Uses keyword matching and pattern recognition.
        """
        user_lower = user_input.lower()
        
        # Hardware control keywords (English + Spanish)
        if any(kw in user_lower for kw in ["brightness", "brillo", "volumen", "volume", "sonido", "mute", "sleep", "dim", "aumenta", "disminuye"]):
            return IntentClassification(
                intent_type=IntentType.FAST_TRACK,
                category=IntentCategory.HARDWARE_CONTROL,
                confidence=0.95,
                reasoning="Hardware control command detected",
                suggested_tool="system_control",
                requires_vision=False,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Memory/graph queries
        if any(kw in user_lower for kw in ["search", "find", "what projects", "what skills", "remember", "show me"]):
            return IntentClassification(
                intent_type=IntentType.FAST_TRACK,
                category=IntentCategory.MEMORY_QUERY,
                confidence=0.85,
                reasoning="Memory query detected",
                suggested_tool="graphrag",
                requires_vision=False,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Music/media control
        if any(kw in user_lower for kw in ["play", "pause", "stop music", "spotify", "next", "previous"]):
            return IntentClassification(
                intent_type=IntentType.FAST_TRACK,
                category=IntentCategory.MUSIC_MEDIA,
                confidence=0.9,
                reasoning="Media control detected",
                suggested_tool="media_control",
                requires_vision=False,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Debugging/vision context
        if any(kw in user_lower for kw in ["error", "bug", "debug", "fix this", "what's wrong", "problema"]):
            return IntentClassification(
                intent_type=IntentType.VISION_CONTEXT,
                category=IntentCategory.DEBUGGING,
                confidence=0.8,
                reasoning="Debugging intent detected",
                suggested_tool="vision_service",
                requires_vision=True,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Coding/generation
        if any(kw in user_lower for kw in ["write", "create", "implement", "code", "function", "class", "module"]):
            return IntentClassification(
                intent_type=IntentType.COMPLEX_REASONING,
                category=IntentCategory.CODE_GENERATION,
                confidence=0.85,
                reasoning="Code generation intent detected",
                suggested_tool="gemini_live",
                requires_vision=False,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Code analysis
        if any(kw in user_lower for kw in ["explain", "analyze", "understand", "how does", "what does"]):
            return IntentClassification(
                intent_type=IntentType.COMPLEX_REASONING,
                category=IntentCategory.CODE_ANALYSIS,
                confidence=0.8,
                reasoning="Code analysis intent detected",
                suggested_tool="gemini_live",
                requires_vision=False,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Research/learning
        if any(kw in user_lower for kw in ["teach", "learn", "explain", "research", "investigation"]):
            return IntentClassification(
                intent_type=IntentType.COMPLEX_REASONING,
                category=IntentCategory.RESEARCH,
                confidence=0.75,
                reasoning="Research/learning intent detected",
                suggested_tool="gemini_live",
                requires_vision=False,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        # Default: complex reasoning (safer fallback)
        return IntentClassification(
            intent_type=IntentType.COMPLEX_REASONING,
            category=IntentCategory.RESEARCH,
            confidence=0.5,
            reasoning="Unknown intent, defaulting to complex reasoning",
            suggested_tool="gemini_live",
            requires_vision=False,
            response_time_ms=0,
            processed_at=datetime.now().isoformat(),
        )
    
    def _build_classification_prompt(self, user_input: str) -> str:
        """Build classification prompt for Ollama"""
        return f"""Classify the following user input into one of these categories:
- FAST_TRACK: System control (brightness, volume), memory queries, reminders, simple tasks
- COMPLEX: Code generation/analysis, deep reasoning, research, business analysis
- VISION: Debugging, screenshot analysis (requires context from screen)

User input: "{user_input}"

Respond with a JSON object:
{{
  "type": "fast_track" | "complex" | "vision",
  "category": "hardware" | "memory" | "media" | "code_gen" | "debugging" | "research",
  "confidence": 0.0-1.0,
  "reasoning": "brief explanation"
}}

Only respond with the JSON object, no other text."""
    
    def _parse_ollama_response(self, response_text: str, user_input: str) -> IntentClassification:
        """Parse Ollama JSON response"""
        try:
            # Extract JSON from response
            json_match = re.search(r'\{.*\}', response_text, re.DOTALL)
            if not json_match:
                return self._classify_via_heuristic(user_input)
            
            data = json.loads(json_match.group())
            
            # Map type
            type_str = data.get("type", "complex").lower()
            intent_type = IntentType.COMPLEX_REASONING
            if "fast" in type_str:
                intent_type = IntentType.FAST_TRACK
            elif "vision" in type_str:
                intent_type = IntentType.VISION_CONTEXT
            
            # Map category
            category_str = data.get("category", "research").lower()
            category = IntentCategory.RESEARCH
            for cat in IntentCategory:
                if cat.value in category_str:
                    category = cat
                    break
            
            return IntentClassification(
                intent_type=intent_type,
                category=category,
                confidence=float(data.get("confidence", 0.7)),
                reasoning=data.get("reasoning", "Ollama classification"),
                suggested_tool=self._tool_for_category(category),
                requires_vision=intent_type == IntentType.VISION_CONTEXT,
                response_time_ms=0,
                processed_at=datetime.now().isoformat(),
            )
        
        except Exception as e:
            logger.warning(f"Failed to parse Ollama response: {e}")
            return self._classify_via_heuristic(user_input)
    
    def _tool_for_category(self, category: IntentCategory) -> str:
        """Map category to suggested tool"""
        mapping = {
            IntentCategory.HARDWARE_CONTROL: "system_control",
            IntentCategory.MEMORY_QUERY: "graphrag",
            IntentCategory.QUICK_TASK: "task_manager",
            IntentCategory.CODE_GENERATION: "gemini_live",
            IntentCategory.CODE_ANALYSIS: "gemini_live",
            IntentCategory.DEBUGGING: "vision_service",
            IntentCategory.RESEARCH: "gemini_live",
            IntentCategory.BUSINESS: "gemini_live",
            IntentCategory.MUSIC_MEDIA: "media_control",
        }
        return mapping.get(category, "gemini_live")
    
    def _clean_cache(self) -> None:
        """Remove expired cache entries"""
        cutoff = datetime.now() - self._cache_ttl
        expired = [
            k for k, v in self._cache.items()
            if datetime.fromisoformat(v.processed_at) < cutoff
        ]
        for k in expired:
            del self._cache[k]
        
        self._last_cache_clean = datetime.now()
        if expired:
            logger.debug(f"Cleaned {len(expired)} expired cache entries")


# Global instance
_classifier: Optional[LocalIntentClassifier] = None


async def get_intent_classifier() -> LocalIntentClassifier:
    """Get or create global classifier instance"""
    global _classifier
    if _classifier is None:
        _classifier = LocalIntentClassifier()
        await _classifier.initialize()
    return _classifier


__all__ = [
    "LocalIntentClassifier",
    "IntentClassification",
    "IntentType",
    "IntentCategory",
    "get_intent_classifier",
]
