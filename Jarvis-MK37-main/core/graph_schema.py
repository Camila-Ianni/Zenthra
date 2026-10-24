"""
Graph Schema Definition for Neo4j

Defines node types, relationship types, constraints, and validation
for JARVIS-MK37 knowledge graph.

Node Types:
  - User: JARVIS AI system identity
  - Project: Code projects (gatitos, API backend, etc.)
  - Skill: Programming skills (Go, React, Laravel, etc.)
  - Task: Completed work items (fix bug, implement feature)
  - Concept: CS concepts (Observer Pattern, Goroutines, MVC)
  - Subject: Faculty subjects (Sistemas Operativos, Business)
  - Error: Common errors and solutions (memory leak, race condition)

Relationships:
  - STUDIES: User studies Subject
  - WORKS_ON: User works on Project
  - REQUIRES: Project/Task requires Concept
  - LEARNED_IN: User learned Concept in/from Subject
  - HAS_SKILL: User possesses Skill
  - USES_SKILL: Project uses Skill
  - RELATED_TO: Concept/Error related to another
  - SOLVES: Task solves Error
"""

from enum import Enum
from typing import Optional, List, Dict, Any
from dataclasses import dataclass, asdict
from datetime import datetime


class NodeType(Enum):
    """Neo4j node labels"""
    USER = "User"
    PROJECT = "Project"
    SKILL = "Skill"
    TASK = "Task"
    CONCEPT = "Concept"
    SUBJECT = "Subject"
    ERROR = "Error"


class RelationType(Enum):
    """Neo4j relationship types"""
    STUDIES = "STUDIES"
    WORKS_ON = "WORKS_ON"
    REQUIRES = "REQUIRES"
    LEARNED_IN = "LEARNED_IN"
    HAS_SKILL = "HAS_SKILL"
    USES_SKILL = "USES_SKILL"
    RELATED_TO = "RELATED_TO"
    SOLVES = "SOLVES"
    INVOLVES_SKILL = "INVOLVES_SKILL"
    TEACHES = "TEACHES"
    REQUIRES_SKILL = "REQUIRES_SKILL"


class NodeConstraint:
    """Defines constraints for Neo4j nodes"""
    
    @staticmethod
    def get_constraints() -> Dict[str, List[str]]:
        """Returns constraints per node type"""
        return {
            NodeType.USER.value: [
                "CREATE CONSTRAINT user_name_unique IF NOT EXISTS ON (u:User) ASSERT u.name IS UNIQUE",
            ],
            NodeType.PROJECT.value: [
                "CREATE CONSTRAINT project_name_unique IF NOT EXISTS ON (p:Project) ASSERT p.name IS UNIQUE",
            ],
            NodeType.SKILL.value: [
                "CREATE CONSTRAINT skill_name_unique IF NOT EXISTS ON (s:Skill) ASSERT s.name IS UNIQUE",
            ],
            NodeType.SUBJECT.value: [
                "CREATE CONSTRAINT subject_code_unique IF NOT EXISTS ON (s:Subject) ASSERT s.code IS UNIQUE",
            ],
            NodeType.CONCEPT.value: [
                "CREATE CONSTRAINT concept_name_unique IF NOT EXISTS ON (c:Concept) ASSERT c.name IS UNIQUE",
            ],
            NodeType.ERROR.value: [
                "CREATE INDEX error_type_language IF NOT EXISTS ON (e:Error) (type, language)",
            ],
            NodeType.TASK.value: [
                "CREATE INDEX task_status IF NOT EXISTS ON (t:Task) (status)",
            ],
        }
    
    @staticmethod
    def get_indexes() -> List[str]:
        """Returns all indexes to create"""
        return [
            "CREATE INDEX user_role IF NOT EXISTS ON (u:User) (role)",
            "CREATE INDEX project_status IF NOT EXISTS ON (p:Project) (status)",
            "CREATE INDEX project_language IF NOT EXISTS ON (p:Project) (language)",
            "CREATE INDEX skill_category IF NOT EXISTS ON (s:Skill) (category)",
            "CREATE INDEX task_completed_date IF NOT EXISTS ON (t:Task) (completed_date)",
            "CREATE INDEX concept_domain IF NOT EXISTS ON (c:Concept) (domain)",
            "CREATE INDEX subject_semester IF NOT EXISTS ON (s:Subject) (semester)",
            "CREATE INDEX error_frequency IF NOT EXISTS ON (e:Error) (frequency)",
        ]


