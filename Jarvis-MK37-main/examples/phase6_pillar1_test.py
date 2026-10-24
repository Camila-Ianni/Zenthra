"""
PHASE 6 - PILLAR 1: OpenMythos Tests
Tests for Swarm Orchestrator and Knowledge Graph
"""

import asyncio
import pytest
import json
import os
from pathlib import Path

from core.swarm_orchestrator import (
    SwarmOrchestrator, SwarmAgent, AgentRole, AgentStatus, AgentTask
)
from core.knowledge_graph import (
    KnowledgeGraph, KnowledgeNode, NodeType, RelationType
)


class TestSwarmOrchestrator:
    """Tests for SwarmOrchestrator"""
    
    def test_orchestrator_initialization(self):
        """Test that orchestrator initializes correctly"""
        orchestrator = SwarmOrchestrator()
        assert orchestrator.max_concurrent_agents == 5
        assert len(orchestrator.agents) == 0
        assert len(orchestrator.execution_history) == 0
    
    def test_agent_pool_creation(self):
        """Test agent pool creation"""
        orchestrator = SwarmOrchestrator()
        
        tasks = [
            AgentTask(role=AgentRole.ANALYZER, description="Analyze this"),
            AgentTask(role=AgentRole.PLANNER, description="Plan this"),
            AgentTask(role=AgentRole.IMPLEMENTER, description="Implement this"),
        ]
        
        agents = orchestrator._create_agent_pool(tasks, execution_handler=None)
        
        assert len(agents) == 3
        assert any(a.role == AgentRole.ANALYZER for a in agents.values())
        assert any(a.role == AgentRole.PLANNER for a in agents.values())
        assert any(a.role == AgentRole.IMPLEMENTER for a in agents.values())
    
    def test_task_decomposition(self):
        """Test task decomposition"""
        orchestrator = SwarmOrchestrator()
        
        context = {"project": "test_project", "language": "python"}
        subtasks = asyncio.run(
            orchestrator._decompose_task(
                "Build a REST API",
                context
            )
        )
        
        assert len(subtasks) >= 3
        assert subtasks[0].role == AgentRole.ANALYZER
        assert subtasks[1].role == AgentRole.PLANNER
        assert subtasks[2].role == AgentRole.IMPLEMENTER
    
    @pytest.mark.asyncio
    async def test_swarm_agent_execution(self):
        """Test individual agent execution"""
        agent = SwarmAgent(
            agent_id="test-analyzer-1",
            role=AgentRole.ANALYZER,
            model_name="gemini-2.0-flash",
        )
        
        task = AgentTask(
            role=AgentRole.ANALYZER,
            description="Analyze this task",
            context={"test": True}
        )
        
        result = await agent.execute(task)
        
        assert result.agent_id == "test-analyzer-1"
        assert result.role == AgentRole.ANALYZER
        assert result.status == AgentStatus.COMPLETED
    
    def test_result_consolidation(self):
        """Test result consolidation"""
        from core.swarm_orchestrator import AgentResult
        
        orchestrator = SwarmOrchestrator()
        
        results = [
            AgentResult(
                agent_id="agent-1",
                role=AgentRole.ANALYZER,
                task_id="task-1",
                status=AgentStatus.COMPLETED,
                output={"analysis": "test"},
                execution_time_ms=100.0,
            ),
            AgentResult(
                agent_id="agent-2",
                role=AgentRole.PLANNER,
                task_id="task-2",
                status=AgentStatus.COMPLETED,
                output={"plan": "test"},
                execution_time_ms=150.0,
            ),
        ]
        
        consolidated = orchestrator._consolidate_results(results, "test task")
        
        assert consolidated.main_output["total_agents"] == 2
        assert consolidated.main_output["successful"] == 2
        assert consolidated.conflicts_resolved >= 0
    
    def test_orchestrator_stats(self):
        """Test orchestrator execution statistics"""
        orchestrator = SwarmOrchestrator()
        
        from core.swarm_orchestrator import AgentResult, SwarmConsolidationResult
        
        results = [
            AgentResult(
                agent_id="agent-1",
                role=AgentRole.ANALYZER,
                task_id="task-1",
                status=AgentStatus.COMPLETED,
                output={},
                execution_time_ms=100.0,
            ),
        ]
        
        consolidated = orchestrator._consolidate_results(results, "task")
        summary = consolidated.main_output["execution_summary"]
        
        assert "total_execution_time_ms" in summary
        assert "average_agent_time_ms" in summary


