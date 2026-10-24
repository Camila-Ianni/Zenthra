"""
Learning Extractor for Swarm Memory Persistence

Analyzes task results and automatically extracts learnings as new graph nodes.
Integrates with swarm orchestrator for post-task memory updates.

Features:
  - Automatic concept/error node creation
  - Relationship inference from task results
  - Confidence scoring for new knowledge
  - Semantic deduplication
"""

import logging
import re
from typing import Optional, Dict, Any, List, Tuple
from dataclasses import dataclass
from datetime import datetime

from core.graph_db_manager import Neo4jGraphDB, get_graph_db
from core.graph_schema import NodeType, RelationType


logger = logging.getLogger(__name__)


@dataclass
class ExtractedLearning:
    """Represents a learning extracted from task results"""
    type: NodeType
    properties: Dict[str, Any]
    confidence: float  # 0.0 to 1.0
    relationships: List[Tuple[str, RelationType, str]] = None  # (source_id, rel_type, target_id)
    
    def __post_init__(self):
        if self.relationships is None:
            self.relationships = []


class LearningExtractor:
    """
    Analyzes task execution results and extracts learnings for persistence.
    """
    
    def __init__(self, graph_db: Optional[Neo4jGraphDB] = None):
        self.graph_db = graph_db
        
        # Regex patterns for extraction
        self.error_pattern = re.compile(r"(error|error:|exception|exception:)(.+?)(?=\n|$)", re.IGNORECASE)
        self.solution_pattern = re.compile(r"(solution:|fix:|resolved|fixed|fixed by|use|try)(.+?)(?=\n|$)", re.IGNORECASE)
        self.language_pattern = re.compile(r"\b(python|javascript|go|java|rust|php|typescript|c\#|ruby|kotlin)\b", re.IGNORECASE)
        self.skill_pattern = re.compile(r"\b(asyncio|goroutine|concurrent|parallel|design pattern|refactor|optimization)\b", re.IGNORECASE)
        self.concept_pattern = re.compile(r"\b(microservices|rest api|caching|database|architecture|algorithm|data structure)\b", re.IGNORECASE)
    
    async def extract_from_task_result(
        self,
        task_id: str,
        task_title: str,
        result_text: str,
        project_id: Optional[str] = None,
        user_id: str = "JARVIS",
    ) -> List[ExtractedLearning]:
        """
        Extract learnings from task execution results.
        
        Args:
            task_id: Unique task identifier
            task_title: Task title/description
            result_text: Full task execution result
            project_id: Optional project context
            user_id: User performing the task
        
        Returns:
            List of extracted learnings with confidence scores
        """
        learnings = []
        
        logger.debug(f"🔍 Extracting learnings from task: {task_title}")
        
        # Extract errors and solutions
        error_learnings = self._extract_errors(result_text)
        learnings.extend(error_learnings)
        
        # Extract technologies used
        tech_learnings = self._extract_technologies(result_text)
        learnings.extend(tech_learnings)
        
        # Extract skills applied
        skill_learnings = self._extract_skills(result_text)
        learnings.extend(skill_learnings)
        
        # Extract concepts
        concept_learnings = self._extract_concepts(result_text)
        learnings.extend(concept_learnings)
        
        # Add task as node
        task_learning = await self._create_task_node(
            task_id, task_title, result_text, project_id
        )
        if task_learning:
            learnings.append(task_learning)
        
        # Link learnings together
        learnings = self._link_learnings(learnings, project_id, user_id)
        
        logger.info(f"✅ Extracted {len(learnings)} learnings from task")
        return learnings
    
    def _extract_errors(self, result_text: str) -> List[ExtractedLearning]:
        """Extract error nodes from result text"""
        learnings = []
        
        matches = self.error_pattern.findall(result_text)
        for match in matches:
            error_text = match[1].strip() if len(match) > 1 else match[0]
            if len(error_text) < 3:
                continue
            
            # Detect language
            language = self._detect_language(error_text)
            
            # Create error node
            learning = ExtractedLearning(
                type=NodeType.ERROR,
                properties={
                    "type": error_text[:50],
                    "language": language,
                    "description": error_text,
                    "extracted_at": datetime.now().isoformat(),
                },
                confidence=0.7
            )
            learnings.append(learning)
        
        return learnings
    
    def _extract_technologies(self, result_text: str) -> List[ExtractedLearning]:
        """Extract technology/framework mentions"""
        learnings = []
        
        languages = set()
        for match in self.language_pattern.finditer(result_text):
            lang = match.group(1).lower()
            languages.add(lang)
        
        for lang in languages:
            learning = ExtractedLearning(
                type=NodeType.SKILL,
                properties={
                    "name": lang.capitalize(),
                    "category": "programming_language",
                    "proficiency": 0.5,  # Default to middle for discovered skills
                    "extracted_at": datetime.now().isoformat(),
                },
                confidence=0.6
            )
            learnings.append(learning)
        
        return learnings
    
    def _extract_skills(self, result_text: str) -> List[ExtractedLearning]:
        """Extract skill mentions"""
        learnings = []
        
        skills = set()
        for match in self.skill_pattern.finditer(result_text):
            skill = match.group(1).lower()
            skills.add(skill)
        
        for skill in skills:
            learning = ExtractedLearning(
                type=NodeType.SKILL,
                properties={
                    "name": skill.replace("_", " ").title(),
                    "category": "technique",
                    "proficiency": 0.6,
                    "extracted_at": datetime.now().isoformat(),
                },
                confidence=0.7
            )
            learnings.append(learning)
        
        return learnings
    
    def _extract_concepts(self, result_text: str) -> List[ExtractedLearning]:
        """Extract concept mentions"""
        learnings = []
        
        concepts = set()
        for match in self.concept_pattern.finditer(result_text):
            concept = match.group(1).lower()
            concepts.add(concept)
        
        for concept in concepts:
            learning = ExtractedLearning(
                type=NodeType.CONCEPT,
                properties={
                    "name": concept.replace("_", " ").title(),
                    "domain": "software_engineering",
                    "extracted_at": datetime.now().isoformat(),
                },
                confidence=0.65
            )
            learnings.append(learning)
        
        return learnings
    
    async def _create_task_node(
        self,
        task_id: str,
        task_title: str,
        result_text: str,
        project_id: Optional[str] = None,
    ) -> Optional[ExtractedLearning]:
        """Create a task node with metadata"""
        learning = ExtractedLearning(
            type=NodeType.TASK,
            properties={
                "title": task_title,
                "description": result_text[:500],  # Truncate long results
                "status": "completed",
                "completed_at": datetime.now().isoformat(),
                "project_id": project_id or "unknown",
            },
            confidence=1.0
        )
        return learning
    
    def _link_learnings(
        self,
        learnings: List[ExtractedLearning],
        project_id: Optional[str] = None,
        user_id: str = "JARVIS",
    ) -> List[ExtractedLearning]:
        """Add relationships between extracted learnings"""
        # Link skills to concepts
        skills = [l for l in learnings if l.type == NodeType.SKILL]
        concepts = [l for l in learnings if l.type == NodeType.CONCEPT]
        
        for skill in skills:
            for concept in concepts:
                skill.relationships.append((
                    skill.properties.get("name"),
                    RelationType.RELATED_TO,
                    concept.properties.get("name")
                ))
        
        # Link errors to concepts
        errors = [l for l in learnings if l.type == NodeType.ERROR]
        for error in errors:
            for concept in concepts:
                error.relationships.append((
                    error.properties.get("type"),
                    RelationType.RELATED_TO,
                    concept.properties.get("name")
                ))
        
        return learnings
    
    def _detect_language(self, text: str) -> str:
        """Detect programming language in text"""
        text_lower = text.lower()
        
        languages = {
            "python": ["python", "pip", "django", "flask", "asyncio"],
            "javascript": ["javascript", "js", "node", "npm", "async/await"],
            "go": ["go", "golang", "goroutine", "defer"],
            "java": ["java", "jvm", "spring", "maven"],
            "rust": ["rust", "cargo", "unsafe"],
            "php": ["php", "laravel", "composer"],
            "typescript": ["typescript", "ts", "interface", "type"],
        }
        
        for lang, keywords in languages.items():
            if any(kw in text_lower for kw in keywords):
                return lang.capitalize()
        
        return "Unknown"
    
    async def persist_learnings(
        self,
        learnings: List[ExtractedLearning],
        task_id: str,
    ) -> int:
        """
        Persist extracted learnings to graph database.
        
        Returns:
            Number of learnings persisted
        """
        if not self.graph_db:
            self.graph_db = await get_graph_db()
        
        persisted_count = 0
        node_id_map = {}  # Map from property name to created node ID
        
        # Create nodes
        for learning in learnings:
            try:
                # Deduplicate by checking if node already exists
                existing = await self.graph_db.find_nodes(
                    learning.type,
                    {"name": learning.properties.get("name")},
                    limit=1
                )
                
                if existing:
                    node_id = existing[0].get("node_id", existing[0].get("name"))
                    node_id_map[learning.properties.get("name")] = node_id
                else:
                    node_id = await self.graph_db.create_node(
                        learning.type,
                        learning.properties
                    )
                    if node_id:
                        node_id_map[learning.properties.get("name")] = node_id
                        persisted_count += 1
            except Exception as e:
                logger.warning(f"Failed to persist learning: {e}")
        
        # Create relationships
        for learning in learnings:
            source_id = node_id_map.get(learning.properties.get("name"))
            if not source_id:
                continue
            
            for source_name, rel_type, target_name in learning.relationships:
                target_id = node_id_map.get(target_name)
                if not target_id:
                    continue
                
                try:
                    await self.graph_db.create_relationship(
                        source_id,
                        rel_type,
                        target_id,
                        {"created_from_task": task_id}
                    )
                except Exception as e:
                    logger.warning(f"Failed to create relationship: {e}")
        
        logger.info(f"✅ Persisted {persisted_count} learnings to graph")
        return persisted_count


async def extract_and_persist_learnings(
    task_id: str,
    task_title: str,
    result_text: str,
    project_id: Optional[str] = None,
    user_id: str = "JARVIS",
) -> int:
    """
    Convenience function to extract and persist learnings in one call.
    
    Returns:
        Number of learnings persisted
    """
    extractor = LearningExtractor()
    learnings = await extractor.extract_from_task_result(
        task_id, task_title, result_text, project_id, user_id
    )
    
    persisted = await extractor.persist_learnings(learnings, task_id)
    return persisted


__all__ = [
    "LearningExtractor",
    "ExtractedLearning",
    "extract_and_persist_learnings",
]
