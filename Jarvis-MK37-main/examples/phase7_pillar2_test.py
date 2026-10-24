"""
Phase 7 Pillar 2: GraphRAG Integration Tests

Tests for GraphRAG overlay combining graph + RAG queries.
Covers: concept discovery, cross-language errors, skill paths, prerequisites.

Total: 20 tests
"""

import pytest
import asyncio
from typing import List

from core.graph_db_manager import Neo4jGraphDB
from core.graphrag_overlay import (
    GraphRAG, GraphRAGResult, GraphNode, RelationshipMatch,
    QueryDepth, get_graphrag, close_graphrag
)
from core.graph_schema import NodeType, RelationType


# ============================================================================
# TESTS: GraphRAG Initialization & Setup (2 tests)
# ============================================================================

class TestGraphRAGInitialization:
    """Test GraphRAG initialization"""
    
    @pytest.mark.asyncio
    async def test_graphrag_initialization(self):
        """Test GraphRAG initialization"""
        graphrag = GraphRAG()
        result = await graphrag.initialize()
        
        assert result is True
        assert graphrag.graph_db is not None
    
    @pytest.mark.asyncio
    async def test_get_graphrag_singleton(self):
        """Test get_graphrag returns singleton"""
        await close_graphrag()
        
        graphrag1 = await get_graphrag()
        graphrag2 = await get_graphrag()
        
        assert graphrag1 is graphrag2
        assert graphrag1.graph_db is not None


# ============================================================================
# TESTS: Project Concept Queries (4 tests)
# ============================================================================

