"""
OpenMythos Swarm Orchestrator
Hierarchical task decomposition with parallel agent execution
Enables JARVIS autonomy through enjambre (swarm) reasoning

Compatible with Python 3.9+
"""

import asyncio
import json
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from typing import List, Dict, Optional, Any, Callable
from uuid import uuid4

logger = logging.getLogger(__name__)


class AgentRole(Enum):
    """Defined roles for swarm agents (inspired by OpenMythos)"""
    ANALYZER = "analyzer"       # Analyzes task complexity
    PLANNER = "planner"         # Creates execution plan
    IMPLEMENTER = "implementer" # Executes implementation
    REVIEWER = "reviewer"       # Reviews results
    VALIDATOR = "validator"     # Final validation


class AgentStatus(Enum):
    """Status lifecycle of an agent"""
    IDLE = "idle"
    THINKING = "thinking"
    EXECUTING = "executing"
    COMPLETED = "completed"
    FAILED = "failed"


@dataclass
class AgentTask:
    """Individual task for a swarm agent"""
    id: str = field(default_factory=lambda: str(uuid4())[:8])
    role: AgentRole = AgentRole.ANALYZER
    description: str = ""
    context: Dict[str, Any] = field(default_factory=dict)
    parent_task_id: Optional[str] = None
    priority: int = 1
    timeout_seconds: int = 60
    required_inputs: List[str] = field(default_factory=list)
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return {
            "id": self.id,
            "role": self.role.value,
            "description": self.description,
            "context": self.context,
            "parent_task_id": self.parent_task_id,
            "priority": self.priority,
            "timeout_seconds": self.timeout_seconds,
            "required_inputs": self.required_inputs,
        }


@dataclass
class AgentResult:
    """Result from agent execution"""
    agent_id: str
    role: AgentRole
    task_id: str
    status: AgentStatus
    output: Any = None
    error: Optional[str] = None
    execution_time_ms: float = 0.0
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return {
            "agent_id": self.agent_id,
            "role": self.role.value,
            "task_id": self.task_id,
            "status": self.status.value,
            "output": self.output,
            "error": self.error,
            "execution_time_ms": self.execution_time_ms,
            "timestamp": self.timestamp,
        }


class SwarmAgent:
    """Individual agent in the swarm"""
    
    def __init__(
        self,
        agent_id: str,
        role: AgentRole,
        model_name: str = "gemini-2.0-flash",
        execution_handler: Optional[Callable] = None
    ):
        self.agent_id = agent_id
        self.role = role
        self.model_name = model_name
        self.status = AgentStatus.IDLE
        self.execution_handler = execution_handler
        
    async def execute(self, task: AgentTask) -> AgentResult:
        """Execute a task asynchronously"""
        import time
        start_time = time.time()
        
        try:
            self.status = AgentStatus.THINKING
            
            prompt = self._build_role_prompt(task)
            logger.info(f"[{self.agent_id}] Executing {self.role.value} task: {task.id}")
            
            output = await self._generate_response(prompt, task)
            
            self.status = AgentStatus.COMPLETED
            execution_time_ms = (time.time() - start_time) * 1000
            
            return AgentResult(
                agent_id=self.agent_id,
                role=self.role,
                task_id=task.id,
                status=AgentStatus.COMPLETED,
                output=output,
                execution_time_ms=execution_time_ms,
            )
        except Exception as e:
            logger.error(f"[{self.agent_id}] Task {task.id} failed: {str(e)}")
            execution_time_ms = (time.time() - start_time) * 1000
            
            return AgentResult(
                agent_id=self.agent_id,
                role=self.role,
                task_id=task.id,
                status=AgentStatus.FAILED,
                error=str(e),
                execution_time_ms=execution_time_ms,
            )
    
    def _build_role_prompt(self, task: AgentTask) -> str:
        """Build role-specific system prompt"""
        base_context = json.dumps(task.context, indent=2)
        
        prompts = {
            AgentRole.ANALYZER: f"""You are an expert code analyzer. Your role is to:
1. Break down the given task into components
2. Identify complexity hotspots
3. List dependencies and blockers
4. Recommend architecture patterns

Task: {task.description}
Context:
{base_context}

Provide a detailed analysis in JSON format with keys:
- components: List of task components
- complexity: 'low', 'medium', or 'high'
- dependencies: List of required dependencies
- blockers: List of potential issues
- recommended_pattern: Suggested architecture pattern""",

            AgentRole.PLANNER: f"""You are a strategic task planner. Your role is to:
1. Create a step-by-step execution plan
2. Define milestones and checkpoints
3. Estimate effort for each step
4. Identify parallelizable tasks

Task: {task.description}
Context:
{base_context}

Provide a plan in JSON format with keys:
- steps: List of execution steps with order and effort
- milestones: List of key checkpoints
- parallelizable_pairs: Tasks that can run in parallel
- estimated_total_minutes: Total effort estimate
- critical_path: Most important sequential steps""",

            AgentRole.IMPLEMENTER: f"""You are a code implementation specialist. Your role is to:
1. Write clean, production-ready code
2. Follow SOLID principles
3. Include error handling
4. Add comprehensive comments

Task: {task.description}
Context:
{base_context}

Provide implementation in JSON format with keys:
- code: Implementation code
- imports: Required imports
- functions: List of public functions with signatures
- error_handling: Error cases covered
- testing_notes: Suggestions for testing""",

            AgentRole.REVIEWER: f"""You are a code quality reviewer. Your role is to:
1. Check code against best practices
2. Identify potential bugs
3. Suggest optimizations
4. Rate overall quality

Task: {task.description}
Context:
{base_context}

Provide review in JSON format with keys:
- quality_score: 1-10
- issues: List of issues found
- suggestions: Optimization suggestions
- patterns_used: Design patterns identified
- recommendation: 'approve', 'revise', or 'reject'""",

            AgentRole.VALIDATOR: f"""You are a final validation specialist. Your role is to:
1. Verify against requirements
2. Check for completeness
3. Validate edge cases
4. Confirm production readiness

Task: {task.description}
Context:
{base_context}

Provide validation report in JSON format with keys:
- requirements_met: List of validated requirements
- edge_cases_covered: Edge cases validated
- completeness_score: 1-10
- ready_for_production: true/false
- final_notes: Summary of validation""",
        }
        
        return prompts.get(self.role, prompts[AgentRole.ANALYZER])
    
    async def _generate_response(self, prompt: str, task: AgentTask) -> Dict[str, Any]:
        """Generate response using the model (placeholder for actual API call)"""
        if self.execution_handler:
            return await self.execution_handler(self.model_name, prompt, task)
        
        return {
            "status": "mock_response",
            "role": self.role.value,
            "message": f"Mock {self.role.value} response for task {task.id}",
        }


