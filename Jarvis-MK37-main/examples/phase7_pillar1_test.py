"""
Phase 7 Pillar 1: Graph Infrastructure Tests

Tests for Neo4j graph database manager and schema.
Covers: connection, CRUD operations, resilience, fallback mode.

Total: 15 tests
"""

import pytest
import asyncio
from pathlib import Path
from typing import Optional

from core.graph_schema import (
    NodeType, RelationType, NodeConstraint, GraphSchemaValidator,
    UserNode, ProjectNode, SkillNode, ConceptNode, ErrorNode, SubjectNode, TaskNode,
    GraphRelationship
)
from core.graph_db_manager import Neo4jGraphDB, get_graph_db, close_graph_db


# ============================================================================
# TESTS: Schema Definition & Validation (5 tests)
# ============================================================================

class TestGraphSchema:
    """Test graph schema definition and validation"""
    
    def test_node_types_defined(self):
        """Test that all node types are properly defined"""
        assert NodeType.USER.value == "User"
        assert NodeType.PROJECT.value == "Project"
        assert NodeType.SKILL.value == "Skill"
        assert NodeType.TASK.value == "Task"
        assert NodeType.CONCEPT.value == "Concept"
        assert NodeType.SUBJECT.value == "Subject"
        assert NodeType.ERROR.value == "Error"
    
    def test_relationship_types_defined(self):
        """Test that all relationship types are properly defined"""
        assert RelationType.STUDIES.value == "STUDIES"
        assert RelationType.WORKS_ON.value == "WORKS_ON"
        assert RelationType.REQUIRES.value == "REQUIRES"
        assert RelationType.LEARNED_IN.value == "LEARNED_IN"
        assert RelationType.HAS_SKILL.value == "HAS_SKILL"
        assert RelationType.USES_SKILL.value == "USES_SKILL"
        assert RelationType.RELATED_TO.value == "RELATED_TO"
        assert RelationType.SOLVES.value == "SOLVES"
    
    def test_node_validation_success(self):
        """Test successful node data validation"""
        data = {"name": "Test Project", "language": "Python"}
        is_valid, error = GraphSchemaValidator.validate_node_data(NodeType.PROJECT, data)
        assert is_valid is True
        assert error is None
    
    def test_node_validation_failure_missing_field(self):
        """Test node validation fails with missing required fields"""
        data = {"name": "Incomplete"}  # Missing language
        is_valid, error = GraphSchemaValidator.validate_node_data(NodeType.PROJECT, data)
        assert is_valid is False
        assert "language" in error or "Missing" in error
    
    def test_relationship_direction_validation(self):
        """Test relationship direction validation"""
        # Valid relationship
        is_valid = GraphSchemaValidator.validate_relationship_direction(
            NodeType.USER, RelationType.STUDIES, NodeType.SUBJECT
        )
        assert is_valid is True
        
        # Valid relationship (reversed)
        is_valid = GraphSchemaValidator.validate_relationship_direction(
            NodeType.USER, RelationType.WORKS_ON, NodeType.PROJECT
        )
        assert is_valid is True


# ============================================================================
# TESTS: Database Connection & Initialization (3 tests)
# ============================================================================

class TestGraphConnection:
    """Test database connection and initialization"""
    
    @pytest.mark.asyncio
    async def test_connect_fallback_mode(self):
        """Test connection with fallback to JSON mode"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        connected = await db.connect()
        
        assert connected is True
        assert db.use_fallback is True
        assert db.is_neo4j_available is False
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_fallback_data_initialization(self):
        """Test fallback data structure initialization"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        assert "nodes" in db._fallback_data
        assert "relationships" in db._fallback_data
        assert isinstance(db._fallback_data["nodes"], dict)
        assert isinstance(db._fallback_data["relationships"], list)
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_disconnect_saves_fallback(self):
        """Test that disconnect saves fallback data to JSON"""
        db = Neo4jGraphDB(
            uri="bolt://nonexistent:9999",
            fallback_path=Path("/tmp/test_graph_fallback.json")
        )
        await db.connect()
        
        # Add some data
        db._fallback_data["nodes"]["test"] = {"type": "User", "name": "Test"}
        
        await db.disconnect()
        
        # Verify file was created
        assert Path("/tmp/test_graph_fallback.json").exists()
        
        # Clean up
        Path("/tmp/test_graph_fallback.json").unlink()


# ============================================================================
# TESTS: CRUD Operations (4 tests)
# ============================================================================

