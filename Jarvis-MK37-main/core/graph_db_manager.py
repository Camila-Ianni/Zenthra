"""
Neo4j Graph Database Manager for JARVIS-MK37

Manages connections, queries, and data persistence to Neo4j.
Includes automatic fallback to JSON if Neo4j is unavailable.

Features:
  - Async connection pooling
  - Cypher query builders
  - Transaction management
  - JSON fallback mode
  - Schema initialization
"""

import asyncio
import json
import logging
from pathlib import Path
from typing import Optional, Dict, Any, List, Tuple
from contextlib import asynccontextmanager

try:
    from neo4j import AsyncDriver, AsyncSession, ManagedTransaction
    from neo4j.exceptions import DriverError, CypherSyntaxError
    NEO4J_AVAILABLE = True
except ImportError:
    NEO4J_AVAILABLE = False

from core.graph_schema import (
    NodeType, RelationType, NodeConstraint, GraphSchemaValidator,
    GRAPH_INIT_QUERIES
)


logger = logging.getLogger(__name__)


class Neo4jGraphDB:
    """
    Async Neo4j graph database client with fallback to JSON.
    
    If Neo4j is unavailable, automatically falls back to JSON persistence
    stored in memory/graph_fallback.json
    """
    
    def __init__(
        self,
        uri: str = "bolt://localhost:7687",
        username: str = "neo4j",
        password: str = "password",
        fallback_path: Optional[Path] = None,
        max_pool_size: int = 50,
    ):
        self.uri = uri
        self.username = username
        self.password = password
        self.fallback_path = fallback_path or Path(__file__).parent.parent / "memory" / "graph_fallback.json"
        self.max_pool_size = max_pool_size
        
        self.driver: Optional[AsyncDriver] = None
        self.is_neo4j_available = False
        self.use_fallback = False
        self._fallback_data: Dict[str, Any] = {
            "nodes": {},
            "relationships": [],
            "metadata": {"created_at": None},
        }
    
    async def connect(self) -> bool:
        """
        Connect to Neo4j database. Falls back to JSON if unavailable.
        
        Returns:
            bool: True if connected (Neo4j or fallback), False on failure
        """
        if not NEO4J_AVAILABLE:
            logger.warning("neo4j library not installed. Using JSON fallback.")
            await self._load_fallback_data()
            self.use_fallback = True
            return True
        
        try:
            from neo4j import AsyncGraphDatabase
            
            self.driver = AsyncGraphDatabase.driver(
                self.uri,
                auth=(self.username, self.password),
                max_pool_size=self.max_pool_size,
            )
            
            # Test connection
            async with self.driver.session() as session:
                await session.run("RETURN 1")
            
            self.is_neo4j_available = True
            self.use_fallback = False
            logger.info(f"✅ Connected to Neo4j at {self.uri}")
            
            # Initialize schema and constraints
            await self._init_schema()
            return True
            
        except (DriverError, Exception) as e:
            logger.warning(f"⚠️ Neo4j connection failed: {e}. Using JSON fallback.")
            await self._load_fallback_data()
            self.use_fallback = True
            self.is_neo4j_available = False
            return True
    
    async def disconnect(self) -> None:
        """Close Neo4j connection and save fallback data"""
        if self.driver:
            await self.driver.close()
            self.driver = None
        await self._save_fallback_data()
    
    async def _init_schema(self) -> None:
        """Initialize database schema, constraints, and indexes"""
        if not self.is_neo4j_available or not self.driver:
            return
        
        async with self.driver.session() as session:
            try:
                # Create constraints
                for node_type, constraints in NodeConstraint.get_constraints().items():
                    for constraint in constraints:
                        try:
                            await session.run(constraint)
                            logger.debug(f"✓ Created constraint for {node_type}")
                        except Exception as e:
                            logger.debug(f"Constraint already exists or error: {e}")
                
                # Create indexes
                for index in NodeConstraint.get_indexes():
                    try:
                        await session.run(index)
                        logger.debug(f"✓ Created index")
                    except Exception:
                        pass  # Index may already exist
                
                # Initialize JARVIS user node
                for query in GRAPH_INIT_QUERIES:
                    await session.run(query)
                
                logger.info("✅ Graph schema initialized")
                
            except Exception as e:
                logger.warning(f"⚠️ Schema initialization error: {e}")
    
    @asynccontextmanager
    async def transaction(self):
        """Context manager for transactions"""
        if self.use_fallback:
            yield None
            return
        
        if not self.driver:
            raise RuntimeError("Database not connected")
        
        async with self.driver.session() as session:
            async with session.begin_transaction() as tx:
                yield tx
    
    async def create_node(
        self,
        node_type: NodeType,
        properties: Dict[str, Any],
    ) -> Optional[str]:
        """
        Create a node in the graph.
        
        Args:
            node_type: Type of node (User, Project, etc.)
            properties: Node properties
        
        Returns:
            Node ID, or None on error
        """
        # Validate
        is_valid, error_msg = GraphSchemaValidator.validate_node_data(node_type, properties)
        if not is_valid:
            logger.error(f"Invalid node data: {error_msg}")
            return None
        
        # Create unique identifier
        node_id = properties.get("node_id") or properties.get("name", "unknown")
        properties["node_id"] = node_id
        
        if self.use_fallback:
            self._fallback_data["nodes"][node_id] = {
                "type": node_type.value,
                **properties,
            }
            return node_id
        
        try:
            async with self.driver.session() as session:
                query = f"""
                CREATE (n:{node_type.value})
                SET n = $props
                RETURN n.node_id as id
                """
                result = await session.run(query, props=properties)
                record = await result.single()
                return record["id"] if record else None
        except Exception as e:
            logger.error(f"Failed to create node: {e}")
            return None
    
    async def create_relationship(
        self,
        source_id: str,
        rel_type: RelationType,
        target_id: str,
        properties: Optional[Dict[str, Any]] = None,
    ) -> bool:
        """
        Create a relationship between two nodes.
        
        Args:
            source_id: Source node ID
            rel_type: Relationship type
            target_id: Target node ID
            properties: Optional relationship properties
        
        Returns:
            bool: Success status
        """
        properties = properties or {}
        properties["created_at"] = properties.get("created_at", str(asyncio.get_event_loop().time()))
        
        if self.use_fallback:
            self._fallback_data["relationships"].append({
                "source": source_id,
                "type": rel_type.value,
                "target": target_id,
                **properties,
            })
            return True
        
        try:
            async with self.driver.session() as session:
                query = f"""
                MATCH (s {{node_id: $source}})
                MATCH (t {{node_id: $target}})
                CREATE (s)-[r:{rel_type.value}]->(t)
                SET r = $props
                RETURN COUNT(r) as count
                """
                result = await session.run(query, source=source_id, target=target_id, props=properties)
                record = await result.single()
                return record["count"] > 0 if record else False
        except Exception as e:
            logger.error(f"Failed to create relationship: {e}")
            return False
    
    async def query(self, cypher: str, parameters: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
        """
        Execute a Cypher query.
        
        Args:
            cypher: Cypher query string
            parameters: Query parameters
        
        Returns:
            List of result records
        """
        parameters = parameters or {}
        
        if self.use_fallback:
            logger.warning(f"Query not supported in fallback mode: {cypher[:50]}...")
            return []
        
        try:
            async with self.driver.session() as session:
                result = await session.run(cypher, parameters)
                records = await result.fetch(1000)  # Limit to 1000 results
                return [dict(record) for record in records]
        except (CypherSyntaxError, Exception) as e:
            logger.error(f"Query error: {e}")
            return []
    
    async def find_nodes(
        self,
        node_type: NodeType,
        properties: Optional[Dict[str, Any]] = None,
        limit: int = 100,
    ) -> List[Dict[str, Any]]:
        """Find nodes by type and optional properties"""
        properties = properties or {}
        
        if self.use_fallback:
            results = []
            for node_id, node in self._fallback_data["nodes"].items():
                if node.get("type") == node_type.value:
                    if all(node.get(k) == v for k, v in properties.items()):
                        results.append(node)
                        if len(results) >= limit:
                            break
            return results
        
        try:
            where_clauses = " AND ".join([f"n.{k} = ${k}" for k in properties.keys()])
            where_clause = f"WHERE {where_clauses}" if where_clauses else ""
            
            query = f"""
            MATCH (n:{node_type.value})
            {where_clause}
            RETURN n
            LIMIT {limit}
            """
            
            return await self.query(query, properties)
        except Exception as e:
            logger.error(f"Find nodes error: {e}")
            return []
    
    async def find_relationships(
        self,
        rel_type: RelationType,
        source_type: Optional[NodeType] = None,
        target_type: Optional[NodeType] = None,
    ) -> List[Dict[str, Any]]:
        """Find relationships by type"""
        if self.use_fallback:
            results = []
            for rel in self._fallback_data["relationships"]:
                if rel.get("type") == rel_type.value:
                    results.append(rel)
            return results
        
        try:
            source_label = f":{source_type.value}" if source_type else ""
            target_label = f":{target_type.value}" if target_type else ""
            
            query = f"""
            MATCH (s{source_label})-[r:{rel_type.value}]->(t{target_label})
            RETURN s, r, t
            LIMIT 100
            """
            
            return await self.query(query)
        except Exception as e:
            logger.error(f"Find relationships error: {e}")
            return []
    
    async def delete_node(self, node_id: str) -> bool:
        """Delete a node and its relationships"""
        if self.use_fallback:
            if node_id in self._fallback_data["nodes"]:
                del self._fallback_data["nodes"][node_id]
                self._fallback_data["relationships"] = [
                    r for r in self._fallback_data["relationships"]
                    if r["source"] != node_id and r["target"] != node_id
                ]
                return True
            return False
        
        try:
            async with self.driver.session() as session:
                query = """
                MATCH (n {node_id: $id})
                DETACH DELETE n
                RETURN COUNT(n) as count
                """
                result = await session.run(query, id=node_id)
                record = await result.single()
                return record["count"] > 0 if record else False
        except Exception as e:
            logger.error(f"Delete node error: {e}")
            return False
    
    async def _load_fallback_data(self) -> None:
        """Load fallback JSON data"""
        if self.fallback_path.exists():
            try:
                with open(self.fallback_path, 'r') as f:
                    self._fallback_data = json.load(f)
                logger.info(f"✅ Loaded fallback data from {self.fallback_path}")
            except Exception as e:
                logger.warning(f"Could not load fallback data: {e}")
    
    async def _save_fallback_data(self) -> None:
        """Save fallback JSON data"""
        try:
            self.fallback_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.fallback_path, 'w') as f:
                json.dump(self._fallback_data, f, indent=2)
            logger.debug(f"✅ Saved fallback data to {self.fallback_path}")
        except Exception as e:
            logger.warning(f"Could not save fallback data: {e}")
    
    async def get_stats(self) -> Dict[str, Any]:
        """Get graph statistics"""
        if self.use_fallback:
            return {
                "mode": "fallback",
                "nodes": len(self._fallback_data["nodes"]),
                "relationships": len(self._fallback_data["relationships"]),
            }
        
        try:
            async with self.driver.session() as session:
                stats = {}
                
                # Count nodes by type
                for node_type in NodeType:
                    result = await session.run(
                        f"MATCH (n:{node_type.value}) RETURN COUNT(n) as count"
                    )
                    record = await result.single()
                    if record:
                        stats[f"{node_type.value}_count"] = record["count"]
                
                # Count relationships
                result = await session.run("MATCH (a)-[r]->(b) RETURN COUNT(r) as count")
                record = await result.single()
                if record:
                    stats["relationships"] = record["count"]
                
                stats["mode"] = "neo4j"
                return stats
        except Exception as e:
            logger.error(f"Get stats error: {e}")
            return {"error": str(e)}


# Global instance
_graph_db: Optional[Neo4jGraphDB] = None


async def get_graph_db() -> Neo4jGraphDB:
    """Get or create global graph database instance"""
    global _graph_db
    if _graph_db is None:
        _graph_db = Neo4jGraphDB()
        await _graph_db.connect()
    return _graph_db


async def close_graph_db() -> None:
    """Close global graph database connection"""
    global _graph_db
    if _graph_db:
        await _graph_db.disconnect()
        _graph_db = None


__all__ = [
    "Neo4jGraphDB",
    "get_graph_db",
    "close_graph_db",
]
