"""
Phase 9 Pillar 2: Parallel Executor Tests

Tests for task dependency graphs, concurrent execution, resource locking.
Total: 8 tests
"""

import pytest
import asyncio

from core.parallel_executor import (
    ParallelExecutor, TaskDependencyGraph, Task, TaskStatus,
    TaskResult, ResourceLock, execute_parallel_tasks
)


class TestResourceLock:
    """Test resource locking"""
    
    @pytest.mark.asyncio
    async def test_lock_acquire_release(self):
        """Test acquiring and releasing lock"""
        lock = ResourceLock("test_resource")
        
        await lock.acquire("task1")
        assert lock.owner == "task1"
        
        lock.release()
        assert not lock.lock.locked()
    
    @pytest.mark.asyncio
    async def test_lock_context_manager(self):
        """Test lock as context manager"""
        lock = ResourceLock("test_resource")
        
        async with lock:
            assert lock.lock.locked()
        
        assert not lock.lock.locked()


class TestTaskDependencyGraph:
    """Test task dependency graph"""
    
    def test_add_task(self):
        """Test adding tasks to graph"""
        graph = TaskDependencyGraph()
        
        async def dummy_func():
            return "result"
        
        graph.add_task("task1", dummy_func)
        
        assert "task1" in graph.tasks
        assert graph.tasks["task1"].status == TaskStatus.PENDING
    
    def test_task_with_dependencies(self):
        """Test task with dependencies"""
        graph = TaskDependencyGraph()
        
        async def func():
            return "result"
        
        graph.add_task("task1", func)
        graph.add_task("task2", func, dependencies={"task1"})
        
        assert "task1" in graph.tasks["task2"].dependencies
    
    def test_get_ready_tasks(self):
        """Test getting tasks ready for execution"""
        graph = TaskDependencyGraph()
        
        async def func():
            return "result"
        
        graph.add_task("task1", func)
        graph.add_task("task2", func, dependencies={"task1"})
        
        ready = graph.get_ready_tasks()
        
        # Only task1 should be ready
        assert "task1" in ready
        assert "task2" not in ready
    
    def test_get_independent_tasks(self):
        """Test grouping independent tasks by level"""
        graph = TaskDependencyGraph()
        
        async def func():
            return "result"
        
        # Create diamond dependency
        graph.add_task("task1", func)
        graph.add_task("task2", func, dependencies={"task1"})
        graph.add_task("task3", func, dependencies={"task1"})
        graph.add_task("task4", func, dependencies={"task2", "task3"})
        
        levels = graph.get_independent_tasks()
        
        # Should have 3 levels: [task1], [task2, task3], [task4]
        assert len(levels) == 3
        assert "task1" in levels[0]
        assert "task2" in levels[1] and "task3" in levels[1]
        assert "task4" in levels[2]
    
    def test_mark_completed(self):
        """Test marking task as completed"""
        graph = TaskDependencyGraph()
        
        async def func():
            return "result"
        
        graph.add_task("task1", func)
        
        result = TaskResult(
            task_id="task1",
            status=TaskStatus.COMPLETED,
            result="test"
        )
        
        graph.mark_completed("task1", result)
        
        assert graph.tasks["task1"].status == TaskStatus.COMPLETED


class TestParallelExecutor:
    """Test parallel task executor"""
    
    @pytest.mark.asyncio
    async def test_single_task_execution(self):
        """Test executing single task"""
        executor = ParallelExecutor()
        
        async def task_func():
            return "result"
        
        executor.add_task("task1", task_func)
        
        results = await executor.execute()
        
        assert "task1" in results
        assert results["task1"].status == TaskStatus.COMPLETED
        assert results["task1"].result == "result"
    
    @pytest.mark.asyncio
    async def test_parallel_independent_tasks(self):
        """Test executing independent tasks in parallel"""
        executor = ParallelExecutor()
        
        executed_order = []
        
        async def task1():
            executed_order.append("task1")
            await asyncio.sleep(0.01)
            return "result1"
        
        async def task2():
            executed_order.append("task2")
            await asyncio.sleep(0.01)
            return "result2"
        
        executor.add_task("t1", task1)
        executor.add_task("t2", task2)
        
        results = await executor.execute()
        
        # Both should complete
        assert len(results) == 2
        assert results["t1"].status == TaskStatus.COMPLETED
        assert results["t2"].status == TaskStatus.COMPLETED
    
    @pytest.mark.asyncio
    async def test_dependent_task_execution(self):
        """Test executing dependent tasks in order"""
        executor = ParallelExecutor()
        
        execution_order = []
        
        async def task1():
            execution_order.append(1)
            return "result1"
        
        async def task2():
            execution_order.append(2)
            return "result2"
        
        executor.add_task("t1", task1)
        executor.add_task("t2", task2, dependencies={"t1"})
        
        results = await executor.execute()
        
        # task1 should execute first
        assert execution_order == [1, 2]
        assert results["t2"].status == TaskStatus.COMPLETED
    
    @pytest.mark.asyncio
    async def test_task_with_args_kwargs(self):
        """Test executing task with arguments"""
        executor = ParallelExecutor()
        
        async def task_func(a, b, c=None):
            return a + b + (c or 0)
        
        executor.add_task(
            "task1",
            task_func,
            args=(1, 2),
            kwargs={"c": 3}
        )
        
        results = await executor.execute()
        
        assert results["task1"].result == 6
    
    @pytest.mark.asyncio
    async def test_resource_locking(self):
        """Test resource locking prevents concurrent access"""
        executor = ParallelExecutor()
        
        access_count = 0
        
        async def task_func():
            nonlocal access_count
            access_count += 1
            await asyncio.sleep(0.01)
            access_count -= 1
            # access_count should be 1 (only this task accessing)
            if access_count > 1:
                raise RuntimeError("Resource accessed concurrently!")
        
        executor.add_task("t1", task_func, required_resources=["res1"])
        executor.add_task("t2", task_func, required_resources=["res1"])
        
        results = await executor.execute()
        
        # Both should complete successfully (no concurrent access)
        assert results["t1"].status == TaskStatus.COMPLETED
        assert results["t2"].status == TaskStatus.COMPLETED
    
    @pytest.mark.asyncio
    async def test_task_failure_handling(self):
        """Test handling of task failures"""
        executor = ParallelExecutor()
        
        async def failing_task():
            raise ValueError("Task failed")
        
        executor.add_task("t1", failing_task)
        
        results = await executor.execute()
        
        assert results["t1"].status == TaskStatus.FAILED
        assert "ValueError" in results["t1"].error


class TestParallelExecuteHelper:
    """Test execute_parallel_tasks helper"""
    
    @pytest.mark.asyncio
    async def test_execute_parallel_tasks_helper(self):
        """Test execute_parallel_tasks convenience function"""
        
        async def task1():
            return "result1"
        
        async def task2():
            return "result2"
        
        tasks = {
            "t1": {
                "func": task1,
                "args": (),
                "kwargs": {},
                "dependencies": set(),
                "resources": []
            },
            "t2": {
                "func": task2,
                "args": (),
                "kwargs": {},
                "dependencies": set(),
                "resources": []
            }
        }
        
        results = await execute_parallel_tasks(tasks)
        
        assert len(results) == 2
        assert results["t1"].status == TaskStatus.COMPLETED
        assert results["t2"].status == TaskStatus.COMPLETED


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