@dataclass
class GraphNode:
    """Base class for graph nodes"""
    node_id: Optional[str] = None
    created_at: Optional[str] = None
    updated_at: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert node to dictionary for Neo4j"""
        data = asdict(self)
        if data['created_at'] is None:
            data['created_at'] = datetime.utcnow().isoformat()
        if data['updated_at'] is None:
            data['updated_at'] = data['created_at']
        return {k: v for k, v in data.items() if v is not None}


@dataclass
class UserNode(GraphNode):
    """User node: JARVIS AI system"""
    name: str = "JARVIS"
    role: str = "AI_ASSISTANT"
    version: str = "MK37"
    description: Optional[str] = None


@dataclass
class ProjectNode(GraphNode):
    """Project node: Code projects"""
    name: str = ""
    language: str = ""
    framework: Optional[str] = None
    status: str = "planning"  # planning, in_progress, completed, paused
    description: Optional[str] = None
    repo_url: Optional[str] = None
    last_updated: Optional[str] = None


@dataclass
class SkillNode(GraphNode):
    """Skill node: Programming skills"""
    name: str = ""
    category: str = ""  # web, systems, ml, mobile, etc.
    proficiency: float = 0.0  # 0.0 to 1.0
    description: Optional[str] = None
    learned_date: Optional[str] = None


@dataclass
class TaskNode(GraphNode):
    """Task node: Completed work items"""
    title: str = ""
    description: Optional[str] = None
    status: str = "pending"  # pending, in_progress, completed, blocked
    duration_minutes: int = 0
    completed_date: Optional[str] = None
    difficulty: int = 1  # 1-5 scale


@dataclass
class ConceptNode(GraphNode):
    """Concept node: Computer science concepts"""
    name: str = ""
    domain: str = ""  # design_patterns, algorithms, data_structures, etc.
    description: Optional[str] = None
    related_languages: Optional[List[str]] = None
    importance: float = 0.5  # 0.0 to 1.0


@dataclass
class SubjectNode(GraphNode):
    """Subject node: Faculty subjects"""
    name: str = ""
    code: str = ""
    semester: int = 0
    professor: Optional[str] = None
    description: Optional[str] = None
    grade: float = 0.0  # 0.0 to 10.0


@dataclass
class ErrorNode(GraphNode):
    """Error node: Common errors and solutions"""
    type: str = ""  # memory_leak, race_condition, null_pointer, etc.
    language: str = ""
    stack_trace: Optional[str] = None
    solution: Optional[str] = None
    frequency: int = 1  # How many times encountered
    severity: int = 1  # 1-5 scale


class GraphRelationship:
    """Represents a relationship in the graph"""
    
    def __init__(
        self,
        rel_type: RelationType,
        source_id: str,
        target_id: str,
        properties: Optional[Dict[str, Any]] = None,
        created_at: Optional[str] = None,
    ):
        self.rel_type = rel_type
        self.source_id = source_id
        self.target_id = target_id
        self.properties = properties or {}
        self.created_at = created_at or datetime.utcnow().isoformat()
    
    def to_cypher_fragment(self) -> tuple:
        """Return (relationship_type, properties) for Cypher query"""
        props = {**self.properties, "created_at": self.created_at}
        return (self.rel_type.value, props)


class GraphSchemaValidator:
    """Validates graph operations against schema"""
    
    @staticmethod
    def validate_node_type(node_type: NodeType) -> bool:
        """Validate that node type is defined"""
        return node_type in NodeType.__members__.values()
    
    @staticmethod
    def validate_relationship_type(rel_type: RelationType) -> bool:
        """Validate that relationship type is defined"""
        return rel_type in RelationType.__members__.values()
    
    @staticmethod
    def validate_relationship_direction(
        source_type: NodeType,
        rel_type: RelationType,
        target_type: NodeType,
    ) -> bool:
        """Validate relationship direction is allowed"""
        allowed_relationships = {
            # (source, rel_type, target)
            (NodeType.USER, RelationType.STUDIES, NodeType.SUBJECT): True,
            (NodeType.USER, RelationType.WORKS_ON, NodeType.PROJECT): True,
            (NodeType.USER, RelationType.LEARNED_IN, NodeType.CONCEPT): True,
            (NodeType.USER, RelationType.HAS_SKILL, NodeType.SKILL): True,
            
            (NodeType.PROJECT, RelationType.REQUIRES, NodeType.CONCEPT): True,
            (NodeType.PROJECT, RelationType.USES_SKILL, NodeType.SKILL): True,
            (NodeType.PROJECT, RelationType.RELATED_TO, NodeType.PROJECT): True,
            
            (NodeType.TASK, RelationType.SOLVES, NodeType.ERROR): True,
            (NodeType.TASK, RelationType.LEARNED_IN, NodeType.CONCEPT): True,
            (NodeType.TASK, RelationType.INVOLVES_SKILL, NodeType.SKILL): True,
            
            (NodeType.CONCEPT, RelationType.RELATED_TO, NodeType.CONCEPT): True,
            (NodeType.CONCEPT, RelationType.RELATED_TO, NodeType.ERROR): True,
            
            (NodeType.ERROR, RelationType.RELATED_TO, NodeType.ERROR): True,
            
            (NodeType.SUBJECT, RelationType.TEACHES, NodeType.CONCEPT): True,
            (NodeType.SUBJECT, RelationType.REQUIRES_SKILL, NodeType.SKILL): True,
        }
        
        key = (source_type, rel_type, target_type)
        return allowed_relationships.get(key, False)
    
    @staticmethod
    def validate_node_data(node_type: NodeType, data: Dict[str, Any]) -> tuple[bool, Optional[str]]:
        """Validate node data matches schema. Returns (is_valid, error_message)"""
        
        # Check required fields per node type
        required_fields = {
            NodeType.USER: ["name"],
            NodeType.PROJECT: ["name", "language"],
            NodeType.SKILL: ["name", "category"],
            NodeType.TASK: ["title"],
            NodeType.CONCEPT: ["name", "domain"],
            NodeType.SUBJECT: ["name", "code"],
            NodeType.ERROR: ["type", "language"],
        }
        
        required = required_fields.get(node_type, [])
        for field in required:
            if field not in data or data[field] is None:
                return False, f"Missing required field: {field}"
        
        # Type-specific validation
        if node_type == NodeType.SKILL:
            proficiency = data.get("proficiency", 0.0)
            if not 0.0 <= proficiency <= 1.0:
                return False, "Skill proficiency must be 0.0-1.0"
        
        if node_type == NodeType.SUBJECT:
            grade = data.get("grade", 0.0)
            if not 0.0 <= grade <= 10.0:
                return False, "Subject grade must be 0.0-10.0"
        
        if node_type == NodeType.CONCEPT:
            importance = data.get("importance", 0.5)
            if not 0.0 <= importance <= 1.0:
                return False, "Concept importance must be 0.0-1.0"
        
        if node_type == NodeType.ERROR:
            frequency = data.get("frequency", 1)
            severity = data.get("severity", 1)
            if frequency < 1:
                return False, "Error frequency must be >= 1"
            if not 1 <= severity <= 5:
                return False, "Error severity must be 1-5"
        
        return True, None


# Graph initialization template (used by graph_db_manager)
GRAPH_INIT_QUERIES = [
    # Create JARVIS user node
    f"""
    MERGE (u:User {{name: 'JARVIS'}})
    SET u.role = 'AI_ASSISTANT',
        u.version = 'MK37',
        u.created_at = datetime.transaction()
    """,
]

# Export all schemas
__all__ = [
    "NodeType",
    "RelationType",
    "NodeConstraint",
    "GraphNode",
    "UserNode",
    "ProjectNode",
    "SkillNode",
    "TaskNode",
    "ConceptNode",
    "SubjectNode",
    "ErrorNode",
    "GraphRelationship",
    "GraphSchemaValidator",
    "GRAPH_INIT_QUERIES",
]
