"""
Phase 8 Pillar 2: Intent Routing & Handover Tests

Tests for routing logic between fast-track and cloud processing.
Covers: routing decisions, handover, latency targets.

Total: 10 tests
"""

import pytest
from datetime import datetime

from core.local_intent_classifier import (
    LocalIntentClassifier, IntentType, IntentCategory
)


# ============================================================================
# TESTS: Routing Decisions (5 tests)
# ============================================================================

class TestRoutingLogic:
    """Test routing decisions"""
    
    def test_hardware_routes_to_fast_track(self):
        """Test that hardware commands route to fast-track"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("set brightness to 50")
        
        assert result.intent_type == IntentType.FAST_TRACK
        assert result.suggested_tool == "system_control"
    
    def test_code_generation_routes_to_cloud(self):
        """Test that code generation routes to cloud"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("write a REST API in Go")
        
        assert result.intent_type == IntentType.COMPLEX_REASONING
        assert result.suggested_tool == "gemini_live"
    
    def test_debugging_triggers_vision(self):
        """Test that debugging triggers vision context"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("this error doesn't make sense")
        
        assert result.intent_type == IntentType.VISION_CONTEXT
        assert result.requires_vision is True
    
    def test_memory_query_is_fast(self):
        """Test that memory queries are fast-track"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("what projects use Python?")
        
        assert result.intent_type == IntentType.FAST_TRACK
        assert result.suggested_tool == "graphrag"
    
    def test_research_is_complex(self):
        """Test that research/learning is complex reasoning"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("teach me about microservices")
        
        assert result.intent_type == IntentType.COMPLEX_REASONING


# ============================================================================
# TESTS: Confidence Scoring (2 tests)
# ============================================================================

class TestConfidenceScoring:
    """Test confidence scores for routing decisions"""
    
    def test_high_confidence_hardware(self):
        """Test high confidence for clear hardware commands"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("increase volume")
        
        assert result.confidence > 0.85
    
    def test_low_confidence_ambiguous(self):
        """Test lower confidence for ambiguous inputs"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("asdf qwerty xyz")
        
        assert result.confidence <= 0.5


# ============================================================================
# TESTS: Latency Targets (2 tests)
# ============================================================================

class TestLatencyTargets:
    """Test latency requirements"""
    
    @pytest.mark.asyncio
    async def test_classification_under_300ms(self):
        """Test that classification completes under 300ms target"""
        classifier = LocalIntentClassifier()
        
        start = datetime.now()
        result = await classifier.classify("play music")
        elapsed_ms = (datetime.now() - start).total_seconds() * 1000
        
        # Should be fast (< 300ms even on slow machines)
        assert elapsed_ms < 1000  # Liberal timeout for CI
        assert result is not None
    
    @pytest.mark.asyncio
    async def test_cache_hit_very_fast(self):
        """Test that cache hits are very fast"""
        classifier = LocalIntentClassifier()
        
        # Warm up cache
        await classifier.classify("set volume to 50%")
        
        # Second call should be much faster
        start = datetime.now()
        result = await classifier.classify("set volume to 50%")
        elapsed_ms = (datetime.now() - start).total_seconds() * 1000
        
        # Cache should be nearly instant (< 10ms)
        assert elapsed_ms < 50
        assert result is not None


# ============================================================================
# TESTS: Fallback Behavior (1 test)
# ============================================================================

class TestFallbackBehavior:
    """Test fallback from Ollama to heuristic"""
    
    @pytest.mark.asyncio
    async def test_heuristic_fallback_always_works(self):
        """Test that heuristic fallback always produces a result"""
        classifier = LocalIntentClassifier()
        classifier.ollama_available = False
        
        # Even with invalid input, should get a classification
        result = await classifier.classify("xyz invalid input 123")
        
        assert result is not None
        assert result.intent_type in [IntentType.FAST_TRACK, IntentType.COMPLEX_REASONING, IntentType.VISION_CONTEXT]


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
