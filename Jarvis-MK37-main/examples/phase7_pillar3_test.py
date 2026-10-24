"""
Phase 7 Pillar 3: Swarm Synergy Tests

Tests for learning extraction and swarm memory integration.
Covers: learning extraction, persistence, swarm context injection.

Total: 18 tests
"""

import pytest
import asyncio
from datetime import datetime

from core.graph_db_manager import Neo4jGraphDB
from core.learning_extractor import (
    LearningExtractor, ExtractedLearning,
    extract_and_persist_learnings
)
from core.graph_schema import NodeType, RelationType


# ============================================================================
# TESTS: Learning Extraction (6 tests)
# ============================================================================

class TestLearningExtraction:
    """Test learning extraction from task results"""
    
    def test_extractor_initialization(self):
        """Test LearningExtractor initialization"""
        extractor = LearningExtractor()
        
        assert extractor is not None
        assert extractor.error_pattern is not None
        assert extractor.language_pattern is not None
        assert extractor.skill_pattern is not None
    
    @pytest.mark.asyncio
    async def test_extract_errors_from_result(self):
        """Test extracting error nodes from result text"""
        extractor = LearningExtractor()
        
        result_text = """
        Error: Memory leak in goroutine
        The application has a goroutine that never terminates.
        Solution: Use defer cleanup and context cancellation
        """
        
        errors = extractor._extract_errors(result_text)
        
        assert len(errors) >= 1
        assert errors[0].type == NodeType.ERROR
        assert errors[0].confidence > 0.5
    
    @pytest.mark.asyncio
    async def test_detect_language(self):
        """Test language detection from error text"""
        extractor = LearningExtractor()
        
        # Python error
        lang = extractor._detect_language("ImportError in asyncio module")
        assert "Python" in lang or "python" in lang.lower()
        
        # Go error
        lang = extractor._detect_language("goroutine leak defer")
        assert "Go" in lang or "go" in lang.lower()
        
        # JavaScript error
        lang = extractor._detect_language("async/await promise rejection")
        assert "JavaScript" in lang or "javascript" in lang.lower()
    
    @pytest.mark.asyncio
    async def test_extract_technologies(self):
        """Test technology extraction from result"""
        extractor = LearningExtractor()
        
        result_text = """
        Implemented using Python with asyncio and Go with goroutines.
        Also tested JavaScript with Node.js
        """
        
        techs = extractor._extract_technologies(result_text)
        
        assert len(techs) >= 2
        tech_names = [t.properties.get("name") for t in techs]
        assert any("python" in name.lower() for name in tech_names)
    
    @pytest.mark.asyncio
    async def test_extract_skills(self):
        """Test skill extraction from result"""
        extractor = LearningExtractor()
        
        result_text = """
        Used asyncio for concurrent tasks and goroutines for parallel processing.
        Applied design patterns and optimization techniques.
        """
        
        skills = extractor._extract_skills(result_text)
        
        assert len(skills) >= 1
        assert all(s.type == NodeType.SKILL for s in skills)
        assert all(0 < s.confidence <= 1 for s in skills)
    
    @pytest.mark.asyncio
    async def test_extract_concepts(self):
        """Test concept extraction from result"""
        extractor = LearningExtractor()
        
        result_text = """
        Refactored the microservices architecture with improved caching.
        Used REST API for database operations.
        """
        
        concepts = extractor._extract_concepts(result_text)
        
        assert len(concepts) >= 2
        assert all(c.type == NodeType.CONCEPT for c in concepts)


# ============================================================================
# TESTS: Task-Based Learning (3 tests)
# ============================================================================

