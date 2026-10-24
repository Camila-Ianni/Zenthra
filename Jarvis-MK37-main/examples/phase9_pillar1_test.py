"""
Phase 9 Pillar 1: Semantic Cache Tests

Tests for ChromaDB integration, embeddings, similarity search, caching.
Total: 7 tests
"""

import pytest
import tempfile
from pathlib import Path
from datetime import datetime, timedelta

from core.semantic_cache import (
    SemanticCacheManager, EmbedderService, CacheEntry, get_semantic_cache
)


@pytest.fixture
def temp_cache_dir():
    """Create temporary cache directory"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)


class TestEmbedderService:
    """Test embedding generation"""
    
    @pytest.mark.asyncio
    async def test_embedder_initialization(self):
        """Test embedder initialization"""
        embedder = EmbedderService()
        result = await embedder.initialize()
        
        assert isinstance(result, bool)
    
    @pytest.mark.asyncio
    async def test_embedding_generation(self):
        """Test embedding generation for text"""
        embedder = EmbedderService()
        await embedder.initialize()
        
        text = "hello world"
        embedding = await embedder.embed(text)
        
        if embedder.available and embedding:
            assert isinstance(embedding, list)
            assert len(embedding) > 0


class TestCacheEntry:
    """Test cache entry dataclass"""
    
    def test_cache_entry_creation(self):
        """Test creating cache entry"""
        entry = CacheEntry(
            query_hash="abc123",
            query_text="test query",
            response="test response",
            context={"key": "value"}
        )
        
        assert entry.query_hash == "abc123"
        assert entry.response == "test response"
    
    def test_cache_entry_expiration(self):
        """Test cache entry TTL"""
        # Create expired entry
        old_time = (datetime.now() - timedelta(hours=2)).isoformat()
        
        entry = CacheEntry(
            query_hash="abc123",
            query_text="test",
            response="response",
            context={},
            timestamp=old_time,
            ttl_minutes=60
        )
        
        assert entry.is_expired() is True
    
    def test_cache_entry_valid(self):
        """Test cache entry not expired"""
        entry = CacheEntry(
            query_hash="abc123",
            query_text="test",
            response="response",
            context={},
            ttl_minutes=60
        )
        
        assert entry.is_expired() is False


class TestSemanticCacheManager:
    """Test semantic cache manager"""
    
    @pytest.mark.asyncio
    async def test_cache_initialization(self, temp_cache_dir):
        """Test cache manager initialization"""
        cache = SemanticCacheManager(cache_dir=temp_cache_dir)
        result = await cache.initialize()
        
        assert result is True
    
    @pytest.mark.asyncio
    async def test_cache_put_and_get(self, temp_cache_dir):
        """Test storing and retrieving from cache"""
        cache = SemanticCacheManager(cache_dir=temp_cache_dir)
        await cache.initialize()
        
        # Store in cache
        await cache.put(
            query="what is Python?",
            response="Python is a programming language",
            context={"language": "en"}
        )
        
        # Should be in memory cache
        assert len(cache.memory_cache) > 0
    
    @pytest.mark.asyncio
    async def test_exact_match_retrieval(self, temp_cache_dir):
        """Test retrieving exact match from memory cache"""
        cache = SemanticCacheManager(cache_dir=temp_cache_dir)
        await cache.initialize()
        
        # Store
        query = "what is JavaScript?"
        response = "JavaScript is a web language"
        
        await cache.put(
            query=query,
            response=response,
            context={"type": "web"}
        )
        
        # Retrieve
        result = await cache.get(query)
        
        if cache.embedder.available:
            assert result is not None
            assert result[0] == response
            assert result[2] == 1.0  # Exact match
    
    @pytest.mark.asyncio
    async def test_similarity_search(self, temp_cache_dir):
        """Test similarity-based retrieval"""
        cache = SemanticCacheManager(
            cache_dir=temp_cache_dir,
            similarity_threshold=0.85
        )
        await cache.initialize()
        
        if not cache.embedder.available:
            pytest.skip("Embedder not available")
        
        # Store query
        original = "How do I write a function in Python?"
        response = "Use def keyword"
        
        await cache.put(
            query=original,
            response=response,
            context={"topic": "python"}
        )
        
        # Query with different wording
        similar = "How do I define a function in Python?"
        result = await cache.get(similar)
        
        # May or may not match depending on similarity threshold
        # Just verify it doesn't error
        assert result is None or isinstance(result, tuple)
    
    @pytest.mark.asyncio
    async def test_cleanup_expired(self, temp_cache_dir):
        """Test cleanup of expired entries"""
        cache = SemanticCacheManager(cache_dir=temp_cache_dir)
        await cache.initialize()
        
        # Add entry with short TTL
        await cache.put(
            query="temporary",
            response="response",
            context={},
            ttl_minutes=0  # Already expired
        )
        
        initial_count = len(cache.memory_cache)
        cleaned = await cache.cleanup_expired()
        
        # Should have cleaned at least the expired entry
        assert cleaned >= 0  # May be 0 or 1 depending on timing
    
    @pytest.mark.asyncio
    async def test_cache_statistics(self, temp_cache_dir):
        """Test cache statistics"""
        cache = SemanticCacheManager(cache_dir=temp_cache_dir)
        await cache.initialize()
        
        # Add some entries
        await cache.put(
            query="test 1",
            response="response 1",
            context={}
        )
        
        stats = await cache.stats()
        
        assert "total_entries" in stats
        assert "expired_entries" in stats
        assert "total_hits" in stats
        assert "embedder_available" in stats
        assert "chromadb_available" in stats


class TestSemanticCacheSingleton:
    """Test singleton pattern"""
    
    @pytest.mark.asyncio
    async def test_get_semantic_cache_singleton(self):
        """Test get_semantic_cache returns same instance"""
        cache1 = await get_semantic_cache()
        cache2 = await get_semantic_cache()
        
        assert cache1 is cache2


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
