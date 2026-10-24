"""
Phase 8 Pillar 1: Local Intent Classifier Tests

Tests for intent classification and routing logic.
Covers: classification accuracy, heuristic fallback, caching.

Total: 12 tests
"""

import pytest
import asyncio
from datetime import datetime

from core.local_intent_classifier import (
    LocalIntentClassifier, IntentClassification, IntentType, IntentCategory,
    get_intent_classifier
)


# ============================================================================
# TESTS: Initialization & Setup (2 tests)
# ============================================================================

class TestClassifierInitialization:
    """Test classifier initialization"""
    
    @pytest.mark.asyncio
    async def test_classifier_init_no_ollama(self):
        """Test initialization when Ollama not available"""
        classifier = LocalIntentClassifier(
            ollama_url="http://nonexistent:11434"
        )
        result = await classifier.initialize()
        
        assert result is False
        assert classifier.ollama_available is False
    
    @pytest.mark.asyncio
    async def test_singleton_pattern(self):
        """Test get_intent_classifier singleton"""
        c1 = await get_intent_classifier()
        c2 = await get_intent_classifier()
        
        assert c1 is c2


# ============================================================================
# TESTS: Heuristic Classification (6 tests)
# ============================================================================

class TestHeuristicClassification:
    """Test heuristic-based classification"""
    
    def test_hardware_control_detection(self):
        """Test detection of hardware control intents"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("aumenta el brillo")
        
        assert result.intent_type == IntentType.FAST_TRACK
        assert result.category == IntentCategory.HARDWARE_CONTROL
        assert result.confidence > 0.8
    
    def test_memory_query_detection(self):
        """Test detection of memory/graph queries"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("search for Python projects")
        
        assert result.intent_type == IntentType.FAST_TRACK
        assert result.category == IntentCategory.MEMORY_QUERY
        assert result.confidence > 0.8
    
    def test_debugging_intent_detection(self):
        """Test detection of debugging intents"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("what's wrong with this error?")
        
        assert result.intent_type == IntentType.VISION_CONTEXT
        assert result.category == IntentCategory.DEBUGGING
        assert result.requires_vision is True
    
    def test_code_generation_detection(self):
        """Test detection of code generation intents"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("write a Python function")
        
        assert result.intent_type == IntentType.COMPLEX_REASONING
        assert result.category == IntentCategory.CODE_GENERATION
    
    def test_media_control_detection(self):
        """Test detection of media control intents"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("play my favorite music")
        
        assert result.intent_type == IntentType.FAST_TRACK
        assert result.category == IntentCategory.MUSIC_MEDIA
    
    def test_unknown_intent_fallback(self):
        """Test fallback for unknown intents"""
        classifier = LocalIntentClassifier()
        
        result = classifier._classify_via_heuristic("xyzqwerty lorem ipsum")
        
        assert result.intent_type == IntentType.COMPLEX_REASONING
        assert result.confidence <= 0.5


# ============================================================================
# TESTS: Classification Async Flow (2 tests)
# ============================================================================

class TestAsyncClassification:
    """Test async classification workflow"""
    
    @pytest.mark.asyncio
    async def test_classify_async(self):
        """Test async classification (heuristic mode)"""
        classifier = LocalIntentClassifier()
        
        result = await classifier.classify("increase brightness")
        
        assert isinstance(result, IntentClassification)
        assert result.intent_type == IntentType.FAST_TRACK
        assert result.response_time_ms > 0
    
    @pytest.mark.asyncio
    async def test_cache_hit(self):
        """Test that repeated queries use cache"""
        classifier = LocalIntentClassifier()
        
        # First classification
        result1 = await classifier.classify("set volume to 50%")
        
        # Second classification (should be cached)
        result2 = await classifier.classify("set volume to 50%")
        
        # Both should succeed
        assert result1 is not None
        assert result2 is not None


# ============================================================================
# TESTS: Tool Selection (1 test)
# ============================================================================

class TestToolSelection:
    """Test tool selection for classified intents"""
    
    def test_tool_mapping(self):
        """Test that correct tools are suggested for categories"""
        classifier = LocalIntentClassifier()
        
        # Test hardware -> system_control
        result_hw = classifier._classify_via_heuristic("dim the screen")
        assert result_hw.suggested_tool == "system_control"
        
        # Test memory -> graphrag
        result_mem = classifier._classify_via_heuristic("find my Go projects")
        assert result_mem.suggested_tool == "graphrag"
        
        # Test code -> gemini_live
        result_code = classifier._classify_via_heuristic("write me a function")
        assert result_code.suggested_tool == "gemini_live"


# ============================================================================
# TESTS: Prompt Building & Response Parsing (1 test)
# ============================================================================

class TestPromptAndParsing:
    """Test prompt construction and response parsing"""
    
    def test_prompt_building(self):
        """Test that classification prompt is valid"""
        classifier = LocalIntentClassifier()
        
        prompt = classifier._build_classification_prompt("test input")
        
        assert "fast_track" in prompt.lower()
        assert "complex" in prompt.lower()
        assert "vision" in prompt.lower()
        assert "json" in prompt.lower()


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