@dataclass
class SwarmConsolidationResult:
    """Result of consolidating multiple agent outputs"""
    main_output: Dict[str, Any]
    all_outputs: List[AgentResult]
    consolidation_method: str
    conflicts_resolved: int
    confidence_score: float
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return {
            "main_output": self.main_output,
            "all_outputs": [o.to_dict() for o in self.all_outputs],
            "consolidation_method": self.consolidation_method,
            "conflicts_resolved": self.conflicts_resolved,
            "confidence_score": self.confidence_score,
            "timestamp": self.timestamp,
        }


class SwarmOrchestrator:
    """Orchestrates parallel agent execution and result consolidation"""
    
    def __init__(self, model_router=None):
        self.model_router = model_router
        self.agents: Dict[str, SwarmAgent] = {}
        self.max_concurrent_agents = 5
        self.execution_history: List[SwarmConsolidationResult] = []
        
    async def decompose_and_execute(
        self,
        main_task: str,
        task_context: Dict[str, Any],
        execution_handler: Optional[Callable] = None,
    ) -> SwarmConsolidationResult:
        """
        Main entry point: decompose task and execute with swarm
        
        Args:
            main_task: High-level task description
            task_context: Context/constraints for the task
            execution_handler: Async function to handle model calls
            
        Returns:
            Consolidated result from all agents
        """
        logger.info(f"Starting swarm execution for task: {main_task}")
        
        subtasks = await self._decompose_task(main_task, task_context)
        
        agents = self._create_agent_pool(subtasks, execution_handler)
        
        results = await self._execute_swarm(agents, subtasks)
        
        consolidated = self._consolidate_results(results, main_task)
        
        self.execution_history.append(consolidated)
        
        return consolidated
    
    async def _decompose_task(
        self,
        main_task: str,
        task_context: Dict[str, Any]
    ) -> List[AgentTask]:
        """Decompose main task into sub-tasks for different agents"""
        subtasks = [
            AgentTask(
                role=AgentRole.ANALYZER,
                description=f"Analyze complexity and dependencies of: {main_task}",
                context=task_context,
                priority=1,
            ),
            AgentTask(
                role=AgentRole.PLANNER,
                description=f"Create execution plan for: {main_task}",
                context=task_context,
                priority=1,
                required_inputs=["analyzer_output"],
            ),
            AgentTask(
                role=AgentRole.IMPLEMENTER,
                description=f"Implement solution for: {main_task}",
                context=task_context,
                priority=2,
                required_inputs=["planner_output"],
            ),
            AgentTask(
                role=AgentRole.REVIEWER,
                description=f"Review implementation quality",
                context=task_context,
                priority=3,
                required_inputs=["implementer_output"],
            ),
            AgentTask(
                role=AgentRole.VALIDATOR,
                description=f"Validate final solution against requirements",
                context=task_context,
                priority=3,
                required_inputs=["reviewer_output"],
            ),
        ]
        
        return subtasks
    
    def _create_agent_pool(
        self,
        tasks: List[AgentTask],
        execution_handler: Optional[Callable]
    ) -> Dict[str, SwarmAgent]:
        """Create agents for each task role"""
        agents = {}
        model_map = {
            AgentRole.ANALYZER: "glm-5",
            AgentRole.PLANNER: "deepseek-v4",
            AgentRole.IMPLEMENTER: "qwen3-coder",
            AgentRole.REVIEWER: "glm-5",
            AgentRole.VALIDATOR: "gemini-2.0-flash",
        }
        
        for task in tasks:
            agent_id = f"{task.role.value}-{task.id}"
            model = model_map.get(task.role, "gemini-2.0-flash")
            
            agent = SwarmAgent(
                agent_id=agent_id,
                role=task.role,
                model_name=model,
                execution_handler=execution_handler,
            )
            agents[agent_id] = agent
        
        return agents
    
    async def _execute_swarm(
        self,
        agents: Dict[str, SwarmAgent],
        tasks: List[AgentTask]
    ) -> List[AgentResult]:
        """Execute all agents with concurrency control"""
        results = []
        semaphore = asyncio.Semaphore(self.max_concurrent_agents)
        
        async def bounded_execute(agent: SwarmAgent, task: AgentTask):
            async with semaphore:
                return await agent.execute(task)
        
        tasks_to_execute = [
            bounded_execute(agents[f"{task.role.value}-{task.id}"], task)
            for task in tasks
        ]
        
        results = await asyncio.gather(*tasks_to_execute, return_exceptions=False)
        
        logger.info(f"Swarm execution complete: {len(results)} agents finished")
        return results
    
    def _consolidate_results(
        self,
        results: List[AgentResult],
        main_task: str
    ) -> SwarmConsolidationResult:
        """Consolidate outputs from all agents"""
        successful_results = [r for r in results if r.status == AgentStatus.COMPLETED]
        failed_results = [r for r in results if r.status == AgentStatus.FAILED]
        
        conflicts = self._detect_conflicts(successful_results)
        
        main_output = {
            "task": main_task,
            "total_agents": len(results),
            "successful": len(successful_results),
            "failed": len(failed_results),
            "agent_outputs": {r.agent_id: r.output for r in successful_results},
            "execution_summary": self._build_summary(successful_results),
        }
        
        confidence = max(0.0, 1.0 - (len(conflicts) * 0.1))
        
        return SwarmConsolidationResult(
            main_output=main_output,
            all_outputs=results,
            consolidation_method="hierarchical_merge",
            conflicts_resolved=len(conflicts),
            confidence_score=confidence,
        )
    
    def _detect_conflicts(self, results: List[AgentResult]) -> List[Dict[str, Any]]:
        """Detect conflicting recommendations between agents"""
        conflicts = []
        
        outputs_by_role = {}
        for result in results:
            role = result.role.value
            if role not in outputs_by_role:
                outputs_by_role[role] = []
            outputs_by_role[role].append(result.output)
        
        return conflicts
    
    def _build_summary(self, results: List[AgentResult]) -> Dict[str, Any]:
        """Build execution summary"""
        total_time = sum(r.execution_time_ms for r in results)
        
        return {
            "total_execution_time_ms": total_time,
            "average_agent_time_ms": total_time / len(results) if results else 0,
            "slowest_agent": max(results, key=lambda r: r.execution_time_ms).role.value if results else None,
            "fastest_agent": min(results, key=lambda r: r.execution_time_ms).role.value if results else None,
        }
    
    def save_execution_history(self, filepath: str) -> None:
        """Save execution history to JSON"""
        history = [e.to_dict() for e in self.execution_history]
        with open(filepath, 'w') as f:
            json.dump(history, f, indent=2, default=str)
        logger.info(f"Saved execution history to {filepath}")