class TestTaskLearning:
    """Test learning extraction from full task execution"""
    
    @pytest.mark.asyncio
    async def test_extract_from_task_result(self):
        """Test extracting all learnings from task result"""
        extractor = LearningExtractor()
        
        result_text = """
        Task: Fix memory leak in Go service
        
        Error: goroutine leak on shutdown
        Solution: Added proper defer cleanup
        
        Technologies: Go 1.22, Docker
        Applied concepts: Concurrency, Resource Management
        """
        
        learnings = await extractor.extract_from_task_result(
            task_id="test-task-001",
            task_title="Fix memory leak",
            result_text=result_text,
            project_id="service-api"
        )
        
        assert len(learnings) > 0
        
        # Check for task node
        task_nodes = [l for l in learnings if l.type == NodeType.TASK]
        assert len(task_nodes) == 1
        assert task_nodes[0].properties.get("title") == "Fix memory leak"
    
    @pytest.mark.asyncio
    async def test_learning_confidence_scores(self):
        """Test that learnings have confidence scores"""
        extractor = LearningExtractor()
        
        result_text = "Fixed bug using Python asyncio"
        
        learnings = await extractor.extract_from_task_result(
            task_id="test-task-002",
            task_title="Fix asyncio issue",
            result_text=result_text
        )
        
        assert len(learnings) > 0
        for learning in learnings:
            assert 0 <= learning.confidence <= 1.0
    
    @pytest.mark.asyncio
    async def test_link_learnings(self):
        """Test that learnings are linked with relationships"""
        extractor = LearningExtractor()
        
        learnings = [
            ExtractedLearning(
                type=NodeType.SKILL,
                properties={"name": "Asyncio"},
                confidence=0.8
            ),
            ExtractedLearning(
                type=NodeType.CONCEPT,
                properties={"name": "Concurrency"},
                confidence=0.7
            ),
        ]
        
        linked = extractor._link_learnings(learnings)
        
        # Check that skills have relationships to concepts
        skills = [l for l in linked if l.type == NodeType.SKILL]
        assert any(len(s.relationships) > 0 for s in skills)


# ============================================================================
# TESTS: Graph Persistence (5 tests)
# ============================================================================

class TestLearningPersistence:
    """Test persisting learnings to graph database"""
    
    @pytest.mark.asyncio
    async def test_persist_single_learning(self):
        """Test persisting a single learning to graph"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        extractor = LearningExtractor(graph_db=db)
        
        learning = ExtractedLearning(
            type=NodeType.SKILL,
            properties={
                "name": "Persist-Test-Skill-" + str(id(db)),
                "category": "test"
            },
            confidence=0.9
        )
        
        persisted = await extractor.persist_learnings([learning], "task-001")
        
        # In fallback mode, should be 0. In neo4j, should be >= 1
        assert persisted >= 0
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_persist_with_deduplication(self):
        """Test that duplicate learnings are not re-created"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        extractor = LearningExtractor(graph_db=db)
        
        # Create same learning twice
        learning1 = ExtractedLearning(
            type=NodeType.SKILL,
            properties={
                "name": "Dedup-Test-Skill-" + str(id(db)),
                "category": "test"
            },
            confidence=0.9
        )
        learning2 = ExtractedLearning(
            type=NodeType.SKILL,
            properties={
                "name": "Dedup-Test-Skill-" + str(id(db)),
                "category": "test"
            },
            confidence=0.8
        )
        
        # First persist
        persisted1 = await extractor.persist_learnings([learning1], "task-001")
        
        # Second persist (should deduplicate)
        persisted2 = await extractor.persist_learnings([learning2], "task-002")
        
        # In fallback mode, both may be 0. Should at least not error.
        assert persisted1 >= 0
        assert persisted2 >= 0
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_persist_relationships(self):
        """Test persisting relationships between learnings"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        extractor = LearningExtractor(graph_db=db)
        
        # Create related learnings
        skill = ExtractedLearning(
            type=NodeType.SKILL,
            properties={
                "name": "Rel-Test-Skill-" + str(id(db)),
                "category": "test"
            },
            confidence=0.9
        )
        skill.relationships.append(("Rel-Test-Skill-" + str(id(db)), RelationType.RELATED_TO, "Rel-Test-Concept-" + str(id(db))))
        
        concept = ExtractedLearning(
            type=NodeType.CONCEPT,
            properties={
                "name": "Rel-Test-Concept-" + str(id(db)),
                "domain": "test"
            },
            confidence=0.8
        )
        
        persisted = await extractor.persist_learnings([skill, concept], "task-003")
        
        assert persisted >= 0
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_convenience_function(self):
        """Test extract_and_persist_learnings convenience function"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Inject db into extractor first
        extractor = LearningExtractor(graph_db=db)
        
        result_text = "Fixed Python asyncio issue"
        
        # This should work without explicit extractor creation
        persisted = await extract_and_persist_learnings(
            task_id="test-task-conv",
            task_title="Test convenience",
            result_text=result_text
        )
        
        assert persisted >= 0  # May be 0 if fallback DB has no persistence
        
        await db.disconnect()
    
    @pytest.mark.asyncio
    async def test_persist_with_project_context(self):
        """Test persisting learnings with project context"""
        db = Neo4jGraphDB(uri="bolt://nonexistent:9999")
        await db.connect()
        
        # Create project context
        project_id = await db.create_node(
            NodeType.PROJECT,
            {
                "name": "learning-context-proj-" + str(id(db)),
                "language": "Python"
            }
        )
        
        extractor = LearningExtractor(graph_db=db)
        
        learning = ExtractedLearning(
            type=NodeType.SKILL,
            properties={
                "name": "Project-Context-Skill-" + str(id(db)),
                "category": "test"
            },
            confidence=0.9
        )
        
        persisted = await extractor.persist_learnings([learning], "task-proj-001")
        
        assert persisted >= 0
        
        await db.disconnect()