class TestKnowledgeGraph:
    """Tests for KnowledgeGraph"""
    
    def test_graph_initialization(self):
        """Test knowledge graph initialization"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        assert graph is not None
        assert len(graph.nodes) == 0
        assert len(graph.relations) == 0
    
    def test_add_node(self):
        """Test adding nodes to graph"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        node = graph.add_node(
            label="Test Pattern",
            node_type=NodeType.PATTERN,
            description="A test pattern",
            content={"language": "python"}
        )
        
        assert node.id is not None
        assert node.label == "Test Pattern"
        assert node.node_type == NodeType.PATTERN
        assert len(graph.nodes) == 1
    
    def test_add_relation(self):
        """Test adding relations to graph"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        node1 = graph.add_node(
            label="Pattern A",
            node_type=NodeType.PATTERN,
        )
        
        node2 = graph.add_node(
            label="Pattern B",
            node_type=NodeType.PATTERN,
        )
        
        relation = graph.add_relation(
            source_id=node1.id,
            target_id=node2.id,
            relation_type=RelationType.RELATED_TO,
        )
        
        assert relation is not None
        assert len(graph.relations) == 1
    
    def test_search_by_label(self):
        """Test searching nodes by label"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        graph.add_node(
            label="Test Pattern",
            node_type=NodeType.PATTERN,
        )
        
        results = graph.search_by_label("Test Pattern")
        assert len(results) == 1
        assert results[0].label == "Test Pattern"
    
    def test_search_by_type(self):
        """Test searching nodes by type"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        graph.add_node(
            label="Error 1",
            node_type=NodeType.ERROR,
        )
        graph.add_node(
            label="Pattern 1",
            node_type=NodeType.PATTERN,
        )
        
        error_nodes = graph.search_by_type(NodeType.ERROR)
        pattern_nodes = graph.search_by_type(NodeType.PATTERN)
        
        assert len(error_nodes) == 1
        assert len(pattern_nodes) == 1
    
    def test_get_related_nodes(self):
        """Test getting related nodes"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        node1 = graph.add_node(label="Node1", node_type=NodeType.PATTERN)
        node2 = graph.add_node(label="Node2", node_type=NodeType.PATTERN)
        node3 = graph.add_node(label="Node3", node_type=NodeType.PATTERN)
        
        graph.add_relation(node1.id, node2.id, RelationType.IMPLEMENTS)
        graph.add_relation(node2.id, node3.id, RelationType.USES)
        
        related = graph.get_related_nodes(node1.id, depth=2)
        
        assert len(related) >= 2
    
    def test_find_patterns(self):
        """Test finding common patterns"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        graph.add_node(
            label="Singleton",
            node_type=NodeType.PATTERN,
        )
        graph.add_node(
            label="Singleton",
            node_type=NodeType.PATTERN,
        )
        
        patterns = graph.find_patterns()
        
        assert "Singleton" in patterns
        assert len(patterns["Singleton"]) >= 1
    
    def test_get_architecture_decisions(self):
        """Test retrieving architecture decisions"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        graph.add_node(
            label="Use async/await",
            node_type=NodeType.ARCHITECTURE,
        )
        graph.add_node(
            label="Use microservices",
            node_type=NodeType.ARCHITECTURE,
        )
        
        decisions = graph.get_architecture_decisions()
        
        assert len(decisions) == 2
    
    def test_get_error_history(self):
        """Test retrieving error history"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        graph.add_node(
            label="ImportError in module X",
            node_type=NodeType.ERROR,
            content={"solution": "Fix import path"}
        )
        
        errors = graph.get_error_history()
        
        assert len(errors) == 1
    
    def test_get_stats(self):
        """Test graph statistics"""
        graph = KnowledgeGraph(storage_path="/tmp/test_kg.json")
        
        graph.add_node(label="N1", node_type=NodeType.PATTERN)
        graph.add_node(label="N2", node_type=NodeType.ERROR)
        
        stats = graph.get_stats()
        
        assert stats["total_nodes"] == 2
        assert stats["node_types"]["pattern"] == 1
        assert stats["node_types"]["error"] == 1
    
    def test_persistence(self):
        """Test graph persistence to storage"""
        storage_path = "/tmp/test_kg_persist.json"
        
        graph1 = KnowledgeGraph(storage_path=storage_path)
        node = graph1.add_node(label="Test", node_type=NodeType.PATTERN)
        graph1.save_to_storage()
        
        graph2 = KnowledgeGraph(storage_path=storage_path)
        
        assert len(graph2.nodes) == 1
        assert list(graph2.nodes.values())[0].label == "Test"
        
        if os.path.exists(storage_path):
            os.remove(storage_path)


class TestIntegration:
    """Integration tests"""
    
    def test_full_workflow(self):
        """Test full workflow with orchestrator and knowledge graph"""
        orchestrator = SwarmOrchestrator()
        graph = KnowledgeGraph(storage_path="/tmp/test_full_wf.json")
        
        node1 = graph.add_node(
            label="Requirement",
            node_type=NodeType.DISCOVERY,
        )
        node2 = graph.add_node(
            label="Implementation Plan",
            node_type=NodeType.DECISION,
        )
        
        graph.add_relation(node1.id, node2.id, RelationType.IMPLEMENTS)
        
        assert len(graph.nodes) == 2
        assert len(graph.relations) == 1
        
        stats = graph.get_stats()
        assert stats["total_nodes"] == 2
    
    def test_backward_compatibility(self):
        """Test that Phase 5 (model_router) still works"""
        try:
            from core.model_router import ModelRouter
            from core.orchestrator import Orchestrator
            
            assert ModelRouter is not None
            assert Orchestrator is not None
        except ImportError as e:
            pytest.fail(f"Phase 5 modules not available: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
