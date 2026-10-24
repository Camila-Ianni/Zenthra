"""
Vision Service - Perception Awareness for Debugging

Captures screenshots and extracts text (OCR) for debugging context.
Integrates with Local Intent Classifier for VISION_CONTEXT intents.

Features:
  - Lightweight screenshot capture (macOS native)
  - Efficient image processing
  - Caching to avoid redundant captures
  - Safe handling (no cloud upload unless needed)
"""

import logging
import subprocess
import base64
from typing import Optional, Tuple, Dict, Any
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path

try:
    from PIL import Image
    import pytesseract
    OCR_AVAILABLE = True
except ImportError:
    OCR_AVAILABLE = False


logger = logging.getLogger(__name__)


@dataclass
class CaptureResult:
    """Result of a screenshot capture"""
    image_path: str
    timestamp: str
    region: str  # "active", "full", "terminal", "editor"
    text_extracted: str
    confidence: float  # 0.0 to 1.0
    size_kb: int


class VisionService:
    """
    Captures and processes screenshots for debugging context.
    Uses macOS native screencapture for speed.
    """
    
    def __init__(self, cache_dir: Optional[Path] = None, ocr_enabled: bool = True):
        self.cache_dir = cache_dir or Path.home() / ".jarvis" / "captures"
        self.ocr_enabled = ocr_enabled and OCR_AVAILABLE
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self._last_capture: Optional[CaptureResult] = None
        self._last_capture_time: Optional[datetime] = None
        self._capture_cooldown = timedelta(seconds=2)  # Avoid rapid captures
    
    async def capture_active_window(
        self,
        include_text: bool = True,
    ) -> Optional[CaptureResult]:
        """
        Capture the currently active window.
        
        Args:
            include_text: Whether to extract text via OCR
        
        Returns:
            CaptureResult with image and extracted text
        """
        # Check cooldown
        if self._last_capture_time and datetime.now() - self._last_capture_time < self._capture_cooldown:
            logger.debug("⏳ Capture cooldown active, returning cached result")
            return self._last_capture
        
        try:
            # Get active app name
            active_app = self._get_active_app()
            
            # Determine region
            region = self._determine_region(active_app)
            
            # Capture screenshot
            image_path = await self._capture_screenshot(region)
            if not image_path:
                return None
            
            # Extract text if enabled
            extracted_text = ""
            if include_text and self.ocr_enabled:
                extracted_text = await self._extract_text(image_path)
            
            # Get file size
            size_kb = image_path.stat().st_size // 1024
            
            # Create result
            result = CaptureResult(
                image_path=str(image_path),
                timestamp=datetime.now().isoformat(),
                region=region,
                text_extracted=extracted_text,
                confidence=0.7 if extracted_text else 0.5,
                size_kb=size_kb
            )
            
            # Cache
            self._last_capture = result
            self._last_capture_time = datetime.now()
            
            logger.info(f"✅ Captured {region} window ({size_kb}KB)")
            return result
        
        except Exception as e:
            logger.error(f"❌ Capture failed: {e}")
            return None
    
    async def capture_full_screen(
        self,
        include_text: bool = True,
    ) -> Optional[CaptureResult]:
        """Capture full screen"""
        try:
            image_path = await self._capture_screenshot("full")
            if not image_path:
                return None
            
            extracted_text = ""
            if include_text and self.ocr_enabled:
                extracted_text = await self._extract_text(image_path)
            
            size_kb = image_path.stat().st_size // 1024
            
            result = CaptureResult(
                image_path=str(image_path),
                timestamp=datetime.now().isoformat(),
                region="full",
                text_extracted=extracted_text,
                confidence=0.6,
                size_kb=size_kb
            )
            
            self._last_capture = result
            self._last_capture_time = datetime.now()
            
            logger.info(f"✅ Captured full screen ({size_kb}KB)")
            return result
        
        except Exception as e:
            logger.error(f"❌ Full screen capture failed: {e}")
            return None
    
    async def capture_terminal_output(self) -> Optional[CaptureResult]:
        """Capture active terminal window"""
        return await self.capture_active_window(include_text=True)
    
    async def _capture_screenshot(self, region: str) -> Optional[Path]:
        """
        Use macOS screencapture command for speed.
        
        Args:
            region: "full", "active", "terminal", "editor"
        
        Returns:
            Path to captured image
        """
        try:
            filename = f"capture_{datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
            filepath = self.cache_dir / filename
            
            cmd = ["screencapture", "-x", "-o", str(filepath)]
            
            # Add region-specific flags
            if region == "active":
                cmd.append("-W")  # Capture active window
            # else: full screen is default
            
            # Execute with timeout
            result = subprocess.run(
                cmd,
                capture_output=True,
                timeout=5,
                text=True
            )
            
            if result.returncode == 0 and filepath.exists():
                return filepath
            else:
                logger.warning(f"screencapture failed: {result.stderr}")
                return None
        
        except subprocess.TimeoutExpired:
            logger.warning("screencapture timeout")
            return None
        except Exception as e:
            logger.error(f"screencapture error: {e}")
            return None
    
    async def _extract_text(self, image_path: Path) -> str:
        """Extract text from image using OCR"""
        if not OCR_AVAILABLE:
            return ""
        
        try:
            # Load and process image
            image = Image.open(image_path)
            
            # Resize for better OCR (if small)
            if image.width < 800:
                scale = 800 / image.width
                new_size = (int(image.width * scale), int(image.height * scale))
                image = image.resize(new_size, Image.Resampling.LANCZOS)
            
            # Extract text
            text = pytesseract.image_to_string(image)
            
            # Clean up
            text = text.strip()
            if len(text) > 2000:
                text = text[:2000] + "..."
            
            logger.debug(f"✅ Extracted {len(text)} chars via OCR")
            return text
        
        except Exception as e:
            logger.warning(f"OCR extraction failed: {e}")
            return ""
    
    def _get_active_app(self) -> str:
        """Get name of active application"""
        try:
            result = subprocess.run(
                ["osascript", "-e", "tell app \"System Events\" to name of first application process where frontmost is true"],
                capture_output=True,
                timeout=2,
                text=True
            )
            return result.stdout.strip()
        except Exception:
            return "Unknown"
    
    def _determine_region(self, app_name: str) -> str:
        """Determine best capture region based on app"""
        app_lower = app_name.lower()
        
        if "terminal" in app_lower or "iterm" in app_lower:
            return "terminal"
        elif "code" in app_lower or "xcode" in app_lower or "editor" in app_lower:
            return "editor"
        else:
            return "active"
    
    async def get_debug_context(self) -> Dict[str, Any]:
        """
        Capture debug context for error analysis.
        Returns: image + extracted text + environment info
        """
        try:
            # Capture active window
            capture = await self.capture_active_window(include_text=True)
            if not capture:
                return {"error": "Capture failed"}
            
            # Build context
            context = {
                "timestamp": capture.timestamp,
                "region": capture.region,
                "image_path": capture.image_path,
                "extracted_text": capture.text_extracted,
                "size_kb": capture.size_kb,
                "confidence": capture.confidence,
                "active_app": self._get_active_app(),
            }
            
            return context
        
        except Exception as e:
            logger.error(f"Failed to get debug context: {e}")
            return {"error": str(e)}
    
    def get_cached_capture(self) -> Optional[CaptureResult]:
        """Return last cached capture if available"""
        if self._last_capture and self._last_capture_time:
            # Return if less than 1 minute old
            if datetime.now() - self._last_capture_time < timedelta(minutes=1):
                return self._last_capture
        return None
    
    def cleanup_old_captures(self, max_age_hours: int = 24) -> int:
        """Remove old capture files to save space"""
        try:
            cutoff = datetime.now() - timedelta(hours=max_age_hours)
            cutoff_timestamp = cutoff.timestamp()
            
            deleted_count = 0
            for capture_file in self.cache_dir.glob("capture_*.png"):
                if capture_file.stat().st_mtime < cutoff_timestamp:
                    capture_file.unlink()
                    deleted_count += 1
            
            logger.info(f"🧹 Cleaned {deleted_count} old capture files")
            return deleted_count
        
        except Exception as e:
            logger.error(f"Cleanup error: {e}")
            return 0


# Global instance
_vision_service: Optional[VisionService] = None


async def get_vision_service() -> VisionService:
    """Get or create global vision service instance"""
    global _vision_service
    if _vision_service is None:
        _vision_service = VisionService()
    return _vision_service


__all__ = [
    "VisionService",
    "CaptureResult",
    "get_vision_service",
]
