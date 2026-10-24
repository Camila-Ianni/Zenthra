"""
PHASE 6 - INTEGRATION TESTS
End-to-end tests for three-pillar architecture integration
"""

import pytest
import tempfile
import os
from pathlib import Path

from core.swarm_orchestrator import SwarmOrchestrator, AgentRole
from core.knowledge_graph import KnowledgeGraph, NodeType
from core.kimi_supervisor import KimiSupervisor, TaskType
from actions.antigravity_skills import AntigravitySkills, SkillCategory


class TestPhase6Integration:
    """Integration tests for Phase 6 three-pillar architecture"""
    
    def test_all_pillars_initialized(self):
        """Test that all three pillars can be initialized"""
        orchestrator = SwarmOrchestrator()
        graph = KnowledgeGraph()
        kimi = KimiSupervisor()
        skills = AntigravitySkills()
        
        assert orchestrator is not None
        assert graph is not None
        assert kimi is not None
        assert skills is not None
    
    def test_openmythos_with_knowledge_graph(self):
        """Test OpenMythos integration with knowledge graph"""
        orchestrator = SwarmOrchestrator()
        graph = KnowledgeGraph()
        
        node = graph.add_node(
            label="Task Decomposition",
            node_type=NodeType.PATTERN,
            description="Breaking down tasks into subtasks",
            content={"complexity": "high"},
        )
        
        assert node.id is not None
        retrieved = graph.get_node(node.id)
        assert retrieved is not None
        assert retrieved.label == "Task Decomposition"
    
    def test_kimi_with_antigravity_context(self):
        """Test Kimi routing with system context"""
        kimi = KimiSupervisor()
        skills = AntigravitySkills()
        
        task_context = {
            "files": [{"size": 100000}] * 20,
            "description": "Large system project",
        }
        
        decision = kimi.route_task(TaskType.ARCHITECTURE, task_context)
        
        assert decision.recommended_model is not None
        assert decision.task_type == TaskType.ARCHITECTURE
    
    def test_cross_pillar_workflow(self):
        """Test workflow using all three pillars"""
        with tempfile.TemporaryDirectory() as tmpdir:
            orchestrator = SwarmOrchestrator()
            graph = KnowledgeGraph(storage_path=os.path.join(tmpdir, "kg.json"))
            kimi = KimiSupervisor(blackboard_path=os.path.join(tmpdir, "bb.json"))
            skills = AntigravitySkills()
            
            node1 = graph.add_node(
                label="Architecture Decision",
                node_type=NodeType.ARCHITECTURE,
            )
            
            node2 = graph.add_node(
                label="Implementation Pattern",
                node_type=NodeType.PATTERN,
            )
            
            from core.knowledge_graph import RelationType
            graph.add_relation(
                node1.id, node2.id,
                relation_type=RelationType.IMPLEMENTS
            )
            
            kimi.save_session_summary(
                session_id="test-1",
                project_name="TestApp",
                token_count=100000,
                architecture_notes={"pattern": "MVC"},
                key_decisions=["Use async"],
                patterns=["Singleton"],
                technical_debt=["Legacy code"],
                recommendations=["Add tests"],
                critical_files=["main.py"],
                dependencies={"flask": "2.0"},
            )
            
            assert len(graph.nodes) == 2
            assert len(kimi.sessions) == 1
    
    def test_backward_compatibility_phase5(self):
        """Test backward compatibility with Phase 5"""
        try:
            from core.model_router import ModelRouter
            from core.orchestrator import Orchestrator
            from core.language_experts import LanguageExpertise
            from core.linter_agent import LinterAgent
            from core.rag_local import RAGLocal
            
            assert ModelRouter is not None
            assert Orchestrator is not None
            assert LanguageExpertise is not None
            assert LinterAgent is not None
            assert RAGLocal is not None
        except ImportError as e:
            pytest.fail(f"Phase 5 backward compatibility broken: {e}")
    
    def test_skill_categories_match_antigravity(self):
        """Test that Antigravity skills are properly categorized"""
        skills = AntigravitySkills()
        
        system_count = len(skills.get_skills_by_category(SkillCategory.SYSTEM_CONTROL))
        media_count = len(skills.get_skills_by_category(SkillCategory.MEDIA_CONTROL))
        browser_count = len(skills.get_skills_by_category(SkillCategory.BROWSER_AUTOMATION))
        
        assert system_count >= 6
        assert media_count >= 5
        assert browser_count >= 2
    
    def test_knowledge_graph_persistence(self):
        """Test knowledge graph persistence across instances"""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "test_kg.json")
            
            graph1 = KnowledgeGraph(storage_path=path)
            node = graph1.add_node(
                label="Test Node",
                node_type=NodeType.DISCOVERY,
            )
            graph1.save_to_storage()
            
            graph2 = KnowledgeGraph(storage_path=path)
            
            assert len(graph2.nodes) == 1
            assert list(graph2.nodes.values())[0].label == "Test Node"
    
    def test_kimi_session_persistence(self):
        """Test Kimi session persistence across instances"""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "test_bb.json")
            
            kimi1 = KimiSupervisor(blackboard_path=path)
            kimi1.save_session_summary(
                session_id="test-sess",
                project_name="PersistTest",
                token_count=50000,
                architecture_notes={"test": True},
                key_decisions=[],
                patterns=[],
                technical_debt=[],
                recommendations=[],
                critical_files=[],
                dependencies={},
            )
            
            kimi2 = KimiSupervisor(blackboard_path=path)
            
            assert len(kimi2.sessions) == 1
            assert kimi2.sessions[0].project_name == "PersistTest"
    
    def test_swarm_orchestrator_stats(self):
        """Test swarm orchestrator statistics"""
        orchestrator = SwarmOrchestrator()
        
        stats = {
            "agents_available": len(orchestrator.agents),
            "max_concurrent": orchestrator.max_concurrent_agents,
            "history_entries": len(orchestrator.execution_history),
        }
        
        assert stats["max_concurrent"] == 5
    
    def test_kimi_routing_for_different_tasks(self):
        """Test Kimi routing decisions for different task types"""
        kimi = KimiSupervisor()
        context = {"description": "test"}
        
        tasks = [
            TaskType.READ_MASSIVE,
            TaskType.WRITE_CODE,
            TaskType.ARCHITECTURE,
            TaskType.DEBUG,
            TaskType.REVIEW,
            TaskType.REFACTOR,
        ]
        
        for task_type in tasks:
            decision = kimi.route_task(task_type, context)
            assert decision.task_type == task_type
            assert decision.recommended_model is not None


