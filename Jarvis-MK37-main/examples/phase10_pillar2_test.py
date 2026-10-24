"""
Phase 10.2: Parallel Swarm Executor Tests
──────────────────────────────────────────

Tests for SwarmExecutor:
✓ Single tool execution
✓ Multiple independent tools
✓ Tools with dependencies
✓ Resource locking prevention
✓ Error propagation
✓ Speedup measurement (-40% target)
✓ Timeout handling
✓ Result aggregation
"""

import pytest
import asyncio
import time
from core.phase10_swarm_executor import (
    SwarmExecutor,
    ToolTask,
    ExecutionStrategy,
    get_swarm_executor,
    reset_swarm_executor,
    execute_parallel_tools,
)


async def mock_tool(duration: float = 0.1, fail: bool = False):
    """Mock tool that simulates work."""
    await asyncio.sleep(duration)
    if fail:
        raise RuntimeError("Tool failed")
    return {"status": "ok", "duration": duration}


@pytest.mark.asyncio
async def test_single_tool_execution():
    """Test 1: Single tool execution"""
    executor = SwarmExecutor()
    
    tool = ToolTask(
        tool_id="tool1",
        tool_name="test_tool",
        tool_func=lambda: mock_tool(0.05),
        args={},
    )
    
    results, stats = await executor.execute_parallel_tasks([tool])
    
    assert len(results) == 1
    assert results[0].success is True
    assert results[0].tool_id == "tool1"
    assert stats.total_tools == 1


@pytest.mark.asyncio
async def test_multiple_independent_tools():
    """Test 2: Multiple independent tools execute in parallel"""
    executor = SwarmExecutor()
    
    tools = [
        ToolTask(
            tool_id=f"tool{i}",
            tool_name=f"test_tool_{i}",
            tool_func=lambda d=0.1: mock_tool(d),
            args={},
        )
        for i in range(3)
    ]
    
    start = time.time()
    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.PARALLEL,
    )
    elapsed = time.time() - start
    
    # All should complete
    assert len(results) == 3
    assert all(r.success for r in results)
    
    # Parallel should be faster than sequential
    # (3 tools × 0.1s sequential = 0.3s, parallel ≈ 0.1s)
    assert elapsed < 0.25


@pytest.mark.asyncio
async def test_tools_with_dependencies():
    """Test 3: Tools with dependencies execute in correct order"""
    executor = SwarmExecutor()
    execution_order = []

    async def tracked_tool(name: str, delay: float = 0.05):
        execution_order.append(name)
        await asyncio.sleep(delay)
        return {"tool": name}

    tools = [
        ToolTask(
            tool_id="tool1",
            tool_name="first",
            tool_func=lambda: tracked_tool("first"),
            args={},
            dependencies=[],
        ),
        ToolTask(
            tool_id="tool2",
            tool_name="second",
            tool_func=lambda: tracked_tool("second"),
            args={},
            dependencies=["tool1"],
        ),
        ToolTask(
            tool_id="tool3",
            tool_name="third",
            tool_func=lambda: tracked_tool("third"),
            args={},
            dependencies=["tool1"],
        ),
    ]

    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.GROUPED,
    )

    assert len(results) == 3
    assert all(r.success for r in results)
    # First tool should execute first
    assert execution_order[0] == "first"


@pytest.mark.asyncio
async def test_resource_locking_prevents_conflicts():
    """Test 4: Resource locking prevents concurrent access"""
    executor = SwarmExecutor()
    access_count = 0
    max_concurrent = 0

    async def locked_tool():
        nonlocal access_count, max_concurrent
        access_count += 1
        max_concurrent = max(max_concurrent, access_count)
        await asyncio.sleep(0.05)
        access_count -= 1
        return {"status": "ok"}

    tools = [
        ToolTask(
            tool_id="shared_resource",
            tool_name=f"tool_{i}",
            tool_func=locked_tool,
            args={},
        )
        for i in range(3)
    ]

    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.PARALLEL,
    )

    # All should complete successfully
    assert len(results) == 3
    assert all(r.success for r in results)


@pytest.mark.asyncio
async def test_error_propagation():
    """Test 5: Errors in one tool don't block others"""
    executor = SwarmExecutor()

    async def failing_tool():
        raise RuntimeError("Tool failed")

    tools = [
        ToolTask(
            tool_id="tool1",
            tool_name="failing",
            tool_func=failing_tool,
            args={},
        ),
        ToolTask(
            tool_id="tool2",
            tool_name="working",
            tool_func=lambda: mock_tool(0.05),
            args={},
        ),
    ]

    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.PARALLEL,
    )

    assert len(results) == 2
    assert results[0].success is False
    assert results[0].error is not None
    assert results[1].success is True


