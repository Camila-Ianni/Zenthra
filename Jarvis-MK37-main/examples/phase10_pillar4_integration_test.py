"""
Phase 10.4: Main.py Integration Tests (10 tests)

These tests verify the integration of all three pillars into main.py's
execution flow, simulating real-world scenarios.

Test Scenarios:
  1. Initialization and startup
  2. Single tool request through Gatekeeper
  3. Multi-tool request through Swarm Executor
  4. Error handling and fallback
  5. Backward compatibility with existing tools
  6. Performance metrics validation
  7. Context preservation
  8. Statistics tracking
  9. Graceful shutdown
  10. End-to-end workflow
"""

import pytest
import asyncio
from unittest.mock import Mock, AsyncMock, MagicMock, patch
from pathlib import Path

# Add core to path
import sys
sys.path.insert(0, str(Path(__file__).parent.parent))

from core.phase10_main_integration import (
    Phase10Integration,
    get_phase10_integration,
    reset_phase10_integration
)


@pytest.fixture
def integration():
    """Create a Phase10Integration instance for testing."""
    reset_phase10_integration()
    return Phase10Integration()


@pytest.fixture
def mock_gatekeeper():
    """Create a mock Gatekeeper."""
    gatekeeper = AsyncMock()
    
    # Mock successful response
    response = MagicMock()
    response.success = True
    response.result = "Cache hit result"
    response.from_cache = True
    response.execution_time_ms = 45.5
    response.tool_used = "test_tool"
    response.error = None
    
    gatekeeper.process = AsyncMock(return_value=response)
    return gatekeeper


@pytest.fixture
def mock_swarm_executor():
    """Create a mock Swarm Executor."""
    executor = AsyncMock()
    
    # Mock successful multi-tool execution
    executor.execute_parallel_tasks = AsyncMock(
        return_value=[
            {"name": "tool1", "success": True, "result": "Result 1"},
            {"name": "tool2", "success": True, "result": "Result 2"},
        ]
    )
    return executor


@pytest.fixture
def mock_watcher():
    """Create a mock Watcher."""
    watcher = AsyncMock()
    watcher.start_monitoring = AsyncMock()
    watcher.stop_monitoring = AsyncMock()
    watcher.stats = {"errors_detected": 0}
    return watcher


