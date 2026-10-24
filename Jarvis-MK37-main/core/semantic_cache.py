"""
Phase 9 Pillar 1: Semantic Cache

Local vector database for semantic query caching using ChromaDB.
Enables <50ms responses for repeated or similar queries.

Key Features:
- Embedding generation via all-MiniLM-L6-v2
- Cosine similarity search (threshold: 0.92)
- Persistent storage in memory/vector_cache/
- TTL-based cache invalidation
- Automatic cleanup of expired entries
"""

import logging
import json
import asyncio
from datetime import datetime, timedelta
from typing import Optional, Dict, Any, List, Tuple
from dataclasses import dataclass, field, asdict
from pathlib import Path
import hashlib

try:
    import chromadb
    from chromadb.config import Settings
    CHROMADB_AVAILABLE = True
except ImportError:
    CHROMADB_AVAILABLE = False

try:
    from sentence_transformers import SentenceTransformer
    SENTENCE_TRANSFORMERS_AVAILABLE = True
except ImportError:
    SENTENCE_TRANSFORMERS_AVAILABLE = False


logger = logging.getLogger(__name__)


@dataclass
class CacheEntry:
    """Cached query-response pair with metadata"""
    query_hash: str
    query_text: str
    response: str
    context: Dict[str, Any]
    embedding: Optional[List[float]] = None
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    ttl_minutes: int = 60
    hit_count: int = 0
    
    def is_expired(self) -> bool:
        """Check if entry has expired"""
        ts = datetime.fromisoformat(self.timestamp)
        expiry = ts + timedelta(minutes=self.ttl_minutes)
        return datetime.now() > expiry
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return asdict(self)
    
    @staticmethod
    def from_dict(data: Dict[str, Any]) -> 'CacheEntry':
        """Create from dictionary"""
        return CacheEntry(**data)


class EmbedderService:
    """Service for generating embeddings locally"""
    
    def __init__(self, model_name: str = "all-MiniLM-L6-v2"):
        self.model_name = model_name
        self.model = None
        self.available = SENTENCE_TRANSFORMERS_AVAILABLE
    
    async def initialize(self) -> bool:
        """Initialize embedding model"""
        if not self.available:
            logger.warning(f"sentence-transformers not available, embeddings disabled")
            return False
        
        try:
            # Load model in background to not block
            loop = asyncio.get_event_loop()
            self.model = await loop.run_in_executor(
                None,
                lambda: SentenceTransformer(self.model_name)
            )
            logger.info(f"Embedder initialized with {self.model_name}")
            return True
        except Exception as e:
            logger.error(f"Failed to initialize embedder: {e}")
            self.available = False
            return False
    
    async def embed(self, text: str) -> Optional[List[float]]:
        """Generate embedding for text"""
        if not self.available or self.model is None:
            return None
        
        try:
            loop = asyncio.get_event_loop()
            embedding = await loop.run_in_executor(
                None,
                lambda: self.model.encode(text, convert_to_tensor=False).tolist()
            )
            return embedding
        except Exception as e:
            logger.warning(f"Embedding failed for text: {e}")
            return None


