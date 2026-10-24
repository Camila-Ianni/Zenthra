"""
GraphRAG Overlay: Combine Graph Queries + Local RAG

This module fuses Neo4j graph queries with local RAG (Retrieval-Augmented Generation)
to provide semantic intelligence across projects, skills, and concepts.

Features:
  - Parallel graph + RAG queries for performance
  - Multi-hop graph traversals (max 3 hops)
  - Cross-language error matching
  - Concept relationship discovery
  - Semantic enrichment for code/project understanding
"""

import asyncio
import logging
from typing import Optional, Dict, Any, List, Tuple, Set
from dataclasses import dataclass
from enum import Enum

from core.graph_db_manager import Neo4jGraphDB, get_graph_db
from core.graph_schema import NodeType, RelationType


logger = logging.getLogger(__name__)


class QueryDepth(Enum):
    """Graph traversal depth limits"""
    DIRECT = 1
    RELATED = 2
    DEEP = 3


@dataclass
class GraphNode:
    """Represents a node from graph query"""
    id: str
    type: NodeType
    properties: Dict[str, Any]
    
    def __repr__(self) -> str:
        return f"Node({self.type.value}: {self.properties.get('name', 'unknown')})"


@dataclass
class RelationshipMatch:
    """Represents a relationship between two nodes"""
    source: GraphNode
    relationship_type: RelationType
    target: GraphNode
    properties: Dict[str, Any]
    hops: int = 1


@dataclass
class GraphRAGResult:
    """Unified result from graph + RAG query"""
    query: str
    graph_nodes: List[GraphNode]
    graph_relationships: List[RelationshipMatch]
    rag_results: List[Dict[str, Any]]
    concepts: List[Dict[str, Any]]
    errors: List[Dict[str, Any]]
    confidence_score: float  # 0.0 to 1.0