class TestGraphCRUD:
    """Test Create, Read, Update, Delete operations"""
    
    @pytest.mark.asyncio
    async def test_create_node(self):
        """Test creating a node in fallback mode"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        project_props = {
            "name": "test-project-create",
            "language": "Python",
            "framework": "FastAPI",
        }
        
        node_id = await db.create_node(NodeType.PROJECT, project_props)
        
        assert node_id is not None
        assert node_id == "test-project-create"
        assert "test-project-create" in db._fallback_data["nodes"]
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_create_relationship(self):
        """Test creating a relationship"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create nodes with unique names
        user_id = await db.create_node(NodeType.USER, {"name": "JARVIS_REL_TEST_" + str(id(db))})
        project_id = await db.create_node(
            NodeType.PROJECT,
            {"name": "test-project-rel-" + str(id(db)), "language": "Python"}
        )
        
        # Create relationship
        success = await db.create_relationship(
            user_id,
            RelationType.WORKS_ON,
            project_id,
            {"started_date": "2026-04-25"}
        )
        
        assert success is True
        # Count relationships in this db instance
        rel_count = len([r for r in db._fallback_data["relationships"] 
                        if r["source"] == user_id and r["target"] == project_id])
        assert rel_count >= 1
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_find_nodes(self):
        """Test finding nodes by type and properties"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create multiple nodes with unique names
        await db.create_node(NodeType.PROJECT, {"name": "find-proj1", "language": "Python"})
        await db.create_node(NodeType.PROJECT, {"name": "find-proj2", "language": "Go"})
        
        # Find projects (may include previous test data, so >= not ==)
        projects = await db.find_nodes(NodeType.PROJECT)
        assert len(projects) >= 2
        
        # Find projects with specific language
        python_projs = await db.find_nodes(
            NodeType.PROJECT,
            {"language": "Python"},
            limit=10
        )
        found_python = any(p["name"] == "find-proj1" for p in python_projs)
        assert found_python is True
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_delete_node(self):
        """Test deleting a node"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create node and relationship with unique names
        node_id = await db.create_node(NodeType.PROJECT, {"name": "delete-proj", "language": "Python"})
        other_id = await db.create_node(NodeType.SKILL, {"name": "delete-skill", "category": "web"})
        
        initial_rel_count = len(db._fallback_data["relationships"])
        
        await db.create_relationship(node_id, RelationType.USES_SKILL, other_id)
        
        # Verify created
        assert node_id in db._fallback_data["nodes"]
        assert len(db._fallback_data["relationships"]) == initial_rel_count + 1
        
        # Delete
        deleted = await db.delete_node(node_id)
        
        assert deleted is True
        assert node_id not in db._fallback_data["nodes"]
        assert len(db._fallback_data["relationships"]) == initial_rel_count
        
        await db.disconnect()


# ============================================================================
# TESTS: Resilience & Error Handling (3 tests)
# ============================================================================

class TestGraphResilience:
    """Test resilience and error handling"""
    
    @pytest.mark.asyncio
    async def test_invalid_node_creation(self):
        """Test that invalid node data is rejected"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Missing required field
        invalid_props = {"description": "Missing name"}
        node_id = await db.create_node(NodeType.PROJECT, invalid_props)
        
        assert node_id is None
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_find_nonexistent_nodes(self):
        """Test finding nodes that don't exist"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Search for nodes that don't exist
        results = await db.find_nodes(NodeType.PROJECT, {"name": "nonexistent"})
        
        assert results == []
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_query_in_fallback_mode(self):
        """Test that Cypher queries gracefully fail in fallback mode"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Try to execute query in fallback mode
        results = await db.query("MATCH (n) RETURN n")
        
        assert results == []
        assert db.use_fallback is True
        
        await db.disconnect()


# ============================================================================
# TESTS: Node Types & Data Classes (3 tests)
# ============================================================================

class TestNodeDataClasses:
    """Test node data classes"""
    
    def test_user_node_creation(self):
        """Test UserNode dataclass"""
        user = UserNode(name="JARVIS", role="AI_ASSISTANT")
        data = user.to_dict()
        
        assert data["name"] == "JARVIS"
        assert data["role"] == "AI_ASSISTANT"
        assert "created_at" in data
    
    def test_project_node_creation(self):
        """Test ProjectNode dataclass"""
        proj = ProjectNode(
            name="test-proj",
            language="Python",
            framework="FastAPI",
            status="in_progress"
        )
        data = proj.to_dict()
        
        assert data["name"] == "test-proj"
        assert data["language"] == "Python"
        assert data["status"] == "in_progress"
    
    def test_error_node_creation(self):
        """Test ErrorNode dataclass"""
        error = ErrorNode(
            type="memory_leak",
            language="Go",
            solution="Use defer cleanup",
            frequency=3
        )
        data = error.to_dict()
        
        assert data["type"] == "memory_leak"
        assert data["language"] == "Go"
        assert data["frequency"] == 3


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