class SemanticCacheManager:
    """
    Manages semantic caching with ChromaDB.
    Stores query-response pairs and retrieves similar cached responses.
    """
    
    def __init__(
        self,
        cache_dir: Path = None,
        similarity_threshold: float = 0.92,
        embedder: EmbedderService = None
    ):
        self.cache_dir = cache_dir or Path.home() / ".jarvis" / "vector_cache"
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        
        self.similarity_threshold = similarity_threshold
        self.embedder = embedder or EmbedderService()
        
        self.client = None
        self.collection = None
        self.available = False
        
        # In-memory cache for fast lookups
        self.memory_cache: Dict[str, CacheEntry] = {}
    
    async def initialize(self) -> bool:
        """Initialize ChromaDB and embedder"""
        try:
            # Initialize embedder first
            embedder_ok = await self.embedder.initialize()
            if not embedder_ok:
                logger.warning("Embedder failed, cache will use fallback mode")
            
            if not CHROMADB_AVAILABLE:
                logger.warning("ChromaDB not available, using memory-only cache")
                self.available = False
                return True  # Can still work with memory cache
            
            # Initialize ChromaDB
            settings = Settings(
                chroma_db_impl="duckdb+parquet",
                persist_directory=str(self.cache_dir),
                anonymized_telemetry=False,
                allow_reset=True,
            )
            
            self.client = chromadb.Client(settings)
            self.collection = self.client.get_or_create_collection(
                name="jarvis_semantic_cache",
                metadata={"hnsw:space": "cosine"}
            )
            
            self.available = True
            logger.info(f"Semantic cache initialized at {self.cache_dir}")
            return True
            
        except Exception as e:
            logger.error(f"ChromaDB initialization failed: {e}")
            self.available = False
            return True  # Still functional with memory cache
    
    def _hash_query(self, query: str) -> str:
        """Generate hash for query"""
        return hashlib.md5(query.encode()).hexdigest()
    
    async def put(
        self,
        query: str,
        response: str,
        context: Dict[str, Any],
        ttl_minutes: int = 60
    ) -> None:
        """Store query-response in cache"""
        query_hash = self._hash_query(query)
        
        # Generate embedding
        embedding = await self.embedder.embed(query)
        
        # Create entry
        entry = CacheEntry(
            query_hash=query_hash,
            query_text=query,
            response=response,
            context=context,
            embedding=embedding,
            ttl_minutes=ttl_minutes
        )
        
        # Store in memory cache
        self.memory_cache[query_hash] = entry
        
        # Store in ChromaDB
        if self.available and embedding:
            try:
                self.collection.add(
                    ids=[query_hash],
                    embeddings=[embedding],
                    documents=[query],
                    metadatas=[{
                        "timestamp": entry.timestamp,
                        "context": json.dumps(context, default=str),
                        "ttl_minutes": ttl_minutes
                    }]
                )
            except Exception as e:
                logger.warning(f"ChromaDB storage failed: {e}")
    
    async def get(self, query: str) -> Optional[Tuple[str, Dict[str, Any], float]]:
        """
        Retrieve cached response for query if similar exists.
        
        Returns: (response, context, similarity_score) or None
        """
        query_hash = self._hash_query(query)
        
        # Check memory cache first (exact match)
        if query_hash in self.memory_cache:
            entry = self.memory_cache[query_hash]
            if not entry.is_expired():
                entry.hit_count += 1
                return (entry.response, entry.context, 1.0)
            else:
                del self.memory_cache[query_hash]
        
        # Search ChromaDB for similar queries
        if not self.available or self.embedder.model is None:
            return None
        
        try:
            embedding = await self.embedder.embed(query)
            if not embedding:
                return None
            
            results = self.collection.query(
                query_embeddings=[embedding],
                n_results=1
            )
            
            if not results or not results['ids'] or not results['ids'][0]:
                return None
            
            # Extract result
            result_id = results['ids'][0][0]
            distances = results['distances'][0][0]
            
            # Convert distance to similarity (cosine distance)
            similarity = 1 - distances
            
            if similarity < self.similarity_threshold:
                return None
            
            # Retrieve from memory or reconstruct
            if result_id in self.memory_cache:
                entry = self.memory_cache[result_id]
                if not entry.is_expired():
                    entry.hit_count += 1
                    return (entry.response, entry.context, similarity)
            
            return None
            
        except Exception as e:
            logger.warning(f"ChromaDB query failed: {e}")
            return None
    
    async def cleanup_expired(self) -> int:
        """Remove expired entries from cache"""
        expired_hashes = [
            h for h, entry in self.memory_cache.items()
            if entry.is_expired()
        ]
        
        for h in expired_hashes:
            del self.memory_cache[h]
            
            if self.available:
                try:
                    self.collection.delete(ids=[h])
                except Exception as e:
                    logger.debug(f"Cleanup deletion failed: {e}")
        
        return len(expired_hashes)
    
    async def stats(self) -> Dict[str, Any]:
        """Get cache statistics"""
        total_entries = len(self.memory_cache)
        expired = sum(1 for e in self.memory_cache.values() if e.is_expired())
        total_hits = sum(e.hit_count for e in self.memory_cache.values())
        
        return {
            "total_entries": total_entries,
            "expired_entries": expired,
            "total_hits": total_hits,
            "embedder_available": self.embedder.available,
            "chromadb_available": self.available,
            "cache_dir": str(self.cache_dir)
        }


# Singleton instance
_cache_manager: Optional[SemanticCacheManager] = None


async def get_semantic_cache() -> SemanticCacheManager:
    """Get or create singleton SemanticCacheManager"""
    global _cache_manager
    
    if _cache_manager is None:
        _cache_manager = SemanticCacheManager()
        await _cache_manager.initialize()
    
    return _cache_manager
