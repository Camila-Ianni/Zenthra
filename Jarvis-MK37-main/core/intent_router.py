"""
Phase 8 Pillar 2: Intent Router

Orchestrates routing decisions between local fast-track processing and cloud APIs.
Implements the "Reflex Protocol" - hybrid local/cloud processing architecture.
"""

import logging
from typing import Optional, Dict, Any
from dataclasses import dataclass

from core.local_intent_classifier import (
    LocalIntentClassifier, IntentClassification, IntentType
)
from core.vision_service import VisionService


logger = logging.getLogger(__name__)


@dataclass
class RoutingDecision:
    """Represents a routing decision for user intent"""
    should_execute_locally: bool
    route: str  # "fast_track", "cloud", "vision_context"
    tool: str
    context_enrichments: Dict[str, Any]
    intent_classification: IntentClassification
    reasoning: str


class IntentRouter:
    """
    Routes classified intents to appropriate execution paths.
    
    Implements decision tree:
    - FAST_TRACK → Local execution (system_control, graphrag, media_control)
    - VISION_CONTEXT → Capture screen, then route to cloud
    - COMPLEX_REASONING → Cloud APIs (Gemini Live, Kimi)
    """
    
    def __init__(
        self,
        classifier: LocalIntentClassifier,
        vision_service: Optional[VisionService] = None
    ):
        self.classifier = classifier
        self.vision_service = vision_service or VisionService()
    
    async def route(self, user_input: str) -> RoutingDecision:
        """
        Classify user input and make routing decision.
        
        Returns:
            RoutingDecision with execution path and context
        """
        # Classify intent
        classification = await self.classifier.classify(user_input)
        
        # Build routing decision based on classification
        if classification.intent_type == IntentType.FAST_TRACK:
            return self._route_fast_track(classification)
        
        elif classification.intent_type == IntentType.VISION_CONTEXT:
            return await self._route_vision_context(classification)
        
        else:  # COMPLEX_REASONING
            return self._route_complex_reasoning(classification)
    
    def _route_fast_track(self, classification: IntentClassification) -> RoutingDecision:
        """Route FAST_TRACK intents to local execution"""
        logger.info(f"FAST_TRACK route: {classification.category.value}")
        
        return RoutingDecision(
            should_execute_locally=True,
            route="fast_track",
            tool=classification.suggested_tool,
            context_enrichments={},
            intent_classification=classification,
            reasoning=f"Fast-track execution via {classification.suggested_tool}"
        )
    
    async def _route_vision_context(
        self, classification: IntentClassification
    ) -> RoutingDecision:
        """Route VISION_CONTEXT intents: capture screen, enrich context, send to cloud"""
        logger.info(f"VISION_CONTEXT route: {classification.category.value}")
        
        # Capture current screen for context
        context_enrichments = {}
        try:
            capture = await self.vision_service.capture_active_window()
            if capture:
                context_enrichments["vision_context"] = {
                    "image_path": str(capture.image_path),
                    "text_extracted": capture.text_extracted,
                    "region": capture.region,
                    "confidence": capture.confidence
                }
                logger.debug(f"Vision context captured from {capture.region}")
        except Exception as e:
            logger.warning(f"Failed to capture vision context: {e}")
            # Continue without vision context
        
        return RoutingDecision(
            should_execute_locally=False,
            route="vision_context",
            tool="gemini_live",
            context_enrichments=context_enrichments,
            intent_classification=classification,
            reasoning="Vision-enriched cloud reasoning via Gemini Live"
        )
    
    def _route_complex_reasoning(self, classification: IntentClassification) -> RoutingDecision:
        """Route COMPLEX_REASONING intents to cloud APIs"""
        logger.info(f"COMPLEX_REASONING route: {classification.category.value}")
        
        return RoutingDecision(
            should_execute_locally=False,
            route="cloud",
            tool=classification.suggested_tool,
            context_enrichments={},
            intent_classification=classification,
            reasoning=f"Cloud reasoning via {classification.suggested_tool}"
        )


async def get_intent_router() -> IntentRouter:
    """
    Get or create singleton IntentRouter instance.
    """
    if not hasattr(get_intent_router, "_instance"):
        classifier = await LocalIntentClassifier.get_classifier()
        vision_service = await VisionService.get_service()
        get_intent_router._instance = IntentRouter(classifier, vision_service)
    
    return get_intent_router._instance