class TestProjectConceptQueries:
    """Test querying concepts related to projects"""
    
    @pytest.mark.asyncio
    async def test_query_project_concepts_nonexistent(self):
        """Test querying concepts for non-existent project"""
        graphrag = GraphRAG()
        await graphrag.initialize()
        
        result = await graphrag.query_project_concepts("nonexistent-project")
        
        assert isinstance(result, GraphRAGResult)
        assert result.confidence_score == 0.0
        assert len(result.graph_nodes) == 0
    
    @pytest.mark.asyncio
    async def test_query_project_concepts_empty_graph(self):
        """Test project concept query with empty graph"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_project_concepts("unique-project-id-" + str(id(db)))
        
        assert isinstance(result, GraphRAGResult)
        # With project not found, confidence should be 0.0
        assert result.confidence_score == 0.0
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_query_project_concepts_with_data(self):
        """Test project concept query with sample data"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create sample data
        proj_id = await db.create_node(
            NodeType.PROJECT,
            {"name": "graphrag-test-proj", "language": "Python"}
        )
        skill_id = await db.create_node(
            NodeType.SKILL,
            {"name": "async", "category": "concurrency", "proficiency": 0.8}
        )
        await db.create_relationship(proj_id, RelationType.USES_SKILL, skill_id)
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_project_concepts(proj_id)
        
        assert isinstance(result, GraphRAGResult)
        assert len(result.graph_nodes) >= 1
        assert result.graph_nodes[0].id == proj_id
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_query_project_concepts_depth_control(self):
        """Test project concept query with different depths"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        # Create nested structure
        proj_id = await db.create_node(
            NodeType.PROJECT,
            {"name": "depth-test-proj", "language": "Go"}
        )
        skill_id = await db.create_node(
            NodeType.SKILL,
            {"name": "goroutines", "category": "concurrency"}
        )
        await db.create_relationship(proj_id, RelationType.USES_SKILL, skill_id)
        
        # Test with DIRECT depth
        result_direct = await graphrag.query_project_concepts(proj_id, QueryDepth.DIRECT)
        assert isinstance(result_direct, GraphRAGResult)
        
        # Test with DEEP depth
        result_deep = await graphrag.query_project_concepts(proj_id, QueryDepth.DEEP)
        assert isinstance(result_deep, GraphRAGResult)
        
        await db.disconnect()


# ============================================================================
# TESTS: Cross-Language Error Queries (4 tests)
# ============================================================================

class TestCrossLanguageErrors:
    """Test cross-language error matching"""
    
    @pytest.mark.asyncio
    async def test_cross_language_no_errors(self):
        """Test cross-language query with no errors"""
        graphrag = GraphRAG()
        await graphrag.initialize()
        
        result = await graphrag.query_cross_language_errors(
            "memory_leak", "Go"
        )
        
        assert isinstance(result, GraphRAGResult)
        assert len(result.errors) == 0
    
    @pytest.mark.asyncio
    async def test_cross_language_same_language(self):
        """Test finding same error in same language"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create errors
        error1 = await db.create_node(
            NodeType.ERROR,
            {
                "type": "null_pointer",
                "language": "Java",
                "solution": "Add null checks"
            }
        )
        error2 = await db.create_node(
            NodeType.ERROR,
            {
                "type": "null_pointer",
                "language": "Java",
                "solution": "Validate null before access"
            }
        )
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_cross_language_errors(
            "null_pointer", "Java", target_languages=["Java"]
        )
        
        assert isinstance(result, GraphRAGResult)
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_solution_similarity(self):
        """Test semantic similarity detection in solutions"""
        graphrag = GraphRAG()
        
        # Test similar solutions
        sol1 = "Use defer for cleanup"
        sol2 = "Use cleanup with defer"
        
        similar = graphrag._are_solutions_similar(sol1, sol2)
        assert similar is True
        
        # Test dissimilar solutions
        sol3 = "Use goroutines for concurrency"
        sol4 = "Never use global variables"
        
        dissimilar = graphrag._are_solutions_similar(sol3, sol4)
        assert dissimilar is False
    
    @pytest.mark.asyncio
    async def test_cross_language_multiple_targets(self):
        """Test querying multiple target languages"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_cross_language_errors(
            "exception_handling",
            "Python",
            target_languages=["Java", "Go", "TypeScript"]
        )
        
        assert isinstance(result, GraphRAGResult)
        
        await db.disconnect()


# ============================================================================
# TESTS: Concept Prerequisites (3 tests)
# ============================================================================

class TestConceptPrerequisites:
    """Test concept prerequisite discovery"""
    
    @pytest.mark.asyncio
    async def test_concept_prerequisites_nonexistent(self):
        """Test prerequisites for non-existent concept"""
        graphrag = GraphRAG()
        await graphrag.initialize()
        
        result = await graphrag.query_concept_prerequisites("unknown-concept")
        
        assert isinstance(result, GraphRAGResult)
        assert result.confidence_score == 0.0
    
    @pytest.mark.asyncio
    async def test_concept_prerequisites_with_data(self):
        """Test prerequisites with sample concept"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create concepts
        concept_id = await db.create_node(
            NodeType.CONCEPT,
            {
                "name": "websockets",
                "domain": "networking",
                "related_languages": ["JavaScript", "Python", "Go"]
            }
        )
        prerequisite_id = await db.create_node(
            NodeType.CONCEPT,
            {
                "name": "http",
                "domain": "networking"
            }
        )
        
        # Create learning relationship
        await db.create_relationship(
            prerequisite_id,
            RelationType.LEARNED_IN,
            concept_id
        )
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_concept_prerequisites(concept_id)
        
        assert isinstance(result, GraphRAGResult)
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_concept_prerequisites_depth_control(self):
        """Test prerequisites with different depths"""
        graphrag = GraphRAG()
        await graphrag.initialize()
        
        result_direct = await graphrag.query_concept_prerequisites(
            "websockets", QueryDepth.DIRECT
        )
        assert isinstance(result_direct, GraphRAGResult)
        
        result_deep = await graphrag.query_concept_prerequisites(
            "websockets", QueryDepth.DEEP
        )
        assert isinstance(result_deep, GraphRAGResult)


# ============================================================================
# TESTS: Skill Mastery Paths (4 tests)
# ============================================================================

