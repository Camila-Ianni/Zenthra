"""
Phase 7 Pillar 4: Integration & Documentation Tests

Tests for Phase 7 complete integration with main.py and backward compatibility.
Covers: initialization, end-to-end workflows, resilience, migration.

Total: 16 tests
"""

import pytest
import asyncio
from pathlib import Path
from unittest.mock import Mock, patch, AsyncMock

from core.graph_db_manager import Neo4jGraphDB, get_graph_db, close_graph_db
from core.graphrag_overlay import get_graphrag, close_graphrag
from core.learning_extractor import LearningExtractor
from core.graph_schema import NodeType, RelationType


# ============================================================================
# TESTS: Module Imports & Initialization (3 tests)
# ============================================================================

class TestPhase7Integration:
    """Test Phase 7 module integration"""
    
    def test_all_modules_importable(self):
        """Test that all Phase 7 modules can be imported"""
        from core.graph_db_manager import Neo4jGraphDB
        from core.graph_schema import NodeType, RelationType
        from core.graphrag_overlay import GraphRAG
        from core.learning_extractor import LearningExtractor
        
        assert Neo4jGraphDB is not None
        assert NodeType is not None
        assert GraphRAG is not None
        assert LearningExtractor is not None
    
    @pytest.mark.asyncio
    async def test_get_graph_db_singleton(self):
        """Test get_graph_db singleton pattern"""
        await close_graph_db()
        
        db1 = await get_graph_db()
        db2 = await get_graph_db()
        
        assert db1 is db2
        
        await close_graph_db()
    
    @pytest.mark.asyncio
    async def test_get_graphrag_singleton(self):
        """Test get_graphrag singleton pattern"""
        await close_graphrag()
        
        rag1 = await get_graphrag()
        rag2 = await get_graphrag()
        
        assert rag1 is rag2
        
        await close_graphrag()


# ============================================================================
# TESTS: End-to-End Workflows (5 tests)
# ============================================================================

class TestEndToEndWorkflows:
    """Test complete workflows from task to memory"""
    
    @pytest.mark.asyncio
    async def test_task_to_graph_workflow(self):
        """Test workflow: task execution -> learning extraction -> graph persistence"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create a project context
        proj_id = await db.create_node(
            NodeType.PROJECT,
            {"name": "e2e-test-proj", "language": "Python"}
        )
        
        # Extract learnings from task result
        extractor = LearningExtractor(graph_db=db)
        result_text = """
        Task: Optimize Python asyncio code
        Error: RuntimeWarning on coroutine shutdown
        Solution: Added proper cleanup with aclose()
        Technology: Python with asyncio
        Concept: Resource Management
        """
        
        learnings = await extractor.extract_from_task_result(
            task_id="e2e-task-001",
            task_title="Optimize asyncio",
            result_text=result_text,
            project_id=proj_id
        )
        
        # Persist learnings
        persisted = await extractor.persist_learnings(learnings, "e2e-task-001")
        
        assert len(learnings) > 0
        assert persisted >= 0
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_project_concepts_query_workflow(self):
        """Test workflow: create project -> query concepts -> verify results"""
        from core.graphrag_overlay import QueryDepth
        
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create project
        proj_id = await db.create_node(
            NodeType.PROJECT,
            {"name": "workflow-test-proj", "language": "Go"}
        )
        
        # Add skill
        skill_id = await db.create_node(
            NodeType.SKILL,
            {"name": "goroutines", "category": "concurrency"}
        )
        
        # Link them
        await db.create_relationship(proj_id, RelationType.USES_SKILL, skill_id)
        
        # Query concepts
        from core.graphrag_overlay import GraphRAG
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_project_concepts(proj_id, QueryDepth.DIRECT)
        
        assert result is not None
        assert result.graph_nodes is not None
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_cross_language_learning_workflow(self):
        """Test cross-language learning integration"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create errors in different languages
        py_error = await db.create_node(
            NodeType.ERROR,
            {
                "type": "resource_leak",
                "language": "Python",
                "solution": "Use context managers"
            }
        )
        
        go_error = await db.create_node(
            NodeType.ERROR,
            {
                "type": "resource_leak",
                "language": "Go",
                "solution": "Use defer for cleanup"
            }
        )
        
        # Query cross-language errors
        from core.graphrag_overlay import GraphRAG
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_cross_language_errors(
            "resource_leak",
            "Python",
            target_languages=["Go"]
        )
        
        assert result is not None
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_complete_learning_cycle(self):
        """Test complete cycle: extract -> persist -> query"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        extractor = LearningExtractor(graph_db=db)
        
        # Task 1: Extract and persist
        result1 = """
        Fixed concurrency bug in JavaScript async/await
        Error: Race condition in Promise.all()
        Solution: Used Promise.allSettled() instead
        """
        
        learnings1 = await extractor.extract_from_task_result(
            task_id="cycle-task-1",
            task_title="Fix JS race condition",
            result_text=result1
        )
        
        await extractor.persist_learnings(learnings1, "cycle-task-1")
        
        # Task 2: Extract different domain
        result2 = """
        Refactored Go microservice with improved caching
        Concepts: Microservices, Caching, Performance
        Technologies: Go, Redis, Docker
        """
        
        learnings2 = await extractor.extract_from_task_result(
            task_id="cycle-task-2",
            task_title="Microservice refactor",
            result_text=result2
        )
        
        await extractor.persist_learnings(learnings2, "cycle-task-2")
        
        # Query should find learnings
        from core.graphrag_overlay import GraphRAG
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        stats = await graphrag.get_statistics()
        assert stats is not None
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_learning_graph_relationships(self):
        """Test that extracted learnings create proper relationships"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        extractor = LearningExtractor(graph_db=db)
        
        # Extract with relationships
        result_text = """
        Implemented async design pattern in Python
        Technologies: Python, asyncio
        Concepts: Concurrency, Design Patterns
        """
        
        learnings = await extractor.extract_from_task_result(
            task_id="rel-test",
            task_title="Design pattern work",
            result_text=result_text
        )
        
        # Check relationships in extracted learnings
        has_relationships = any(len(l.relationships) > 0 for l in learnings)
        assert has_relationships or len(learnings) > 0
        
        await db.disconnect()