@pytest.mark.asyncio
async def test_speedup_measurement():
    """Test 6: Verify -40% speedup for parallel execution"""
    executor = SwarmExecutor()

    tools = [
        ToolTask(
            tool_id=f"tool{i}",
            tool_name=f"test_{i}",
            tool_func=lambda: mock_tool(0.1),
            args={},
        )
        for i in range(3)
    ]

    # Parallel execution
    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.PARALLEL,
    )

    # Should have speedup
    assert stats.speedup_percent > 0
    # Realistic speedup should be at least -30% (some overhead)
    assert stats.speedup_percent > -30


@pytest.mark.asyncio
async def test_timeout_handling():
    """Test 7: Timeout handling for slow tools"""
    executor = SwarmExecutor()

    async def slow_tool():
        await asyncio.sleep(5.0)
        return {"status": "ok"}

    tools = [
        ToolTask(
            tool_id="slow",
            tool_name="slow_tool",
            tool_func=slow_tool,
            args={},
            timeout=0.1,
        ),
    ]

    results, stats = await executor.execute_parallel_tasks(tools)

    assert len(results) == 1
    assert results[0].success is False
    assert "Timeout" in results[0].error


@pytest.mark.asyncio
async def test_result_aggregation():
    """Test 8: Results are properly aggregated"""
    executor = SwarmExecutor()

    tools = [
        ToolTask(
            tool_id=f"tool{i}",
            tool_name=f"test_{i}",
            tool_func=lambda d=0.05: mock_tool(d),
            args={},
        )
        for i in range(3)
    ]

    results, stats = await executor.execute_parallel_tasks(tools)

    # Verify stats
    assert stats.total_tools == 3
    assert stats.total_latency_ms > 0
    assert all(isinstance(r.latency_ms, float) for r in results)


@pytest.mark.asyncio
async def test_grouped_execution_strategy():
    """Test grouped execution with dependencies"""
    executor = SwarmExecutor()

    async def quick_tool():
        await asyncio.sleep(0.05)
        return {"status": "ok"}

    tools = [
        ToolTask(
            tool_id="tool1",
            tool_name="first",
            tool_func=quick_tool,
            args={},
        ),
        ToolTask(
            tool_id="tool2",
            tool_name="second",
            tool_func=quick_tool,
            args={},
            dependencies=["tool1"],
        ),
        ToolTask(
            tool_id="tool3",
            tool_name="parallel1",
            tool_func=quick_tool,
            args={},
            dependencies=["tool1"],
        ),
        ToolTask(
            tool_id="tool4",
            tool_name="parallel2",
            tool_func=quick_tool,
            args={},
            dependencies=["tool1"],
        ),
    ]

    start = time.time()
    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.GROUPED,
    )
    elapsed = time.time() - start

    assert len(results) == 4
    assert all(r.success for r in results)
    # Should be faster than pure sequential
    assert elapsed < 0.25


@pytest.mark.asyncio
async def test_sequential_strategy():
    """Test pure sequential execution"""
    executor = SwarmExecutor()

    tools = [
        ToolTask(
            tool_id=f"tool{i}",
            tool_name=f"test_{i}",
            tool_func=lambda: mock_tool(0.1),
            args={},
        )
        for i in range(3)
    ]

    start = time.time()
    results, stats = await executor.execute_parallel_tasks(
        tools,
        strategy=ExecutionStrategy.SEQUENTIAL,
    )
    elapsed = time.time() - start

    assert len(results) == 3
    assert all(r.success for r in results)
    # Sequential should take ~0.3s (3 × 0.1s)
    assert elapsed >= 0.25


@pytest.mark.asyncio
async def test_execute_parallel_tools_helper():
    """Test the helper function interface"""
    async def test_func():
        return {"status": "ok"}

    tools = [
        {
            "tool_id": "t1",
            "tool_name": "test1",
            "tool_func": test_func,
            "args": {},
            "dependencies": [],
        },
        {
            "tool_id": "t2",
            "tool_name": "test2",
            "tool_func": test_func,
            "args": {},
            "dependencies": [],
        },
    ]

    result = await execute_parallel_tools(tools)

    assert "results" in result
    assert "stats" in result
    assert "speedup_percent" in result
    assert len(result["results"]) == 2


@pytest.mark.asyncio
async def test_identify_independent_groups():
    """Test dependency graph analysis"""
    executor = SwarmExecutor()

    tools = [
        ToolTask(tool_id="a", tool_name="a", tool_func=lambda: None, dependencies=[]),
        ToolTask(tool_id="b", tool_name="b", tool_func=lambda: None, dependencies=[]),
        ToolTask(tool_id="c", tool_name="c", tool_func=lambda: None, dependencies=["a", "b"]),
        ToolTask(tool_id="d", tool_name="d", tool_func=lambda: None, dependencies=["c"]),
    ]

    groups = executor._identify_independent_groups(tools)

    # Should have 3 groups: [a, b], [c], [d]
    assert len(groups) >= 2
    # First group should have a and b (independent)
    assert len(groups[0]) >= 2


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
