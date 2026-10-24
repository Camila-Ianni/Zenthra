"""
Knowledge Graph for OpenMythos
Persistent storage of learned patterns, decisions, and architecture knowledge
Enables cross-session learning and context reuse

Compatible with Python 3.9+
"""

import json
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from typing import Dict, List, Optional, Any, Set, Tuple
from uuid import uuid4
import os

logger = logging.getLogger(__name__)


class NodeType(Enum):
    """Types of nodes in knowledge graph"""
    CODE = "code"
    PATTERN = "pattern"
    DECISION = "decision"
    ERROR = "error"
    DISCOVERY = "discovery"
    DEPENDENCY = "dependency"
    ARCHITECTURE = "architecture"


class RelationType(Enum):
    """Types of relations between nodes"""
    DEPENDS_ON = "depends_on"
    IMPLEMENTS = "implements"
    USES = "uses"
    CONFLICTS_WITH = "conflicts_with"
    IMPROVES = "improves"
    RELATED_TO = "related_to"
    PART_OF = "part_of"


@dataclass
class KnowledgeNode:
    """Single node in knowledge graph"""
    id: str = field(default_factory=lambda: str(uuid4())[:8])
    node_type: NodeType = NodeType.DISCOVERY
    label: str = ""
    description: str = ""
    content: Dict[str, Any] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: str = field(default_factory=lambda: datetime.now().isoformat())
    updated_at: str = field(default_factory=lambda: datetime.now().isoformat())
    relevance_score: float = 1.0
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return {
            "id": self.id,
            "node_type": self.node_type.value,
            "label": self.label,
            "description": self.description,
            "content": self.content,
            "metadata": self.metadata,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "relevance_score": self.relevance_score,
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'KnowledgeNode':
        """Deserialize from dictionary"""
        data_copy = data.copy()
        if isinstance(data_copy.get('node_type'), str):
            data_copy['node_type'] = NodeType(data_copy['node_type'])
        return cls(**data_copy)


@dataclass
class KnowledgeRelation:
    """Relation between two nodes"""
    source_id: str
    target_id: str
    relation_type: RelationType = RelationType.RELATED_TO
    confidence: float = 0.8
    weight: float = 1.0
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: str = field(default_factory=lambda: datetime.now().isoformat())
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return {
            "source_id": self.source_id,
            "target_id": self.target_id,
            "relation_type": self.relation_type.value,
            "confidence": self.confidence,
            "weight": self.weight,
            "metadata": self.metadata,
            "created_at": self.created_at,
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'KnowledgeRelation':
        """Deserialize from dictionary"""
        data_copy = data.copy()
        if isinstance(data_copy.get('relation_type'), str):
            data_copy['relation_type'] = RelationType(data_copy['relation_type'])
        return cls(**data_copy)


class KnowledgeGraph:
    """Persistent knowledge graph for learning across sessions"""
    
    def __init__(self, storage_path: str = "memory/knowledge_graph.json"):
        self.storage_path = storage_path
        self.nodes: Dict[str, KnowledgeNode] = {}
        self.relations: List[KnowledgeRelation] = []
        self.indexes: Dict[str, Set[str]] = {
            "by_type": {},
            "by_label": {},
        }
        self._load_from_storage()
    
    def add_node(
        self,
        label: str,
        node_type: NodeType,
        description: str = "",
        content: Optional[Dict[str, Any]] = None,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> KnowledgeNode:
        """Add a node to the graph"""
        node = KnowledgeNode(
            node_type=node_type,
            label=label,
            description=description,
            content=content or {},
            metadata=metadata or {},
        )
        
        self.nodes[node.id] = node
        
        self._update_indexes(node)
        logger.info(f"Added node {node.id}: {label} ({node_type.value})")
        
        return node
    
    def add_relation(
        self,
        source_id: str,
        target_id: str,
        relation_type: RelationType = RelationType.RELATED_TO,
        confidence: float = 0.8,
        weight: float = 1.0,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Optional[KnowledgeRelation]:
        """Add a relation between two nodes"""
        if source_id not in self.nodes or target_id not in self.nodes:
            logger.warning(f"Cannot add relation: nodes not found")
            return None
        
        relation = KnowledgeRelation(
            source_id=source_id,
            target_id=target_id,
            relation_type=relation_type,
            confidence=confidence,
            weight=weight,
            metadata=metadata or {},
        )
        
        self.relations.append(relation)
        logger.info(f"Added relation: {source_id} -{relation_type.value}-> {target_id}")
        
        return relation
    
    def get_node(self, node_id: str) -> Optional[KnowledgeNode]:
        """Retrieve a node by ID"""
        return self.nodes.get(node_id)
    
    def search_by_label(self, label: str) -> List[KnowledgeNode]:
        """Search nodes by label (substring match)"""
        matching_ids = self.indexes["by_label"].get(label.lower(), set())
        return [self.nodes[nid] for nid in matching_ids if nid in self.nodes]
    
    def search_by_type(self, node_type: NodeType) -> List[KnowledgeNode]:
        """Get all nodes of a specific type"""
        type_key = node_type.value
        matching_ids = self.indexes["by_type"].get(type_key, set())
        return [self.nodes[nid] for nid in matching_ids if nid in self.nodes]
    
    def get_related_nodes(
        self,
        node_id: str,
        relation_type: Optional[RelationType] = None,
        depth: int = 1,
    ) -> List[Tuple[KnowledgeNode, KnowledgeRelation, int]]:
        """Get nodes related to a given node (with BFS traversal)"""
        if node_id not in self.nodes:
            return []
        
        visited = {node_id}
        queue = [(node_id, 0)]
        results = []
        
        while queue:
            current_id, current_depth = queue.pop(0)
            
            if current_depth >= depth:
                continue
            
            for rel in self.relations:
                if rel.source_id == current_id:
                    if relation_type and rel.relation_type != relation_type:
                        continue
                    
                    target_node = self.nodes.get(rel.target_id)
                    if target_node:
                        results.append((target_node, rel, current_depth + 1))
                        
                        if rel.target_id not in visited:
                            visited.add(rel.target_id)
                            queue.append((rel.target_id, current_depth + 1))
        
        return results
    
    def find_patterns(self) -> Dict[str, List[KnowledgeNode]]:
        """Find common patterns in the graph"""
        patterns = {}
        pattern_nodes = self.search_by_type(NodeType.PATTERN)
        
        for node in pattern_nodes:
            pattern_name = node.label
            if pattern_name not in patterns:
                patterns[pattern_name] = []
            patterns[pattern_name].append(node)
        
        return patterns
    
    def get_architecture_decisions(self) -> List[KnowledgeNode]:
        """Get all recorded architecture decisions"""
        return self.search_by_type(NodeType.ARCHITECTURE)
    
    def get_error_history(self) -> List[KnowledgeNode]:
        """Get all recorded errors for learning"""
        return self.search_by_type(NodeType.ERROR)
    
    def prune_old_nodes(self, days: int = 30) -> int:
        """Remove nodes older than specified days (optional optimization)"""
        cutoff_date = datetime.now() - timedelta(days=days)
        nodes_to_remove = []
        
        for node_id, node in self.nodes.items():
            try:
                node_date = datetime.fromisoformat(node.created_at)
                if node_date < cutoff_date:
                    nodes_to_remove.append(node_id)
            except ValueError:
                pass
        
        for node_id in nodes_to_remove:
            del self.nodes[node_id]
            self.relations = [
                r for r in self.relations
                if r.source_id != node_id and r.target_id != node_id
            ]
        
        if nodes_to_remove:
            logger.info(f"Pruned {len(nodes_to_remove)} old nodes")
        
        return len(nodes_to_remove)
    
    def get_stats(self) -> Dict[str, Any]:
        """Get graph statistics"""
        type_counts = {}
        for node_type in NodeType:
            type_counts[node_type.value] = len(self.search_by_type(node_type))
        
        relation_counts = {}
        for rel_type in RelationType:
            count = sum(1 for r in self.relations if r.relation_type == rel_type)
            relation_counts[rel_type.value] = count
        
        return {
            "total_nodes": len(self.nodes),
            "total_relations": len(self.relations),
            "node_types": type_counts,
            "relation_types": relation_counts,
            "avg_node_relevance": (
                sum(n.relevance_score for n in self.nodes.values()) / len(self.nodes)
                if self.nodes else 0.0
            ),
        }
    
    def save_to_storage(self) -> None:
        """Persist graph to JSON file"""
        os.makedirs(os.path.dirname(self.storage_path) or ".", exist_ok=True)
        
        data = {
            "nodes": [n.to_dict() for n in self.nodes.values()],
            "relations": [r.to_dict() for r in self.relations],
            "last_updated": datetime.now().isoformat(),
        }
        
        with open(self.storage_path, 'w') as f:
            json.dump(data, f, indent=2, default=str)
        
        logger.info(f"Saved knowledge graph to {self.storage_path}")
    
    def _load_from_storage(self) -> None:
        """Load graph from JSON file if it exists"""
        if not os.path.exists(self.storage_path):
            logger.info(f"Knowledge graph storage not found: {self.storage_path}")
            return
        
        try:
            with open(self.storage_path, 'r') as f:
                data = json.load(f)
            
            for node_data in data.get("nodes", []):
                node = KnowledgeNode.from_dict(node_data)
                self.nodes[node.id] = node
                self._update_indexes(node)
            
            for rel_data in data.get("relations", []):
                relation = KnowledgeRelation.from_dict(rel_data)
                self.relations.append(relation)
            
            logger.info(f"Loaded knowledge graph with {len(self.nodes)} nodes and {len(self.relations)} relations")
        except Exception as e:
            logger.error(f"Error loading knowledge graph: {str(e)}")
    
    def _update_indexes(self, node: KnowledgeNode) -> None:
        """Update internal indexes for fast lookup"""
        type_key = node.node_type.value
        if type_key not in self.indexes["by_type"]:
            self.indexes["by_type"][type_key] = set()
        self.indexes["by_type"][type_key].add(node.id)
        
        label_key = node.label.lower()
        if label_key not in self.indexes["by_label"]:
            self.indexes["by_label"][label_key] = set()
        self.indexes["by_label"][label_key].add(node.id)
    
    def export_to_dot(self, filepath: str) -> None:
        """Export graph to Graphviz DOT format (optional visualization)"""
        with open(filepath, 'w') as f:
            f.write("digraph KnowledgeGraph {\n")
            f.write('  rankdir=LR;\n')
            f.write('  node [shape=box];\n')
            
            for node in self.nodes.values():
                label = f"{node.label}\\n({node.node_type.value})"
                f.write(f'  "{node.id}" [label="{label}"];\n')
            
            for rel in self.relations:
                f.write(f'  "{rel.source_id}" -> "{rel.target_id}" [label="{rel.relation_type.value}"];\n')
            
            f.write("}\n")
        
        logger.info(f"Exported knowledge graph to {filepath}")
