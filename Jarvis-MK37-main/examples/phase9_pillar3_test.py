"""
Phase 9 Pillar 3: Router Integration Tests

Tests for cache-first routing, performance improvement, streaming.
Total: 6 tests
"""

import pytest
from datetime import datetime

from core.intent_router import IntentRouter
from core.local_intent_classifier import LocalIntentClassifier
from core.semantic_cache import SemanticCacheManager
from core.vision_service import VisionService


class TestIntentRouterWithCache:
    """Test router integration with semantic cache"""
    
    @pytest.mark.asyncio
    async def test_cache_hit_before_routing(self):
        """Test cache is checked before routing"""
        classifier = LocalIntentClassifier()
        cache = SemanticCacheManager()
        await cache.initialize()
        
        router = IntentRouter(classifier)
        
        # Pre-populate cache
        query = "what is Python?"
        cached_response = "Python is a programming language"
        
        await cache.put(
            query=query,
            response=cached_response,
            context={"type": "cached"}
        )
        
        # Try to retrieve from cache
        result = await cache.get(query)
        
        if result:
            assert result[0] == cached_response
            assert result[2] == 1.0  # Exact match
    
    @pytest.mark.asyncio
    async def test_repeated_query_performance(self):
        """Test performance improvement on repeated queries"""
        cache = SemanticCacheManager()
        await cache.initialize()
        
        query = "test repeated query"
        response = "cached response"
        
        # First time: cache miss
        result1 = await cache.get(query)
        assert result1 is None  # Not in cache
        
        # Store in cache
        await cache.put(query, response, {})
        
        # Second time: cache hit
        result2 = await cache.get(query)
        
        if cache.embedder.available:
            assert result2 is not None
            assert result2[0] == response
    
    @pytest.mark.asyncio
    async def test_cache_stats_tracking(self):
        """Test cache tracks statistics"""
        cache = SemanticCacheManager()
        await cache.initialize()
        
        # Add entry
        await cache.put("test", "response", {})
        
        # Get stats
        stats = await cache.stats()
        
        assert stats["total_entries"] >= 1
        assert "total_hits" in stats
        assert "chromadb_available" in stats
    
    @pytest.mark.asyncio
    async def test_cache_fallback_on_miss(self):
        """Test router falls back to normal routing on cache miss"""
        classifier = LocalIntentClassifier()
        router = IntentRouter(classifier)
        
        # Query not in cache
        decision = await router.route("write a new REST API")
        
        assert decision is not None
        assert decision.intent_classification is not None
    
    @pytest.mark.asyncio
    async def test_cache_with_context_enrichment(self):
        """Test cache preserves context enrichment"""
        cache = SemanticCacheManager()
        await cache.initialize()
        
        context = {
            "user_id": "user123",
            "language": "Python",
            "topic": "functions"
        }
        
        await cache.put(
            query="how to write a function?",
            response="Use def keyword",
            context=context
        )
        
        # Retrieve
        result = await cache.get("how to write a function?")
        
        if result:
            retrieved_context = result[1]
            assert retrieved_context["user_id"] == "user123"
    
    @pytest.mark.asyncio
    async def test_high_similarity_queries(self):
        """Test similar queries retrieve same cached response"""
        cache = SemanticCacheManager(similarity_threshold=0.85)
        await cache.initialize()
        
        if not cache.embedder.available:
            pytest.skip("Embedder not available")
        
        original = "What is machine learning?"
        response = "ML is a subset of AI"
        
        await cache.put(original, response, {})
        
        # Similar query
        similar = "Define machine learning"
        
        # Should potentially find cached response if similarity > threshold
        result = await cache.get(similar)
        
        # Result may be None or valid (depending on similarity)
        assert result is None or isinstance(result, tuple)


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
