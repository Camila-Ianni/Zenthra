"""
Phase 10: Smart Gatekeeper Orchestrator
────────────────────────────────────────

The Gatekeeper is the unified entry point for all user requests. It implements
a three-step decision tree:

1. SemanticCache lookup (ChromaDB) → <50ms if hit
2. LocalIntentClassifier routing → FAST_TRACK vs COMPLEX_REASONING
3. Tool execution + result caching

This module integrates:
- SemanticCacheManager (core/semantic_cache.py)
- LocalIntentClassifier (core/local_intent_classifier.py)
- IntentRouter (core/intent_router.py)
"""

import asyncio
import logging
from typing import Optional, Dict, Any, List
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger("GatekeeperOrchestrator")


class IntentType(str, Enum):
    """Classification of user intent."""
    FAST_TRACK = "fast_track"  # Hardware, simple memory queries
    COMPLEX_REASONING = "complex_reasoning"  # Code analysis, creative tasks
    UNKNOWN = "unknown"


@dataclass
class GatekeeperRequest:
    """A unified request structure for the gatekeeper."""
    query: str
    context: Dict[str, Any]
    user_id: str = "default"
    session_id: str = "default"
    retry_count: int = 0


@dataclass
class GatekeeperResponse:
    """A unified response from the gatekeeper."""
    result: Any
    source: str  # "cache", "fast_track", "complex", "error"
    latency_ms: float
    intent_type: IntentType
    cached: bool = False
    confidence: float = 1.0


class GatekeeperOrchestrator:
    """
    Unified orchestrator for all JARVIS requests.
    
    Flow:
    1. Process request through cache (check if answer exists)
    2. Classify intent (FAST_TRACK vs COMPLEX_REASONING)
    3. Route to appropriate executor
    4. Cache result for future hits
    """

    def __init__(
        self,
        semantic_cache,
        intent_classifier,
        intent_router,
        vision_service=None,
    ):
        """
        Initialize the gatekeeper with required components.
        
        Args:
            semantic_cache: SemanticCacheManager instance
            intent_classifier: LocalIntentClassifier instance
            intent_router: IntentRouter instance
            vision_service: Optional VisionService for context enrichment
        """
        self.semantic_cache = semantic_cache
        self.intent_classifier = intent_classifier
        self.intent_router = intent_router
        self.vision_service = vision_service
        
        self.stats = {
            "total_requests": 0,
            "cache_hits": 0,
            "fast_track": 0,
            "complex_reasoning": 0,
            "errors": 0,
        }

    async def process(
        self,
        query: str,
        context: Optional[Dict[str, Any]] = None,
        user_id: str = "default",
        session_id: str = "default",
    ) -> GatekeeperResponse:
        """
        Process a user request through the gatekeeper pipeline.
        
        Args:
            query: User input query
            context: Optional context dictionary
            user_id: User identifier
            session_id: Session identifier
            
        Returns:
            GatekeeperResponse with result and metadata
        """
        import time
        start_time = time.time()
        
        request = GatekeeperRequest(
            query=query,
            context=context or {},
            user_id=user_id,
            session_id=session_id,
        )
        
        self.stats["total_requests"] += 1
        
        try:
            # Step 1: Check semantic cache for instant hit
            response = await self._check_cache(request)
            if response:
                response.latency_ms = (time.time() - start_time) * 1000
                return response

            # Step 2: Classify intent (FAST_TRACK vs COMPLEX_REASONING)
            intent_type = await self._classify_intent(request)
            
            # Step 3: Route and execute based on intent
            if intent_type == IntentType.FAST_TRACK:
                response = await self._execute_fast_track(request)
                self.stats["fast_track"] += 1
            elif intent_type == IntentType.COMPLEX_REASONING:
                response = await self._execute_complex_reasoning(request)
                self.stats["complex_reasoning"] += 1
            else:
                raise ValueError(f"Unknown intent type: {intent_type}")

            # Step 4: Cache the result for future hits
            await self._cache_result(request, response)
            
            response.latency_ms = (time.time() - start_time) * 1000
            return response

        except Exception as e:
            logger.error(f"Gatekeeper error: {e}", exc_info=True)
            self.stats["errors"] += 1
            return GatekeeperResponse(
                result={"error": str(e)},
                source="error",
                latency_ms=(time.time() - start_time) * 1000,
                intent_type=IntentType.UNKNOWN,
                confidence=0.0,
            )

    async def _check_cache(self, request: GatekeeperRequest) -> Optional[GatekeeperResponse]:
        """
        Step 1: Check semantic cache for quick hits.
        
        Target: <50ms for cache hits
        """
        try:
            cache_result = await self.semantic_cache.get(request.query)
            if cache_result:
                response, context, similarity = cache_result
                if similarity >= 0.92:  # High confidence match
                    self.stats["cache_hits"] += 1
                    return GatekeeperResponse(
                        result=response,
                        source="cache",
                        latency_ms=0.0,  # Will be updated in process()
                        intent_type=self._infer_intent_from_context(context),
                        cached=True,
                        confidence=similarity,
                    )
        except Exception as e:
            logger.warning(f"Cache lookup failed: {e}")
        
        return None

    async def _classify_intent(self, request: GatekeeperRequest) -> IntentType:
        """
        Step 2: Classify whether request is FAST_TRACK or COMPLEX_REASONING.
        
        FAST_TRACK: Hardware commands, simple memory queries, quick lookups
        COMPLEX_REASONING: Code analysis, creative tasks, cloud reasoning
        """
        try:
            classification = await self.intent_classifier.classify(request.query)
            
            # Map classifier output to IntentType
            if classification.get("category") == "fast_track":
                return IntentType.FAST_TRACK
            elif classification.get("category") in ["hardware", "simple", "memory"]:
                return IntentType.FAST_TRACK
            else:
                return IntentType.COMPLEX_REASONING
                
        except Exception as e:
            logger.warning(f"Intent classification failed: {e}, defaulting to COMPLEX_REASONING")
            return IntentType.COMPLEX_REASONING

    async def _execute_fast_track(self, request: GatekeeperRequest) -> GatekeeperResponse:
        """
        Execute FAST_TRACK requests locally (hardware, memory, quick lookups).
        """
        try:
            # Route to appropriate tool without cloud
            result = await self.intent_router.route_and_execute_local(
                request.query,
                request.context,
            )
            
            return GatekeeperResponse(
                result=result,
                source="fast_track",
                latency_ms=0.0,
                intent_type=IntentType.FAST_TRACK,
                confidence=0.95,
            )
        except Exception as e:
            logger.error(f"Fast track execution failed: {e}")
            raise

    async def _execute_complex_reasoning(self, request: GatekeeperRequest) -> GatekeeperResponse:
        """
        Execute COMPLEX_REASONING requests via cloud (Gemini, etc).
        """
        try:
            result = await self.intent_router.route_and_execute(
                request.query,
                request.context,
            )
            
            return GatekeeperResponse(
                result=result,
                source="complex",
                latency_ms=0.0,
                intent_type=IntentType.COMPLEX_REASONING,
                confidence=0.90,
            )
        except Exception as e:
            logger.error(f"Complex reasoning execution failed: {e}")
            raise

    async def _cache_result(self, request: GatekeeperRequest, response: GatekeeperResponse) -> None:
        """
        Cache successful results for future cache hits.
        """
        if response.source == "error":
            return  # Don't cache errors
        
        try:
            await self.semantic_cache.put(
                query=request.query,
                response=response.result,
                context={
                    "source": response.source,
                    "intent": response.intent_type.value,
                    "user_id": request.user_id,
                },
            )
        except Exception as e:
            logger.warning(f"Failed to cache result: {e}")

    def _infer_intent_from_context(self, context: Dict[str, Any]) -> IntentType:
        """Infer intent type from cached context."""
        intent_str = context.get("intent", "unknown")
        try:
            return IntentType(intent_str)
        except ValueError:
            return IntentType.COMPLEX_REASONING

    def get_stats(self) -> Dict[str, Any]:
        """Get gatekeeper statistics."""
        return {
            **self.stats,
            "cache_hit_rate": (
                self.stats["cache_hits"] / max(self.stats["total_requests"], 1)
            ),
        }

    async def reset_stats(self) -> None:
        """Reset statistics counters."""
        self.stats = {
            "total_requests": 0,
            "cache_hits": 0,
            "fast_track": 0,
            "complex_reasoning": 0,
            "errors": 0,
        }


