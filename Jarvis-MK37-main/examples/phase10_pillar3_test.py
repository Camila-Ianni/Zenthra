"""
Phase 10.3: The Watcher Tests
──────────────────────────────

Tests for Watcher proactive monitoring:
✓ Shell error capture
✓ Error type classification
✓ Severity classification
✓ Pattern matching
✓ GraphRAG queries
✓ Notification logic
✓ New error persistence
✓ Background loop management
"""

import pytest
import asyncio
import time
from unittest.mock import AsyncMock, MagicMock
from core.phase10_watcher import (
    Watcher,
    DetectedError,
    ErrorSeverity,
    KnownErrorMatch,
    get_watcher,
    reset_watcher,
)


def create_watcher():
    """Create a watcher with mocked dependencies."""
    vision_service = AsyncMock()
    vision_service.capture_active_window = AsyncMock(return_value="")
    
    graphrag = AsyncMock()
    graphrag.query_cross_language_errors = AsyncMock(return_value=[])
    
    extractor = AsyncMock()
    extractor.extract_and_persist_learnings = AsyncMock(return_value={"id": "learning-1"})
    
    return Watcher(
        vision_service=vision_service,
        graphrag_overlay=graphrag,
        learning_extractor=extractor,
    ), vision_service, graphrag, extractor


@pytest.mark.asyncio
async def test_shell_error_capture():
    """Test 1: Capture errors from shell output"""
    watcher, _, _, _ = create_watcher()
    
    output = """
Traceback (most recent call last):
  File "main.py", line 10, in <module>
    SyntaxError: invalid syntax
"""
    
    errors = await watcher.capture_stderr_output(output)
    
    assert len(errors) > 0
    assert any("SyntaxError" in e.error_type for e in errors)
    assert watcher.stats["errors_detected"] > 0


@pytest.mark.asyncio
async def test_error_type_classification():
    """Test 2: Classify error types"""
    watcher, _, _, _ = create_watcher()
    
    test_cases = [
        ("SyntaxError: invalid syntax", "SyntaxError"),
        ("ImportError: cannot import", "ImportError"),
        ("ConnectionError: connection refused", "ConnectionError"),
        ("TimeoutError: timeout occurred", "TimeoutError"),
    ]
    
    for error_text, expected_type in test_cases:
        error_type = watcher._classify_error_type(error_text)
        assert error_type == expected_type


@pytest.mark.asyncio
async def test_severity_classification():
    """Test 3: Classify error severity"""
    watcher, _, _, _ = create_watcher()
    
    critical = watcher._classify_severity("Fatal crash in process")
    assert critical == ErrorSeverity.CRITICAL
    
    high = watcher._classify_severity("Error: failed to execute")
    assert high == ErrorSeverity.HIGH
    
    medium = watcher._classify_severity("Warning: deprecated API")
    assert medium == ErrorSeverity.MEDIUM


@pytest.mark.asyncio
async def test_pattern_matching():
    """Test 4: Pattern matching for known errors"""
    watcher, _, _, _ = create_watcher()
    
    errors_with_patterns = [
        "ERROR: Connection refused",
        "FAIL: Import error occurred",
        "WARN: Database error",
    ]
    
    detected_errors = []
    for error_text in errors_with_patterns:
        errors = await watcher.capture_stderr_output(error_text)
        detected_errors.extend(errors)
    
    assert len(detected_errors) == 3


@pytest.mark.asyncio
async def test_graphrag_queries():
    """Test 5: Query GraphRAG for similar errors"""
    watcher, _, graphrag, _ = create_watcher()
    
    # Mock graph response
    graphrag.query_cross_language_errors.return_value = [
        {
            "error": "ConnectionError",
            "solution": "Check network connection",
            "language": "python",
            "confidence": 0.95,
            "count": 3,
        },
    ]
    
    error = DetectedError(
        error_text="ConnectionError: connection refused",
        error_type="ConnectionError",
        severity=ErrorSeverity.HIGH,
        timestamp=time.time(),
        source="stderr",
        context={},
    )
    
    matches = await watcher.query_similar_errors(error)
    
    assert len(matches) == 1
    assert matches[0].solution == "Check network connection"
    assert watcher.stats["patterns_matched"] == 1


@pytest.mark.asyncio
async def test_notification_logic():
    """Test 6: Notification generation"""
    watcher, _, _, _ = create_watcher()
    
    error = DetectedError(
        error_text="SyntaxError: invalid syntax",
        error_type="SyntaxError",
        severity=ErrorSeverity.HIGH,
        timestamp=time.time(),
        source="stderr",
        context={},
    )
    
    solutions = [
        KnownErrorMatch(
            error_pattern="SyntaxError",
            solution="Check indentation",
            language="python",
            confidence=0.98,
            occurrence_count=5,
        ),
    ]
    
    result = await watcher.notify_user(error, solutions)
    
    assert result is True
    assert watcher.stats["notifications_sent"] == 1
    assert watcher.stats["solutions_found"] == 1


@pytest.mark.asyncio
async def test_new_error_persistence():
    """Test 7: Persist new errors to knowledge graph"""
    watcher, _, _, extractor = create_watcher()
    
    error = DetectedError(
        error_text="ValueError: invalid literal",
        error_type="ValueError",
        severity=ErrorSeverity.MEDIUM,
        timestamp=time.time(),
        source="stderr",
        context={},
    )
    
    result = await watcher.extract_and_learn_error(error)
    
    assert result is True
    assert extractor.extract_and_persist_learnings.called


@pytest.mark.asyncio
async def test_background_loop_management():
    """Test 8: Background loop start/stop"""
    watcher, vision_service, _, _ = create_watcher()
    vision_service.capture_active_window.return_value = "No errors"
    
    # Start monitoring
    await watcher.start_monitoring()
    assert watcher.is_running is True
    
    # Give it a moment to run
    await asyncio.sleep(0.1)
    
    # Stop monitoring
    await watcher.stop_monitoring()
    assert watcher.is_running is False


@pytest.mark.asyncio
async def test_stats_tracking():
    """Test stats tracking"""
    watcher, _, _, _ = create_watcher()
    
    output = """
SyntaxError: invalid syntax
ConnectionError: failed to connect
"""
    
    await watcher.capture_stderr_output(output)
    
    stats = watcher.get_stats()
    assert stats["errors_detected"] >= 2


@pytest.mark.asyncio
async def test_detected_error_dataclass():
    """Test DetectedError dataclass"""
    error = DetectedError(
        error_text="Test error",
        error_type="TestError",
        severity=ErrorSeverity.MEDIUM,
        timestamp=time.time(),
        source="stderr",
        context={"test": "context"},
    )
    
    assert error.error_text == "Test error"
    assert error.severity == ErrorSeverity.MEDIUM


@pytest.mark.asyncio
async def test_known_error_match_dataclass():
    """Test KnownErrorMatch dataclass"""
    match = KnownErrorMatch(
        error_pattern="SyntaxError",
        solution="Fix indentation",
        language="python",
        confidence=0.95,
        occurrence_count=10,
    )
    
    assert match.error_pattern == "SyntaxError"
    assert match.confidence == 0.95


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
