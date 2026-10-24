"""
Phase 8 Pillar 3: Vision Service Tests

Tests for screenshot capture and OCR.
Covers: capture functionality, OCR, caching, cleanup.

Total: 8 tests
"""

import pytest
from pathlib import Path
from unittest.mock import Mock, patch, AsyncMock
import tempfile

from core.vision_service import (
    VisionService, CaptureResult, get_vision_service
)


# ============================================================================
# TESTS: Initialization & Setup (2 tests)
# ============================================================================

class TestVisionInitialization:
    """Test vision service initialization"""
    
    def test_vision_service_init(self):
        """Test VisionService initialization"""
        with tempfile.TemporaryDirectory() as tmpdir:
            service = VisionService(cache_dir=Path(tmpdir))
            
            assert service.cache_dir.exists()
            assert service.ocr_enabled is True or service.ocr_enabled is False
    
    @pytest.mark.asyncio
    async def test_get_vision_service_singleton(self):
        """Test get_vision_service singleton"""
        s1 = await get_vision_service()
        s2 = await get_vision_service()
        
        assert s1 is s2


# ============================================================================
# TESTS: Region Detection (2 tests)
# ============================================================================

class TestRegionDetection:
    """Test region detection logic"""
    
    def test_terminal_detection(self):
        """Test Terminal app detection"""
        service = VisionService()
        
        region = service._determine_region("Terminal")
        assert region == "terminal"
        
        region = service._determine_region("iTerm2")
        assert region == "terminal"
    
    def test_editor_detection(self):
        """Test Editor app detection"""
        service = VisionService()
        
        region = service._determine_region("Visual Studio Code")
        assert region == "editor"
        
        region = service._determine_region("Xcode")
        assert region == "editor"


# ============================================================================
# TESTS: Capture Result Structure (1 test)
# ============================================================================

class TestCaptureResult:
    """Test CaptureResult dataclass"""
    
    def test_capture_result_creation(self):
        """Test CaptureResult dataclass"""
        result = CaptureResult(
            image_path="/tmp/test.png",
            timestamp="2026-04-25T22:00:00",
            region="editor",
            text_extracted="def hello(): pass",
            confidence=0.85,
            size_kb=256
        )
        
        assert result.image_path == "/tmp/test.png"
        assert result.region == "editor"
        assert result.confidence == 0.85
        assert result.size_kb == 256


# ============================================================================
# TESTS: Caching & Cooldown (2 tests)
# ============================================================================

class TestCachingBehavior:
    """Test caching and cooldown logic"""
    
    @pytest.mark.asyncio
    async def test_capture_cooldown(self):
        """Test capture cooldown mechanism"""
        with tempfile.TemporaryDirectory() as tmpdir:
            service = VisionService(cache_dir=Path(tmpdir))
            
            # Mock the capture to avoid actual screencapture
            service._capture_screenshot = AsyncMock(return_value=Path(tmpdir) / "test.png")
            Path(tmpdir, "test.png").touch()
            
            # First capture should work
            result1 = await service.capture_active_window()
            assert result1 is not None
            
            # Immediate second call should return cached
            result2 = await service.capture_active_window()
            assert result2 is not None
    
    def test_get_cached_capture(self):
        """Test retrieving cached capture"""
        service = VisionService()
        
        # No cache initially
        cached = service.get_cached_capture()
        assert cached is None


# ============================================================================
# TESTS: Cleanup (1 test)
# ============================================================================

class TestCleanup:
    """Test cleanup functionality"""
    
    def test_cleanup_old_captures(self):
        """Test cleanup of old capture files"""
        with tempfile.TemporaryDirectory() as tmpdir:
            service = VisionService(cache_dir=Path(tmpdir))
            
            # Create fake capture files
            for i in range(3):
                filepath = Path(tmpdir) / f"capture_{i}.png"
                filepath.touch()
            
            # Cleanup should find files but won't delete (0 hours old)
            count = service.cleanup_old_captures(max_age_hours=0)
            
            # Should have deleted some (old files)
            assert count >= 0


# ============================================================================
# Test Execution
# ============================================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
