"""
PHASE 6 - PILLAR 3: Kimi/Moonshot Tests
Tests for long-term memory supervisor and context management
"""

import pytest
import json
import os
import tempfile
from datetime import datetime

from core.kimi_supervisor import (
    KimiSupervisor, SessionSummary, ContextWindow, RoutingDecision,
    TaskType, ContextPriority, KimiContextBuilder
)


class TestSessionSummary:
    """Tests for SessionSummary dataclass"""
    
    def test_session_summary_creation(self):
        """Test creating a session summary"""
        summary = SessionSummary(
            session_id="sess-1",
            project_name="TestProject",
            timestamp=datetime.now().isoformat(),
            token_count=50000,
            architecture_notes={"pattern": "MVC"},
            key_decisions=["Use async/await"],
            patterns_discovered=["Singleton", "Factory"],
        )
        
        assert summary.session_id == "sess-1"
        assert summary.project_name == "TestProject"
        assert summary.token_count == 50000
    
    def test_session_summary_serialization(self):
        """Test serializing and deserializing session"""
        original = SessionSummary(
            session_id="sess-2",
            project_name="Test",
            timestamp=datetime.now().isoformat(),
            token_count=100000,
        )
        
        data = original.to_dict()
        restored = SessionSummary.from_dict(data)
        
        assert restored.session_id == original.session_id
        assert restored.project_name == original.project_name


class TestContextWindow:
    """Tests for ContextWindow"""
    
    def test_context_window_creation(self):
        """Test creating a context window"""
        window = ContextWindow(
            task_type=TaskType.READ_MASSIVE,
            project_size_tokens=500000,
            available_context_tokens=100000,
        )
        
        assert window.task_type == TaskType.READ_MASSIVE
        assert window.available_context_tokens == 100000
        assert window.current_usage == 0
    
    def test_has_space_check(self):
        """Test checking if context has space"""
        window = ContextWindow(
            task_type=TaskType.WRITE_CODE,
            project_size_tokens=50000,
            available_context_tokens=100000,
        )
        
        assert window.has_space(50000) is True
        assert window.has_space(100001) is False
        
        window.current_usage = 80000
        assert window.has_space(20000) is True
        assert window.has_space(20001) is False
    
    def test_remaining_tokens(self):
        """Test calculating remaining tokens"""
        window = ContextWindow(
            task_type=TaskType.ARCHITECTURE,
            project_size_tokens=100000,
            available_context_tokens=100000,
        )
        
        assert window.remaining_tokens() == 100000
        
        window.current_usage = 30000
        assert window.remaining_tokens() == 70000


