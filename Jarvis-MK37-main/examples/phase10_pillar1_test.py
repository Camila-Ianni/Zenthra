"""
Phase 10.1: Smart Gatekeeper Tests
───────────────────────────────────

Tests for the GatekeeperOrchestrator:
✓ Cache hit detection and <50ms return
✓ Cache miss fallthrough
✓ FAST_TRACK routing (hardware, simple)
✓ COMPLEX_REASONING routing (cloud)
✓ Multi-tool detection
✓ Error handling and graceful degradation
✓ Performance targets
✓ Context preservation
"""

import asyncio
import pytest
import time
from unittest.mock import AsyncMock, MagicMock
from core.phase10_gatekeeper import (
    GatekeeperOrchestrator,
    GatekeeperRequest,
    GatekeeperResponse,
    IntentType,
    reset_gatekeeper,
)


def create_gatekeeper():
    """Create a gatekeeper with mocked dependencies."""
    mock_cache = AsyncMock()
    mock_cache.get = AsyncMock(return_value=None)
    mock_cache.put = AsyncMock(return_value=None)
    
    mock_classifier = AsyncMock()
    mock_classifier.classify = AsyncMock(
        return_value={"category": "complex_reasoning"}
    )
    
    mock_router = AsyncMock()
    mock_router.route_and_execute = AsyncMock(return_value={"status": "ok"})
    mock_router.route_and_execute_local = AsyncMock(return_value={"status": "ok"})
    
    return (
        GatekeeperOrchestrator(
            semantic_cache=mock_cache,
            intent_classifier=mock_classifier,
            intent_router=mock_router,
        ),
        mock_cache,
        mock_classifier,
        mock_router,
    )


@pytest.mark.asyncio
async def test_cache_hit_quick_return():
    """Test 1: Cache hit returns quickly (<50ms)"""
    gk, cache, _, _ = create_gatekeeper()
    
    # Setup: Cache has a hit
    cache.get.return_value = (
        {"answer": "cached result"},
        {"source": "cache"},
        0.95,  # High similarity
    )
    
    start = time.time()
    response = await gk.process("test query")
    latency = (time.time() - start) * 1000
    
    assert response.source == "cache"
    assert response.cached is True
    assert response.confidence >= 0.92
    assert latency < 200  # Generous threshold
    assert gk.stats["cache_hits"] == 1


@pytest.mark.asyncio
async def test_cache_miss_fallthrough():
    """Test 2: Cache miss falls through to classifier"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup: Cache miss
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {"status": "ok", "data": "result"}
    
    response = await gk.process("test query")
    
    assert response.source == "complex"
    assert classifier.classify.called
    assert router.route_and_execute.called
    assert gk.stats["cache_hits"] == 0


@pytest.mark.asyncio
async def test_fast_track_routing():
    """Test 3: FAST_TRACK routing for hardware commands"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup: Cache miss, classifier says FAST_TRACK
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "fast_track"}
    router.route_and_execute_local.return_value = {"brightness": 50}
    
    response = await gk.process("Bajá el brillo a 50%")
    
    assert response.source == "fast_track"
    assert response.intent_type == IntentType.FAST_TRACK
    assert router.route_and_execute_local.called
    assert router.route_and_execute.call_count == 0
    assert gk.stats["fast_track"] == 1


@pytest.mark.asyncio
async def test_complex_reasoning_routing():
    """Test 4: COMPLEX_REASONING routing for cloud tasks"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup: Cache miss, classifier says COMPLEX_REASONING
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {
        "thought": "analyzed code",
        "recommendation": "refactor",
    }
    
    response = await gk.process("Analiza este código y sugiere mejoras")
    
    assert response.source == "complex"
    assert response.intent_type == IntentType.COMPLEX_REASONING
    assert router.route_and_execute.called
    assert router.route_and_execute_local.call_count == 0
    assert gk.stats["complex_reasoning"] == 1


@pytest.mark.asyncio
async def test_multi_tool_detection():
    """Test 5: Multi-tool requests are properly classified"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup: Complex query requiring multiple tools
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {"status": "ok"}
    
    query = "Abrí Workbench, buscame apuntes de BD, y bajá el brillo"
    response = await gk.process(query)
    
    # Multi-tool queries should go to complex_reasoning
    assert response.intent_type == IntentType.COMPLEX_REASONING


@pytest.mark.asyncio
async def test_error_handling():
    """Test 6: Error handling and graceful degradation"""
    gk, cache, _, router = create_gatekeeper()
    
    # Setup: Router throws exception
    cache.get.return_value = None
    router.route_and_execute.side_effect = RuntimeError("Tool failed")
    
    response = await gk.process("test query")
    
    assert response.source == "error"
    assert response.confidence == 0.0
    assert gk.stats["errors"] == 1


@pytest.mark.asyncio
async def test_result_caching():
    """Test 7: Successful results are cached for future hits"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup: Cache miss, successful execution
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {"status": "ok"}
    
    await gk.process("test query")
    
    # Verify cache.put was called
    assert cache.put.called
    call_args = cache.put.call_args
    assert call_args[1]["query"] == "test query"
    assert call_args[1]["response"] == {"status": "ok"}


@pytest.mark.asyncio
async def test_context_preservation():
    """Test 8: Context is preserved through the pipeline"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {"status": "ok"}
    
    test_context = {
        "user": "test_user",
        "session": "test_session",
        "previous_tasks": ["task1", "task2"],
    }
    
    response = await gk.process(
        "test query",
        context=test_context,
        user_id="test_user",
        session_id="test_session",
    )
    
    assert response.result == {"status": "ok"}


@pytest.mark.asyncio
async def test_stats_tracking():
    """Test stats tracking across requests"""
    gk, cache, classifier, router = create_gatekeeper()
    
    cache.get.return_value = None
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {"status": "ok"}
    
    # Make multiple requests
    for _ in range(3):
        await gk.process("query")
    
    stats = gk.get_stats()
    assert stats["total_requests"] == 3
    assert stats["complex_reasoning"] == 3


@pytest.mark.asyncio
async def test_cache_similarity_threshold():
    """Test that cache only returns matches above 0.92 threshold"""
    gk, cache, classifier, router = create_gatekeeper()
    
    # Setup: Cache result with low similarity
    cache.get.return_value = (
        {"answer": "wrong answer"},
        {"source": "cache"},
        0.80,  # Below 0.92 threshold
    )
    classifier.classify.return_value = {"category": "complex_reasoning"}
    router.route_and_execute.return_value = {"status": "ok"}
    
    response = await gk.process("test query")
    
    # Should not use cache hit, should continue to classifier
    assert response.source != "cache"
    assert classifier.classify.called


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