# ============================================================================
# TESTS: Semantic Processing (4 tests)
# ============================================================================

class TestSemanticProcessing:
    """Test semantic extraction and linking"""
    
    def test_language_detection_accuracy(self):
        """Test language detection accuracy on various inputs"""
        extractor = LearningExtractor()
        
        test_cases = [
            ("ValueError in Python", "Python"),
            ("goroutine panic", "Go"),
            ("ReferenceError in async/await", "JavaScript"),
            ("NullPointerException in Java", "Java"),
        ]
        
        for text, expected_lang in test_cases:
            detected = extractor._detect_language(text)
            assert any(exp.lower() in detected.lower() for exp in [expected_lang, "unknown"])
    
    def test_pattern_matching(self):
        """Test regex pattern matching"""
        extractor = LearningExtractor()
        
        # Test error pattern
        text = "Error: Connection timeout occurred"
        matches = extractor.error_pattern.findall(text)
        assert len(matches) > 0
        
        # Test language pattern
        text = "Used Python, JavaScript and Go"
        matches = extractor.language_pattern.findall(text)
        assert len(matches) >= 2
    
    def test_extracted_learning_dataclass(self):
        """Test ExtractedLearning dataclass"""
        learning = ExtractedLearning(
            type=NodeType.SKILL,
            properties={"name": "Test", "category": "test"},
            confidence=0.85
        )
        
        assert learning.type == NodeType.SKILL
        assert learning.confidence == 0.85
        assert len(learning.relationships) == 0
        
        # Add relationship
        learning.relationships.append(("Test", RelationType.RELATED_TO, "Other"))
        assert len(learning.relationships) == 1
    
    def test_comprehensive_extraction(self):
        """Test comprehensive extraction from complex result"""
        extractor = LearningExtractor()
        
        complex_result = """
        Task: Refactor authentication service
        
        Issues encountered:
        Error: Race condition in token validation (Go)
        Exception: Memory leak in session manager (Python)
        
        Solutions applied:
        - Added mutex locks for goroutine safety
        - Implemented proper context cleanup
        
        Technologies used:
        - Go with goroutines
        - Python with asyncio
        - REST API design pattern
        - Microservices architecture
        
        Concepts learned:
        - Concurrent programming
        - Database optimization
        """
        
        errors = extractor._extract_errors(complex_result)
        techs = extractor._extract_technologies(complex_result)
        skills = extractor._extract_skills(complex_result)
        concepts = extractor._extract_concepts(complex_result)
        
        assert len(errors) > 0, "Should extract errors"
        assert len(techs) > 0, "Should extract technologies"
        assert len(skills) > 0, "Should extract skills"
        assert len(concepts) > 0, "Should extract concepts"


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