class TestPhase6Metrics:
    """Tests for Phase 6 metrics and monitoring"""
    
    def test_knowledge_graph_stats(self):
        """Test knowledge graph statistics"""
        graph = KnowledgeGraph()
        
        for node_type in [NodeType.PATTERN, NodeType.ARCHITECTURE, NodeType.ERROR]:
            graph.add_node(
                label=f"Test {node_type.value}",
                node_type=node_type,
            )
        
        stats = graph.get_stats()
        
        assert stats["total_nodes"] == 3
        assert stats["total_relations"] == 0
    
    def test_kimi_stats(self):
        """Test Kimi supervisor statistics"""
        with tempfile.TemporaryDirectory() as tmpdir:
            kimi = KimiSupervisor(blackboard_path=os.path.join(tmpdir, "bb.json"))
            
            for i in range(3):
                kimi.save_session_summary(
                    session_id=f"sess-{i}",
                    project_name="TestProject",
                    token_count=50000,
                    architecture_notes={},
                    key_decisions=["Decision1", "Decision2"],
                    patterns=["Pattern1"],
                    technical_debt=[],
                    recommendations=[],
                    critical_files=[],
                    dependencies={},
                )
            
            stats = kimi.get_stats()
            
            assert stats["total_sessions"] == 3
            assert stats["unique_projects"] == 1
            assert stats["total_decisions_tracked"] == 6


class TestPhase6Documentation:
    """Tests for Phase 6 documentation and examples"""
    
    def test_all_files_exist(self):
        """Test that all Phase 6 files are created"""
        files_to_check = [
            "core/swarm_orchestrator.py",
            "core/knowledge_graph.py",
            "core/kimi_supervisor.py",
            "actions/antigravity_skills.py",
            "examples/phase6_pillar1_test.py",
            "examples/phase6_pillar2_test.py",
            "examples/phase6_pillar3_test.py",
        ]
        
        base_path = Path(__file__).parent.parent
        
        for file_path in files_to_check:
            full_path = base_path / file_path
            assert full_path.exists(), f"Missing file: {file_path}"
    
    def test_all_classes_importable(self):
        """Test that all Phase 6 classes are importable"""
        try:
            from core.swarm_orchestrator import SwarmOrchestrator, SwarmAgent, AgentRole
            from core.knowledge_graph import KnowledgeGraph, KnowledgeNode, NodeType
            from core.kimi_supervisor import KimiSupervisor, TaskType, ContextWindow
            from actions.antigravity_skills import AntigravitySkills, SkillCategory
            
            assert SwarmOrchestrator is not None
            assert KnowledgeGraph is not None
            assert KimiSupervisor is not None
            assert AntigravitySkills is not None
        except ImportError as e:
            pytest.fail(f"Import error: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