# Singleton instance
_gatekeeper_instance: Optional[GatekeeperOrchestrator] = None


async def get_gatekeeper(
    semantic_cache=None,
    intent_classifier=None,
    intent_router=None,
    vision_service=None,
) -> GatekeeperOrchestrator:
    """
    Get or create the gatekeeper singleton instance.
    
    Args:
        semantic_cache: SemanticCacheManager instance
        intent_classifier: LocalIntentClassifier instance
        intent_router: IntentRouter instance
        vision_service: Optional VisionService instance
        
    Returns:
        GatekeeperOrchestrator singleton
    """
    global _gatekeeper_instance
    
    if _gatekeeper_instance is None:
        # Lazy import to avoid circular dependencies
        if semantic_cache is None:
            from core.semantic_cache import get_semantic_cache
            semantic_cache = await get_semantic_cache()
        
        if intent_classifier is None:
            from core.local_intent_classifier import LocalIntentClassifier
            intent_classifier = LocalIntentClassifier()
        
        if intent_router is None:
            from core.intent_router import IntentRouter
            intent_router = IntentRouter()
        
        _gatekeeper_instance = GatekeeperOrchestrator(
            semantic_cache=semantic_cache,
            intent_classifier=intent_classifier,
            intent_router=intent_router,
            vision_service=vision_service,
        )
    
    return _gatekeeper_instance


async def reset_gatekeeper() -> None:
    """Reset the gatekeeper singleton (for testing)."""
    global _gatekeeper_instance
    _gatekeeper_instance = None