class TestKimiSupervisor:
    """Tests for KimiSupervisor"""
    
    def test_supervisor_initialization(self):
        """Test initializing Kimi supervisor"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            
            assert supervisor is not None
            assert supervisor.delegation_threshold == 1_000_000
            assert len(supervisor.sessions) == 0
    
    def test_project_size_detection(self):
        """Test detecting project size"""
        supervisor = KimiSupervisor()
        
        context = {
            "files": [
                {"size": 10000},
                {"size": 5000},
            ],
            "description": "Test project" * 100,
        }
        
        size = supervisor.detect_project_size(context)
        assert size > 0
    
    def test_delegation_threshold(self):
        """Test delegation threshold logic"""
        supervisor = KimiSupervisor(delegation_threshold=50000)
        
        small_context = {"description": "Small"}
        large_context = {"files": [{"size": 100000}] * 10}
        
        assert supervisor.should_delegate_to_kimi(small_context) is False
        assert supervisor.should_delegate_to_kimi(large_context) is True
    
    def test_task_routing(self):
        """Test task routing decisions"""
        supervisor = KimiSupervisor()
        
        context = {"description": "Test"}
        
        decision = supervisor.route_task(TaskType.READ_MASSIVE, context)
        
        assert decision.task_type == TaskType.READ_MASSIVE
        assert decision.recommended_model is not None
        assert isinstance(decision.should_use_kimi, bool)
    
    def test_context_window_creation(self):
        """Test creating context windows"""
        supervisor = KimiSupervisor()
        context = {"description": "Test"}
        
        window_read = supervisor.create_context_window(TaskType.READ_MASSIVE, context)
        window_write = supervisor.create_context_window(TaskType.WRITE_CODE, context)
        
        assert window_read.task_type == TaskType.READ_MASSIVE
        assert window_write.task_type == TaskType.WRITE_CODE
        assert window_read.available_context_tokens > window_write.available_context_tokens
    
    def test_save_and_load_session(self):
        """Test saving and loading session summaries"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            
            summary = supervisor.save_session_summary(
                session_id="sess-1",
                project_name="TestProj",
                token_count=100000,
                architecture_notes={"style": "REST"},
                key_decisions=["Use cache"],
                patterns=["Singleton"],
                technical_debt=["Refactor auth"],
                recommendations=["Add tests"],
                critical_files=["main.py"],
                dependencies={"flask": "2.0"},
            )
            
            assert len(supervisor.sessions) == 1
            
            supervisor2 = KimiSupervisor(blackboard_path=blackboard)
            assert len(supervisor2.sessions) == 1
            assert supervisor2.sessions[0].session_id == "sess-1"
    
    def test_build_context_from_session(self):
        """Test building context from previous session"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            
            supervisor.save_session_summary(
                session_id="sess-1",
                project_name="MyProject",
                token_count=50000,
                architecture_notes={"pattern": "MVC"},
                key_decisions=["Use PostgreSQL"],
                patterns=["Repository"],
                technical_debt=["Logging"],
                recommendations=["Add docs"],
                critical_files=["models.py"],
                dependencies={"sqlalchemy": "1.4"},
            )
            
            window = supervisor.create_context_window(TaskType.WRITE_CODE, {})
            built_context = supervisor.build_context_from_session("MyProject", window)
            
            assert "architecture_notes" in built_context
            assert built_context["key_decisions"] == ["Use PostgreSQL"]
    
    def test_context_reuse_score(self):
        """Test calculating context reuse score"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            
            supervisor.save_session_summary(
                session_id="sess-1",
                project_name="TestProj",
                token_count=50000,
                architecture_notes={},
                key_decisions=[],
                patterns=[],
                technical_debt=[],
                recommendations=[],
                critical_files=[],
                dependencies={},
            )
            
            score = supervisor.get_context_reuse_score("TestProj")
            assert 0.0 <= score <= 1.0
            assert score >= 0.8
    
    def test_get_stats(self):
        """Test getting supervisor statistics"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            
            supervisor.save_session_summary(
                session_id="sess-1",
                project_name="Proj1",
                token_count=50000,
                architecture_notes={},
                key_decisions=["Decision1", "Decision2"],
                patterns=["Pattern1"],
                technical_debt=[],
                recommendations=[],
                critical_files=[],
                dependencies={},
            )
            
            supervisor.save_session_summary(
                session_id="sess-2",
                project_name="Proj2",
                token_count=60000,
                architecture_notes={},
                key_decisions=[],
                patterns=[],
                technical_debt=[],
                recommendations=[],
                critical_files=[],
                dependencies={},
            )
            
            stats = supervisor.get_stats()
            
            assert stats["total_sessions"] == 2
            assert stats["unique_projects"] == 2
            assert stats["total_decisions_tracked"] == 2


class TestRoutingDecision:
    """Tests for RoutingDecision"""
    
    def test_routing_decision_creation(self):
        """Test creating routing decision"""
        decision = RoutingDecision(
            task_type=TaskType.ARCHITECTURE,
            recommended_model="glm-5",
            should_use_kimi=True,
            context_size=1500000,
            kimi_reason="Large project",
        )
        
        assert decision.task_type == TaskType.ARCHITECTURE
        assert decision.recommended_model == "glm-5"
        assert decision.should_use_kimi is True
    
    def test_routing_decision_serialization(self):
        """Test serializing routing decision"""
        decision = RoutingDecision(
            task_type=TaskType.READ_MASSIVE,
            recommended_model="glm-5",
            should_use_kimi=True,
            context_size=2000000,
        )
        
        data = decision.to_dict()
        
        assert data["recommended_model"] == "glm-5"
        assert data["context_size"] == 2000000


class TestKimiContextBuilder:
    """Tests for KimiContextBuilder"""
    
    def test_context_builder_creation(self):
        """Test creating context builder"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            window = supervisor.create_context_window(TaskType.WRITE_CODE, {})
            
            builder = KimiContextBuilder(supervisor, window)
            assert builder is not None
    
    def test_add_context_with_space(self):
        """Test adding context when space available"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            window = supervisor.create_context_window(TaskType.READ_MASSIVE, {})
            
            builder = KimiContextBuilder(supervisor, window)
            
            result = builder.add_context(
                "architecture",
                "MVC pattern",
                ContextPriority.HIGH
            )
            
            assert result is True
            assert window.current_usage > 0
    
    def test_add_context_without_space(self):
        """Test adding context when space unavailable"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            window = supervisor.create_context_window(TaskType.WRITE_CODE, {})
            window.current_usage = window.available_context_tokens
            
            builder = KimiContextBuilder(supervisor, window)
            
            result = builder.add_context("test", "data")
            
            assert result is False
    
    def test_context_builder_build(self):
        """Test building final context"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            window = supervisor.create_context_window(TaskType.READ_MASSIVE, {})
            
            builder = KimiContextBuilder(supervisor, window)
            
            builder.add_context("critical", "critical data", ContextPriority.CRITICAL)
            builder.add_context("high", "high data", ContextPriority.HIGH)
            builder.add_context("low", "low data", ContextPriority.LOW)
            
            context = builder.build()
            
            assert "parts" in context
            assert "total_tokens" in context
            assert "parts_by_priority" in context


class TestTaskTypes:
    """Tests for task type routing"""
    
    def test_task_type_values(self):
        """Test all task type values"""
        assert TaskType.READ_MASSIVE.value == "read_massive"
        assert TaskType.WRITE_CODE.value == "write_code"
        assert TaskType.ARCHITECTURE.value == "architecture"
        assert TaskType.DEBUG.value == "debug"
        assert TaskType.REVIEW.value == "review"
        assert TaskType.REFACTOR.value == "refactor"
    
    def test_task_routing_all_types(self):
        """Test routing for all task types"""
        supervisor = KimiSupervisor()
        context = {"description": "Test"}
        
        for task_type in TaskType:
            decision = supervisor.route_task(task_type, context)
            assert decision.recommended_model is not None


class TestIntegration:
    """Integration tests"""
    
    def test_full_workflow(self):
        """Test complete Kimi workflow"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            supervisor = KimiSupervisor(blackboard_path=blackboard)
            
            project_context = {
                "files": [{"size": 10000}] * 50,
                "description": "Large backend service",
            }
            
            large = supervisor.should_delegate_to_kimi(project_context)
            
            if large:
                decision = supervisor.route_task(TaskType.READ_MASSIVE, project_context)
                window = supervisor.create_context_window(TaskType.READ_MASSIVE, project_context)
                
                assert decision.recommended_model is not None
                assert window.available_context_tokens > 0
    
    def test_cross_session_learning(self):
        """Test learning across multiple sessions"""
        with tempfile.TemporaryDirectory() as tmpdir:
            blackboard = os.path.join(tmpdir, "blackboard.json")
            
            supervisor1 = KimiSupervisor(blackboard_path=blackboard)
            supervisor1.save_session_summary(
                session_id="session-1",
                project_name="MyApp",
                token_count=100000,
                architecture_notes={"style": "microservices"},
                key_decisions=["Use Docker"],
                patterns=["Circuit Breaker"],
                technical_debt=["Add logging"],
                recommendations=["Use k8s"],
                critical_files=["docker-compose.yml"],
                dependencies={"docker": "latest"},
            )
            
            supervisor2 = KimiSupervisor(blackboard_path=blackboard)
            reuse_score = supervisor2.get_context_reuse_score("MyApp")
            
            assert reuse_score > 0.5
            assert len(supervisor2.sessions) == 1


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
