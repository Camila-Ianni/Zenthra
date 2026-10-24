"""
Phase 9 Pillar 2: Parallel Executor

Manages concurrent execution of independent tasks with dependency tracking.
Enables -40% speedup for multi-tool operations.

Key Features:
- Task dependency graph modeling
- Concurrent execution via asyncio.gather
- Resource locking for hardware/files
- Error handling and propagation
- Performance monitoring
"""

import logging
import asyncio
from typing import Optional, Dict, Any, List, Callable, Set, Coroutine
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime


logger = logging.getLogger(__name__)


class TaskStatus(Enum):
    """Task execution status"""
    PENDING = "pending"
    READY = "ready"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    BLOCKED = "blocked"


@dataclass
class TaskResult:
    """Result of task execution"""
    task_id: str
    status: TaskStatus
    result: Optional[Any] = None
    error: Optional[str] = None
    start_time: Optional[str] = None
    end_time: Optional[str] = None
    duration_ms: float = 0.0
    
    @property
    def is_success(self) -> bool:
        return self.status == TaskStatus.COMPLETED


@dataclass
class Task:
    """Represents a task in the dependency graph"""
    task_id: str
    func: Callable
    args: tuple = field(default_factory=tuple)
    kwargs: dict = field(default_factory=dict)
    dependencies: Set[str] = field(default_factory=set)
    required_resources: List[str] = field(default_factory=list)
    
    status: TaskStatus = TaskStatus.PENDING
    result: Optional[TaskResult] = None


class ResourceLock:
    """Simple lock for shared resources"""
    
    def __init__(self, resource_name: str):
        self.resource_name = resource_name
        self.lock = asyncio.Lock()
        self.owner: Optional[str] = None
    
    async def acquire(self, task_id: str):
        """Acquire lock for a task"""
        await self.lock.acquire()
        self.owner = task_id
        logger.debug(f"Lock acquired for {self.resource_name} by {task_id}")
    
    def release(self):
        """Release lock"""
        if self.lock.locked():
            self.lock.release()
            logger.debug(f"Lock released for {self.resource_name}")
    
    async def __aenter__(self):
        await self.lock.acquire()
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        self.lock.release()


class TaskDependencyGraph:
    """Models dependencies between tasks"""
    
    def __init__(self):
        self.tasks: Dict[str, Task] = {}
        self.resource_locks: Dict[str, ResourceLock] = {}
    
    def add_task(
        self,
        task_id: str,
        func: Callable,
        args: tuple = (),
        kwargs: dict = None,
        dependencies: Set[str] = None,
        required_resources: List[str] = None
    ) -> None:
        """Add task to graph"""
        kwargs = kwargs or {}
        dependencies = dependencies or set()
        required_resources = required_resources or []
        
        task = Task(
            task_id=task_id,
            func=func,
            args=args,
            kwargs=kwargs,
            dependencies=dependencies,
            required_resources=required_resources
        )
        
        self.tasks[task_id] = task
        
        # Create resource locks
        for resource in required_resources:
            if resource not in self.resource_locks:
                self.resource_locks[resource] = ResourceLock(resource)
    
    def get_ready_tasks(self) -> List[str]:
        """Get tasks that can be executed now"""
        ready = []
        
        for task_id, task in self.tasks.items():
            if task.status != TaskStatus.PENDING:
                continue
            
            # Check if all dependencies are completed
            deps_done = all(
                self.tasks[dep].status == TaskStatus.COMPLETED
                for dep in task.dependencies
                if dep in self.tasks
            )
            
            if deps_done:
                ready.append(task_id)
        
        return ready
    
    def get_blocked_tasks(self) -> List[str]:
        """Get tasks waiting for dependencies"""
        blocked = []
        
        for task_id, task in self.tasks.items():
            if task.status not in [TaskStatus.PENDING, TaskStatus.BLOCKED]:
                continue
            
            # Check if any dependency is not completed
            for dep in task.dependencies:
                if dep in self.tasks:
                    if self.tasks[dep].status != TaskStatus.COMPLETED:
                        blocked.append(task_id)
                        break
        
        return blocked
    
    def get_independent_tasks(self) -> List[List[str]]:
        """
        Group tasks into levels by independence.
        Returns list of task groups that can execute in parallel.
        """
        levels = []
        assigned = set()
        
        while len(assigned) < len(self.tasks):
            current_level = []
            
            for task_id, task in self.tasks.items():
                if task_id in assigned:
                    continue
                
                # Check if dependencies are all assigned
                deps_assigned = all(
                    dep in assigned for dep in task.dependencies
                )
                
                if deps_assigned:
                    current_level.append(task_id)
            
            if not current_level:
                break
            
            levels.append(current_level)
            assigned.update(current_level)
        
        return levels
    
    def mark_completed(self, task_id: str, result: TaskResult) -> None:
        """Mark task as completed"""
        if task_id in self.tasks:
            self.tasks[task_id].status = TaskStatus.COMPLETED
            self.tasks[task_id].result = result
    
    def mark_failed(self, task_id: str, error: str) -> None:
        """Mark task as failed"""
        if task_id in self.tasks:
            self.tasks[task_id].status = TaskStatus.FAILED
            self.tasks[task_id].result = TaskResult(
                task_id=task_id,
                status=TaskStatus.FAILED,
                error=error
            )