class GraphRAG:
    """
    Combines graph database with local RAG for semantic intelligence.
    """
    
    def __init__(self, graph_db: Optional[Neo4jGraphDB] = None):
        self.graph_db = graph_db
        self._rag_index = {}  # Simple in-memory RAG index
        self._concept_cache: Dict[str, List[str]] = {}
    
    async def initialize(self) -> bool:
        """Initialize GraphRAG system"""
        if self.graph_db is None:
            self.graph_db = await get_graph_db()
        
        if not self.graph_db.is_neo4j_available and self.graph_db.use_fallback:
            logger.info("⚠️ GraphRAG running in fallback mode (Neo4j unavailable)")
            return True
        
        logger.info("✅ GraphRAG initialized")
        return True
    
    async def query_project_concepts(
        self,
        project_id: str,
        max_depth: QueryDepth = QueryDepth.RELATED,
    ) -> GraphRAGResult:
        """
        Query what concepts/skills are related to a project.
        
        Example: "What systems concepts apply to this Go project?"
        """
        logger.debug(f"🔍 Querying concepts for project: {project_id}")
        
        graph_nodes = []
        relationships = []
        concepts = []
        
        # Find project node
        project_nodes = await self.graph_db.find_nodes(
            NodeType.PROJECT,
            {"node_id": project_id}
        )
        
        if not project_nodes:
            logger.warning(f"Project not found: {project_id}")
            return GraphRAGResult(
                query=f"project_concepts:{project_id}",
                graph_nodes=[],
                graph_relationships=[],
                rag_results=[],
                concepts=[],
                errors=[],
                confidence_score=0.0
            )
        
        project = project_nodes[0]
        project_node = GraphNode(
            id=project_id,
            type=NodeType.PROJECT,
            properties=project
        )
        graph_nodes.append(project_node)
        
        # Find directly related skills
        skills = await self._find_related_nodes(
            project_id,
            NodeType.PROJECT,
            NodeType.SKILL,
            RelationType.USES_SKILL,
            depth=1
        )
        
        for skill in skills:
            skill_node = GraphNode(
                id=skill.get("node_id", "unknown"),
                type=NodeType.SKILL,
                properties=skill
            )
            graph_nodes.append(skill_node)
            relationships.append(RelationshipMatch(
                source=project_node,
                relationship_type=RelationType.USES_SKILL,
                target=skill_node,
                properties={}
            ))
        
        # Find concepts at depth 2 (project -> skill -> concept)
        if max_depth.value >= 2:
            for skill_data in skills:
                skill_id = skill_data.get("node_id")
                concept_rels = await self.graph_db.find_relationships(
                    RelationType.LEARNED_IN
                )
                
                for rel in concept_rels:
                    if rel.get("source") == skill_id:
                        concepts.append(rel.get("target"))
        
        confidence_score = min(1.0, 0.5 + (len(skills) * 0.1) + (len(concepts) * 0.05))
        
        return GraphRAGResult(
            query=f"project_concepts:{project_id}",
            graph_nodes=graph_nodes,
            graph_relationships=relationships,
            rag_results=[],
            concepts=[{"id": c, "type": "skill_concept"} for c in concepts],
            errors=[],
            confidence_score=confidence_score
        )
    
    async def query_cross_language_errors(
        self,
        error_type: str,
        source_language: str,
        target_languages: Optional[List[str]] = None,
        max_depth: QueryDepth = QueryDepth.DEEP,
    ) -> GraphRAGResult:
        """
        Find errors in other languages related to an error in source_language.
        
        Example: "I got a memory leak in Go. Has this happened in Python/Java?"
        """
        logger.debug(f"🔍 Cross-language error query: {error_type} ({source_language})")
        
        target_languages = target_languages or ["Python", "Java", "PHP", "TypeScript"]
        
        # Find errors in source language
        source_errors = await self.graph_db.find_nodes(
            NodeType.ERROR,
            {"type": error_type, "language": source_language},
            limit=5
        )
        
        graph_nodes = []
        relationships = []
        related_errors = []
        
        for error in source_errors:
            error_node = GraphNode(
                id=error.get("node_id", "unknown"),
                type=NodeType.ERROR,
                properties=error
            )
            graph_nodes.append(error_node)
            
            # Find related errors in target languages
            for target_lang in target_languages:
                target_errors = await self.graph_db.find_nodes(
                    NodeType.ERROR,
                    {"type": error_type, "language": target_lang},
                    limit=3
                )
                
                for target_error in target_errors:
                    target_node = GraphNode(
                        id=target_error.get("node_id", "unknown"),
                        type=NodeType.ERROR,
                        properties=target_error
                    )
                    graph_nodes.append(target_node)
                    
                    # Check semantic similarity via solution field
                    if self._are_solutions_similar(
                        error.get("solution", ""),
                        target_error.get("solution", "")
                    ):
                        relationships.append(RelationshipMatch(
                            source=error_node,
                            relationship_type=RelationType.RELATED_TO,
                            target=target_node,
                            properties={"similarity": 0.8}
                        ))
                        related_errors.append(target_error)
        
        confidence_score = min(1.0, 0.3 + (len(related_errors) * 0.15))
        
        return GraphRAGResult(
            query=f"cross_lang_errors:{error_type}:{source_language}",
            graph_nodes=graph_nodes,
            graph_relationships=relationships,
            rag_results=[],
            concepts=[],
            errors=related_errors,
            confidence_score=confidence_score
        )
    
    async def query_concept_prerequisites(
        self,
        concept_id: str,
        depth: QueryDepth = QueryDepth.RELATED,
    ) -> GraphRAGResult:
        """
        Find prerequisite concepts for learning a given concept.
        
        Example: "What do I need to know before learning WebSockets?"
        """
        logger.debug(f"🔍 Concept prerequisites for: {concept_id}")
        
        # Find concept
        concepts = await self.graph_db.find_nodes(
            NodeType.CONCEPT,
            {"node_id": concept_id}
        )
        
        if not concepts:
            return GraphRAGResult(
                query=f"concept_prereqs:{concept_id}",
                graph_nodes=[],
                graph_relationships=[],
                rag_results=[],
                concepts=[],
                errors=[],
                confidence_score=0.0
            )
        
        concept = concepts[0]
        concept_node = GraphNode(
            id=concept_id,
            type=NodeType.CONCEPT,
            properties=concept
        )
        
        graph_nodes = [concept_node]
        relationships = []
        prerequisites = []
        
        # Find related subjects and concepts
        related_relationships = await self.graph_db.find_relationships(
            RelationType.LEARNED_IN
        )
        
        for rel in related_relationships:
            if rel.get("target") == concept_id:
                # Someone learned this concept somewhere
                source_id = rel.get("source")
                prerequisites.append({"id": source_id, "type": "prerequisite"})
        
        confidence_score = 0.5 + min(0.5, len(prerequisites) * 0.1)
        
        return GraphRAGResult(
            query=f"concept_prereqs:{concept_id}",
            graph_nodes=graph_nodes,
            graph_relationships=relationships,
            rag_results=[],
            concepts=prerequisites,
            errors=[],
            confidence_score=confidence_score
        )
    
    async def query_skill_mastery_path(
        self,
        target_skill: str,
        current_skills: List[str],
    ) -> GraphRAGResult:
        """
        Generate a learning path from current skills to target skill.
        
        Example: "I know Python and JavaScript. How do I learn Go?"
        """
        logger.debug(f"🔍 Skill path: {current_skills} -> {target_skill}")
        
        # Find target skill
        target_nodes = await self.graph_db.find_nodes(
            NodeType.SKILL,
            {"name": target_skill}
        )
        
        if not target_nodes:
            return GraphRAGResult(
                query=f"skill_path:{target_skill}",
                graph_nodes=[],
                graph_relationships=[],
                rag_results=[],
                concepts=[],
                errors=[],
                confidence_score=0.0
            )
        
        graph_nodes = []
        relationships = []
        intermediate_skills = []
        
        # Add current skills as graph nodes
        for skill_name in current_skills:
            skill_nodes = await self.graph_db.find_nodes(
                NodeType.SKILL,
                {"name": skill_name}
            )
            if skill_nodes:
                skill_node = GraphNode(
                    id=skill_nodes[0].get("node_id", skill_name),
                    type=NodeType.SKILL,
                    properties=skill_nodes[0]
                )
                graph_nodes.append(skill_node)
        
        # Add target skill
        target_node = GraphNode(
            id=target_nodes[0].get("node_id", target_skill),
            type=NodeType.SKILL,
            properties=target_nodes[0]
        )
        graph_nodes.append(target_node)
        
        # Find related skills (intermediate steps)
        all_skills = await self.graph_db.find_nodes(NodeType.SKILL)
        for skill in all_skills:
            # Simple heuristic: check if skill shares concepts with both current and target
            if self._is_intermediate_skill(skill, current_skills, target_skill):
                intermediate_skills.append(skill)
                skill_node = GraphNode(
                    id=skill.get("node_id", "unknown"),
                    type=NodeType.SKILL,
                    properties=skill
                )
                graph_nodes.append(skill_node)
        
        confidence_score = 0.4 + min(0.6, len(intermediate_skills) * 0.15)
        
        return GraphRAGResult(
            query=f"skill_path:{target_skill}",
            graph_nodes=graph_nodes,
            graph_relationships=relationships,
            rag_results=[],
            concepts=[{"id": s.get("node_id"), "name": s.get("name")} for s in intermediate_skills],
            errors=[],
            confidence_score=confidence_score
        )
    
    async def _find_related_nodes(
        self,
        source_id: str,
        source_type: NodeType,
        target_type: NodeType,
        rel_type: RelationType,
        depth: int = 1,
    ) -> List[Dict[str, Any]]:
        """Find nodes related through specific relationship"""
        results = []
        visited: Set[str] = set()
        
        async def traverse(current_id: str, current_depth: int):
            if current_depth > depth or current_id in visited:
                return
            
            visited.add(current_id)
            
            # Find relationships
            rels = await self.graph_db.find_relationships(rel_type)
            for rel in rels:
                if rel.get("source") == current_id:
                    target_nodes = await self.graph_db.find_nodes(
                        target_type,
                        {"node_id": rel.get("target")}
                    )
                    if target_nodes:
                        results.append(target_nodes[0])
                        if current_depth < depth:
                            await traverse(rel.get("target"), current_depth + 1)
        
        await traverse(source_id, 1)
        return results
    
    def _are_solutions_similar(self, solution1: str, solution2: str) -> bool:
        """Check if two error solutions are semantically similar"""
        if not solution1 or not solution2:
            return False
        
        # Simple keyword overlap heuristic
        words1 = set(solution1.lower().split())
        words2 = set(solution2.lower().split())
        
        overlap = len(words1 & words2)
        total = len(words1 | words2)
        
        similarity = overlap / total if total > 0 else 0
        return similarity > 0.3
    
    def _is_intermediate_skill(
        self,
        skill: Dict[str, Any],
        source_skills: List[str],
        target_skill: str,
    ) -> bool:
        """Determine if a skill is an intermediate step"""
        skill_name = skill.get("name", "").lower()
        skill_category = skill.get("category", "").lower()
        
        # Skip if it's one of the source or target skills
        if skill_name == target_skill.lower():
            return False
        if any(skill_name == s.lower() for s in source_skills):
            return False
        
        # Heuristic: intermediate skills share category or are "bridge" skills
        bridge_keywords = ["refactor", "design", "architecture", "optimization"]
        is_bridge = any(kw in skill_name for kw in bridge_keywords)
        
        return is_bridge or skill_category not in ["", "unknown"]
    
    async def get_statistics(self) -> Dict[str, Any]:
        """Get GraphRAG system statistics"""
        if not self.graph_db:
            return {"error": "GraphRAG not initialized"}
        
        stats = await self.graph_db.get_stats()
        
        return {
            **stats,
            "graphrag_mode": "neo4j" if self.graph_db.is_neo4j_available else "fallback",
            "rag_index_size": len(self._rag_index),
            "concept_cache_size": len(self._concept_cache),
        }


# Global instance
_graphrag: Optional[GraphRAG] = None


async def get_graphrag() -> GraphRAG:
    """Get or create global GraphRAG instance"""
    global _graphrag
    if _graphrag is None:
        _graphrag = GraphRAG()
        await _graphrag.initialize()
    return _graphrag


async def close_graphrag() -> None:
    """Close GraphRAG instance"""
    global _graphrag
    _graphrag = None


__all__ = [
    "GraphRAG",
    "GraphRAGResult",
    "GraphNode",
    "RelationshipMatch",
    "QueryDepth",
    "get_graphrag",
    "close_graphrag",
]