class TestSkillMasteryPath:
    """Test learning path generation"""
    
    @pytest.mark.asyncio
    async def test_skill_path_nonexistent_target(self):
        """Test skill path with non-existent target"""
        graphrag = GraphRAG()
        await graphrag.initialize()
        
        result = await graphrag.query_skill_mastery_path(
            "unknown-skill", ["Python"]
        )
        
        assert isinstance(result, GraphRAGResult)
        assert result.confidence_score == 0.0
    
    @pytest.mark.asyncio
    async def test_skill_path_simple(self):
        """Test simple skill path"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create skills
        python_id = await db.create_node(
            NodeType.SKILL,
            {"name": "Python", "category": "languages", "proficiency": 0.9}
        )
        go_id = await db.create_node(
            NodeType.SKILL,
            {"name": "Go", "category": "languages", "proficiency": 0.0}
        )
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_skill_mastery_path(
            "Go", ["Python"]
        )
        
        assert isinstance(result, GraphRAGResult)
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_skill_path_with_intermediates(self):
        """Test skill path with intermediate steps"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create skill progression
        python_id = await db.create_node(
            NodeType.SKILL,
            {"name": "Python", "category": "languages"}
        )
        async_id = await db.create_node(
            NodeType.SKILL,
            {"name": "async programming", "category": "concurrency"}
        )
        go_id = await db.create_node(
            NodeType.SKILL,
            {"name": "Go", "category": "languages"}
        )
        
        graphrag = GraphRAG(graph_db=db)
        await graphrag.initialize()
        
        result = await graphrag.query_skill_mastery_path(
            "Go", ["Python"]
        )
        
        assert isinstance(result, GraphRAGResult)
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_intermediate_skill_detection(self):
        """Test intermediate skill detection heuristic"""
        graphrag = GraphRAG()
        
        # Bridge skill
        bridge_skill = {"name": "refactoring", "category": "best_practices"}
        is_intermediate = graphrag._is_intermediate_skill(
            bridge_skill, ["Python"], "Go"
        )
        assert is_intermediate is True
        
        # Source skill (should not be intermediate)
        source_skill = {"name": "Python", "category": "languages"}
        is_intermediate = graphrag._is_intermediate_skill(
            source_skill, ["Python"], "Go"
        )
        assert is_intermediate is False
        
        # Target skill (should not be intermediate)
        target_skill = {"name": "Go", "category": "languages"}
        is_intermediate = graphrag._is_intermediate_skill(
            target_skill, ["Python"], "Go"
        )
        assert is_intermediate is False


# ============================================================================
# TESTS: GraphRAG Result Structure (2 tests)
# ============================================================================

class TestGraphRAGResults:
    """Test GraphRAG result structures"""
    
    def test_graphrag_result_creation(self):
        """Test GraphRAGResult dataclass"""
        result = GraphRAGResult(
            query="test_query",
            graph_nodes=[],
            graph_relationships=[],
            rag_results=[],
            concepts=[],
            errors=[],
            confidence_score=0.8
        )
        
        assert result.query == "test_query"
        assert result.confidence_score == 0.8
        assert isinstance(result.graph_nodes, list)
    
    def test_graph_node_creation(self):
        """Test GraphNode dataclass"""
        node = GraphNode(
            id="test-node",
            type=NodeType.PROJECT,
            properties={"name": "test", "language": "Python"}
        )
        
        assert node.id == "test-node"
        assert node.type == NodeType.PROJECT
        assert node.properties["name"] == "test"
        assert "Project" in str(node)


# ============================================================================
# TESTS: Statistics & Metadata (1 test)
# ============================================================================

class TestGraphRAGStatistics:
    """Test statistics and metadata"""
    
    @pytest.mark.asyncio
    async def test_get_statistics(self):
        """Test getting GraphRAG statistics"""
        graphrag = GraphRAG()
        await graphrag.initialize()
        
        stats = await graphrag.get_statistics()
        
        assert isinstance(stats, dict)
        assert "graphrag_mode" in stats
        assert "rag_index_size" in stats
        assert "concept_cache_size" in stats


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
