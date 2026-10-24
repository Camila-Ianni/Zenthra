"""
Phase 8 Integration Test: Full Reflex Protocol Workflow

Tests the complete flow from user input to routing decision.
"""

import pytest

from core.intent_router import IntentRouter, get_intent_router
from core.local_intent_classifier import LocalIntentClassifier, IntentType
from core.vision_service import VisionService


class TestIntentRouterE2E:
    """End-to-end tests for intent routing"""
    
    @pytest.mark.asyncio
    async def test_fast_track_hardware_routing(self):
        """Test hardware command routes to fast_track"""
        classifier = LocalIntentClassifier()
        router = IntentRouter(classifier)
        
        decision = await router.route("set brightness to 50")
        
        assert decision.should_execute_locally is True
        assert decision.route == "fast_track"
        assert decision.tool == "system_control"
    
    @pytest.mark.asyncio
    async def test_complex_reasoning_routing(self):
        """Test code generation routes to cloud"""
        classifier = LocalIntentClassifier()
        router = IntentRouter(classifier)
        
        decision = await router.route("write a REST API in Go")
        
        assert decision.should_execute_locally is False
        assert decision.route == "cloud"
        assert decision.tool == "gemini_live"
    
    @pytest.mark.asyncio
    async def test_vision_context_routing(self):
        """Test debugging routes to vision_context"""
        classifier = LocalIntentClassifier()
        router = IntentRouter(classifier)
        
        decision = await router.route("what's wrong with this error?")
        
        assert decision.should_execute_locally is False
        assert decision.route == "vision_context"
        # Vision context enrichments may be empty if screencapture fails
        assert isinstance(decision.context_enrichments, dict)
    
    @pytest.mark.asyncio
    async def test_routing_decision_contains_classification(self):
        """Test that routing decision includes classification details"""
        classifier = LocalIntentClassifier()
        router = IntentRouter(classifier)
        
        decision = await router.route("find my Python projects")
        
        assert decision.intent_classification is not None
        assert decision.intent_classification.intent_type == IntentType.FAST_TRACK
        assert decision.reasoning != ""


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