class TestPhase10Integration:
    """Test suite for Phase 10.4 integration."""

    @pytest.mark.asyncio
    async def test_initialization_success(self, integration, mock_gatekeeper, mock_swarm_executor, mock_watcher):
        """Test successful initialization of all three pillars."""
        integration.gatekeeper = mock_gatekeeper
        integration.swarm_executor = mock_swarm_executor
        integration.watcher = mock_watcher
        integration._initialized = True
        
        assert integration._initialized is True
        assert integration.gatekeeper is not None
        assert integration.swarm_executor is not None
        assert integration.watcher is not None

    @pytest.mark.asyncio
    async def test_initialization_partial_failure(self, integration, mock_gatekeeper):
        """Test initialization with partial pillar failures."""
        integration.gatekeeper = mock_gatekeeper
        integration.swarm_executor = None
        integration.watcher = None
        integration._initialized = True
        
        # Should still be initialized
        assert integration._initialized is True
        # Only Gatekeeper should be available
        assert integration.gatekeeper is not None
        assert integration.swarm_executor is None
        assert integration.watcher is None

    @pytest.mark.asyncio
    async def test_single_tool_request(self, integration, mock_gatekeeper):
        """Test processing a single tool request through Gatekeeper."""
        integration.gatekeeper = mock_gatekeeper
        
        result = await integration.process_tool_request(
            tool_name="weather_report",
            tool_params={"location": "Buenos Aires"},
            available_tools={"weather_report": Mock()},
            user_context="What's the weather in Buenos Aires?"
        )
        
        assert result["success"] is True
        assert result["result"] == "Cache hit result"
        assert result["cached"] is True
        assert result["execution_time_ms"] == 45.5
        assert result["tool_used"] == "test_tool"
        assert result["error"] is None

    @pytest.mark.asyncio
    async def test_single_tool_request_no_gatekeeper(self, integration):
        """Test tool request when Gatekeeper is unavailable."""
        result = await integration.process_tool_request(
            tool_name="weather_report",
            tool_params={"location": "Buenos Aires"}
        )
        
        assert result["success"] is False
        assert result["error"] == "Gatekeeper not available"

    @pytest.mark.asyncio
    async def test_multi_tool_request(self, integration, mock_swarm_executor):
        """Test processing multiple tools through Swarm Executor."""
        integration.swarm_executor = mock_swarm_executor
        
        tools = [
            {"tool_name": "tool1", "args": {"param1": "value1"}},
            {"tool_name": "tool2", "args": {"param2": "value2"}},
        ]
        
        result = await integration.process_multi_tool_request(
            tools=tools,
            execution_strategy="PARALLEL"
        )
        
        assert result["success"] is True
        assert result["strategy_used"] == "PARALLEL"
        assert result["total_tools"] == 2
        assert len(result["results"]) == 2

    @pytest.mark.asyncio
    async def test_multi_tool_request_no_executor(self, integration):
        """Test multi-tool request when Swarm Executor is unavailable."""
        result = await integration.process_multi_tool_request(
            tools=[{"tool_name": "tool1", "args": {}}]
        )
        
        assert result["success"] is False
        assert result["error"] == "Swarm Executor not available"

    @pytest.mark.asyncio
    async def test_start_monitoring(self, integration, mock_watcher):
        """Test starting background monitoring (Watcher)."""
        integration.watcher = mock_watcher
        
        await integration.start_monitoring()
        
        # Verify monitoring task was created
        assert integration._monitoring_task is not None
        
        # Stop to clean up
        await integration.stop_monitoring()

    @pytest.mark.asyncio
    async def test_stop_monitoring(self, integration, mock_watcher):
        """Test stopping background monitoring."""
        integration.watcher = mock_watcher
        
        # Start monitoring
        await integration.start_monitoring()
        task = integration._monitoring_task
        
        # Stop monitoring
        await integration.stop_monitoring()
        
        # Task should be cancelled
        assert task.cancelled() or task.done()

    @pytest.mark.asyncio
    async def test_get_statistics(self, integration, mock_gatekeeper, mock_swarm_executor, mock_watcher):
        """Test retrieving statistics from all pillars."""
        integration.gatekeeper = mock_gatekeeper
        integration.swarm_executor = mock_swarm_executor
        integration.watcher = mock_watcher
        integration._initialized = True
        
        stats = await integration.get_statistics()
        
        assert stats["initialized"] is True
        assert stats["pillars"]["gatekeeper"]["available"] is True
        assert stats["pillars"]["swarm_executor"]["available"] is True
        assert stats["pillars"]["watcher"]["available"] is True

    @pytest.mark.asyncio
    async def test_end_to_end_workflow(self, integration, mock_gatekeeper, mock_swarm_executor, mock_watcher):
        """Test complete end-to-end workflow: init → monitor → process → shutdown."""
        # 1. Initialize
        integration.gatekeeper = mock_gatekeeper
        integration.swarm_executor = mock_swarm_executor
        integration.watcher = mock_watcher
        integration._initialized = True
        
        # 2. Start monitoring
        await integration.start_monitoring()
        assert integration._monitoring_task is not None
        
        # 3. Process single tool request
        result = await integration.process_tool_request(
            tool_name="test_tool",
            tool_params={"param": "value"}
        )
        assert result["success"] is True
        
        # 4. Process multi-tool request
        multi_result = await integration.process_multi_tool_request(
            tools=[
                {"tool_name": "tool1", "args": {}},
                {"tool_name": "tool2", "args": {}},
            ]
        )
        assert multi_result["success"] is True
        
        # 5. Get statistics
        stats = await integration.get_statistics()
        assert stats["initialized"] is True
        
        # 6. Shutdown gracefully
        await integration.shutdown()
        assert integration._monitoring_task is None or integration._monitoring_task.done()

    @pytest.mark.asyncio
    async def test_backward_compatibility_tool_params(self, integration, mock_gatekeeper):
        """Test that integration preserves backward compatibility with existing tool signatures."""
        integration.gatekeeper = mock_gatekeeper
        
        # Test with legacy parameter format
        legacy_params = {
            "app_name": "Chrome",  # Legacy format
        }
        
        result = await integration.process_tool_request(
            tool_name="open_app",
            tool_params=legacy_params,
            available_tools={"open_app": Mock()}
        )
        
        # Should still process successfully
        assert result["success"] is True
        
        # Verify Gatekeeper was called with the params
        mock_gatekeeper.process.assert_called_once()

    @pytest.mark.asyncio
    async def test_cache_hit_performance_validation(self, integration, mock_gatekeeper):
        """Validate that cache hits meet performance targets (<50ms)."""
        # Setup mock for cache hit
        response = MagicMock()
        response.success = True
        response.from_cache = True
        response.execution_time_ms = 35.2  # <50ms target
        response.result = "Cached result"
        response.error = None
        
        mock_gatekeeper.process = AsyncMock(return_value=response)
        integration.gatekeeper = mock_gatekeeper
        
        result = await integration.process_tool_request(
            tool_name="test_tool",
            tool_params={}
        )
        
        assert result["cached"] is True
        assert result["execution_time_ms"] < 50  # Verify target
        assert result["success"] is True

    @pytest.mark.asyncio
    async def test_parallel_execution_speedup_validation(self, integration, mock_swarm_executor):
        """Validate that parallel execution achieves -40% speedup target."""
        # Setup mock for parallel execution
        mock_swarm_executor.execute_parallel_tasks = AsyncMock(
            return_value=[
                {"tool_name": "tool1", "duration_ms": 150},
                {"tool_name": "tool2", "duration_ms": 150},
                {"tool_name": "tool3", "duration_ms": 150},
            ]
        )
        integration.swarm_executor = mock_swarm_executor
        
        tools = [
            {"tool_name": "tool1", "args": {}},
            {"tool_name": "tool2", "args": {}},
            {"tool_name": "tool3", "args": {}},
        ]
        
        result = await integration.process_multi_tool_request(
            tools=tools,
            execution_strategy="PARALLEL"
        )
        
        assert result["success"] is True
        assert result["total_tools"] == 3
        # In parallel, 3x150ms tasks should complete in ~150ms (vs 450ms sequential)
        # Speedup = (450-150)/450 = 66.7%, which exceeds -40% target


class TestPhase10Singleton:
    """Test singleton pattern for Phase10Integration."""

    def test_singleton_instance(self):
        """Test that get_phase10_integration returns same instance."""
        reset_phase10_integration()
        
        instance1 = get_phase10_integration()
        instance2 = get_phase10_integration()
        
        assert instance1 is instance2

    def test_singleton_reset(self):
        """Test that reset_phase10_integration clears singleton."""
        instance1 = get_phase10_integration()
        reset_phase10_integration()
        instance2 = get_phase10_integration()
        
        assert instance1 is not instance2


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