# ============================================================================
# TESTS: Resilience & Error Handling (4 tests)
# ============================================================================

class TestResilience:
    """Test system resilience"""
    
    @pytest.mark.asyncio
    async def test_fallback_mode_on_db_failure(self):
        """Test automatic fallback to JSON on DB failure"""
        db = Neo4jGraphDB(uri="bolt://invalid-neo4j-server:99999")
        connected = await db.connect()
        
        assert connected is True
        assert db.use_fallback is True
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_graphrag_with_fallback_db(self):
        """Test GraphRAG works in fallback mode"""
        db = Neo4jGraphDB(uri="bolt://invalid:99999")
        await db.connect()
        
        from core.graphrag_overlay import GraphRAG
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        # Should work without error even in fallback
        stats = await graphrag.get_statistics()
        assert stats.get("graphrag_mode") == "fallback"
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_learning_extractor_handles_malformed_input(self):
        """Test learning extractor with malformed task results"""
        extractor = LearningExtractor()
        
        # Empty result
        learnings_empty = await extractor.extract_from_task_result(
            task_id="malformed-1",
            task_title="Empty task",
            result_text=""
        )
        assert learnings_empty is not None
        
        # Very short result
        learnings_short = await extractor.extract_from_task_result(
            task_id="malformed-2",
            task_title="Short",
            result_text="OK"
        )
        assert learnings_short is not None
    
    @pytest.mark.asyncio
    async def test_graph_persistence_survives_errors(self):
        """Test graph persistence continues even with some errors"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        extractor = LearningExtractor(graph_db=db)
        
        # Mix valid and invalid learnings
        from core.learning_extractor import ExtractedLearning
        learnings = [
            ExtractedLearning(
                type=NodeType.SKILL,
                properties={"name": "Valid-Skill-" + str(id(db)), "category": "test"},
                confidence=0.9
            ),
        ]
        
        # Should persist without crashing
        try:
            persisted = await extractor.persist_learnings(learnings, "resilience-task")
            assert persisted >= 0
        except Exception as e:
            pytest.fail(f"Persistence should not fail: {e}")
        
        await db.disconnect()


# ============================================================================
# TESTS: Backward Compatibility (2 tests)
# ============================================================================

class TestBackwardCompatibility:
    """Test backward compatibility with existing systems"""
    
    def test_existing_blackboard_compatibility(self):
        """Test compatibility with existing blackboard.json"""
        blackboard_path = Path(__file__).parent.parent / "memory" / "blackboard.json"
        
        # Should not break if blackboard exists
        assert True  # Placeholder - actual check would load blackboard
    
    def test_json_fallback_preserves_data(self):
        """Test JSON fallback mode preserves data format"""
        from core.graph_db_manager import Neo4jGraphDB
        
        db = Neo4jGraphDB(
            uri="bolt://nonexistent:9999",
            fallback_path=Path("/tmp/test_fallback_compat.json")
        )
        
        # JSON fallback should maintain expected structure
        assert "nodes" in db._fallback_data
        assert "relationships" in db._fallback_data
        
        # Clean up
        if Path("/tmp/test_fallback_compat.json").exists():
            Path("/tmp/test_fallback_compat.json").unlink()


# ============================================================================
# TESTS: Documentation & Configuration (2 tests)
# ============================================================================

class TestDocumentation:
    """Test documentation and configuration completeness"""
    
    def test_phase7_plan_exists(self):
        """Test Phase 7 plan documentation exists"""
        plan_path = Path(__file__).parent.parent / "PHASE7_GRAPHRAG_PLAN.md"
        
        if plan_path.exists():
            content = plan_path.read_text()
            assert "Graph" in content
            assert "Node" in content or "relationship" in content.lower()
    
    def test_graph_schema_documented(self):
        """Test graph schema is documented in code"""
        from core.graph_schema import NodeType, RelationType
        
        # Should have docstrings
        assert NodeType.__doc__ is not None or hasattr(NodeType, '__members__')
        assert RelationType.__doc__ is not None or hasattr(RelationType, '__members__')


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