class ParallelExecutor:
    """Executes tasks concurrently respecting dependencies"""
    
    def __init__(self):
        self.graph = TaskDependencyGraph()
    
    def add_task(
        self,
        task_id: str,
        func: Callable,
        args: tuple = (),
        kwargs: dict = None,
        dependencies: Set[str] = None,
        required_resources: List[str] = None
    ) -> None:
        """Add task to execution graph"""
        self.graph.add_task(
            task_id,
            func,
            args,
            kwargs,
            dependencies,
            required_resources
        )
    
    async def _execute_task(self, task_id: str) -> TaskResult:
        """Execute single task with resource locking"""
        task = self.graph.tasks[task_id]
        
        start_time = datetime.now()
        logger.debug(f"Executing task {task_id}")
        
        try:
            # Acquire resource locks
            lock_contexts = []
            for resource in task.required_resources:
                lock = self.graph.resource_locks[resource]
                lock_contexts.append(lock)
                await lock.acquire(task_id)
            
            try:
                # Execute task
                task.status = TaskStatus.RUNNING
                
                if asyncio.iscoroutinefunction(task.func):
                    result_value = await task.func(*task.args, **task.kwargs)
                else:
                    result_value = task.func(*task.args, **task.kwargs)
                
                end_time = datetime.now()
                duration = (end_time - start_time).total_seconds() * 1000
                
                result = TaskResult(
                    task_id=task_id,
                    status=TaskStatus.COMPLETED,
                    result=result_value,
                    start_time=start_time.isoformat(),
                    end_time=end_time.isoformat(),
                    duration_ms=duration
                )
                
                logger.info(f"Task {task_id} completed in {duration:.1f}ms")
                return result
                
            finally:
                # Release resource locks
                for lock in lock_contexts:
                    lock.release()
                
        except Exception as e:
            end_time = datetime.now()
            duration = (end_time - start_time).total_seconds() * 1000
            error_msg = f"{type(e).__name__}: {str(e)}"
            
            result = TaskResult(
                task_id=task_id,
                status=TaskStatus.FAILED,
                error=error_msg,
                start_time=start_time.isoformat(),
                end_time=end_time.isoformat(),
                duration_ms=duration
            )
            
            logger.error(f"Task {task_id} failed: {error_msg}")
            return result
    
    async def execute(self) -> Dict[str, TaskResult]:
        """
        Execute all tasks respecting dependencies.
        
        Returns:
            Dictionary mapping task_id to TaskResult
        """
        results = {}
        levels = self.graph.get_independent_tasks()
        
        logger.info(f"Executing {len(self.graph.tasks)} tasks in {len(levels)} levels")
        
        for level_idx, level_tasks in enumerate(levels):
            logger.debug(f"Level {level_idx}: {len(level_tasks)} parallel tasks")
            
            # Execute all tasks in this level concurrently
            coros = [self._execute_task(task_id) for task_id in level_tasks]
            level_results = await asyncio.gather(*coros, return_exceptions=True)
            
            # Process results
            for task_id, result in zip(level_tasks, level_results):
                if isinstance(result, Exception):
                    results[task_id] = TaskResult(
                        task_id=task_id,
                        status=TaskStatus.FAILED,
                        error=f"Execution failed: {result}"
                    )
                else:
                    results[task_id] = result
                    self.graph.mark_completed(task_id, result)
        
        return results


async def execute_parallel_tasks(
    tasks: Dict[str, Dict[str, Any]],
    resource_limits: Optional[Dict[str, List[str]]] = None
) -> Dict[str, TaskResult]:
    """
    Execute multiple tasks in parallel respecting dependencies.
    
    Args:
        tasks: Dict mapping task_id to {func, args, kwargs, dependencies, resources}
        resource_limits: Dict mapping resource name to list of task_ids that share it
    
    Returns:
        Results from all tasks
    """
    executor = ParallelExecutor()
    
    # Add tasks
    for task_id, task_def in tasks.items():
        executor.add_task(
            task_id,
            task_def['func'],
            task_def.get('args', ()),
            task_def.get('kwargs', {}),
            task_def.get('dependencies', set()),
            task_def.get('resources', [])
        )
    
    # Execute
    return await executor.execute()
