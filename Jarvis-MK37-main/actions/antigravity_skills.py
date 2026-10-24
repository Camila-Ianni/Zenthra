"""
Antigravity Skills Bridge
Mac OS-level system control, media management, and browser automation
Transforms JARVIS into a true OS assistant

Compatible with Python 3.9+
"""

import subprocess
import logging
import os
from dataclasses import dataclass
from typing import Optional, Dict, Any, List
from enum import Enum
import time

logger = logging.getLogger(__name__)


class SkillCategory(Enum):
    """Categories of Antigravity skills"""
    SYSTEM_CONTROL = "system_control"
    MEDIA_CONTROL = "media_control"
    BROWSER_AUTOMATION = "browser_automation"


class SkillStatus(Enum):
    """Status of skill execution"""
    SUCCESS = "success"
    FAILED = "failed"
    PERMISSION_DENIED = "permission_denied"
    NOT_AVAILABLE = "not_available"


@dataclass
class SkillResult:
    """Result of skill execution"""
    skill_name: str
    category: SkillCategory
    status: SkillStatus
    output: Optional[str] = None
    error: Optional[str] = None
    execution_time_ms: float = 0.0


class AntigravitySkills:
    """Mac OS integration layer for JARVIS"""
    
    def __init__(self):
        self.skills: Dict[str, Any] = {}
        self._register_skills()
        self.spotify_client = None
        self._initialize_spotify()
    
    def _register_skills(self) -> None:
        """Register all available skills"""
        self.skills = {
            # System Control
            "set_volume": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Set system volume (0-100)",
                "handler": self.set_volume,
            },
            "set_brightness": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Set screen brightness (0-100)",
                "handler": self.set_brightness,
            },
            "get_battery_status": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Get battery percentage and status",
                "handler": self.get_battery_status,
            },
            "lock_screen": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Lock the screen",
                "handler": self.lock_screen,
            },
            "sleep_mac": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Put Mac to sleep",
                "handler": self.sleep_mac,
            },
            "get_screen_resolution": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Get current screen resolution",
                "handler": self.get_screen_resolution,
            },
            "open_app": {
                "category": SkillCategory.SYSTEM_CONTROL,
                "description": "Open an application by name",
                "handler": self.open_app,
            },
            
            # Media Control
            "spotify_play": {
                "category": SkillCategory.MEDIA_CONTROL,
                "description": "Play on Spotify",
                "handler": self.spotify_play,
            },
            "spotify_pause": {
                "category": SkillCategory.MEDIA_CONTROL,
                "description": "Pause Spotify",
                "handler": self.spotify_pause,
            },
            "spotify_next": {
                "category": SkillCategory.MEDIA_CONTROL,
                "description": "Skip to next track",
                "handler": self.spotify_next,
            },
            "spotify_previous": {
                "category": SkillCategory.MEDIA_CONTROL,
                "description": "Previous track",
                "handler": self.spotify_previous,
            },
            "spotify_set_volume": {
                "category": SkillCategory.MEDIA_CONTROL,
                "description": "Set Spotify volume",
                "handler": self.spotify_set_volume,
            },
            
            # Browser Automation
            "open_browser": {
                "category": SkillCategory.BROWSER_AUTOMATION,
                "description": "Open URL in browser",
                "handler": self.open_browser,
            },
            "close_browser_tab": {
                "category": SkillCategory.BROWSER_AUTOMATION,
                "description": "Close browser tab",
                "handler": self.close_browser_tab,
            },
        }
    
    def _initialize_spotify(self) -> None:
        """Initialize Spotify client if available"""
        try:
            import spotipy
            from spotipy.oauth2 import SpotifyOAuth
            
            scope = "user-modify-playback-state,user-read-playback-state"
            self.spotify_client = spotipy.Spotify(
                auth_manager=SpotifyOAuth(scope=scope, cache_path=".spotifycache")
            )
            logger.info("✅ Spotify client initialized")
        except ImportError:
            logger.warning("⚠️ Spotipy not installed. Spotify skills unavailable.")
        except Exception as e:
            logger.warning(f"⚠️ Could not initialize Spotify: {str(e)}")
    
    # ========== SYSTEM CONTROL SKILLS ==========
    
    def set_volume(self, level: int) -> SkillResult:
        """Set system volume (0-100)"""
        start_time = time.time()
        
        try:
            level = max(0, min(100, level))
            script = f'set volume output volume {level}'
            subprocess.run(
                ['osascript', '-e', script],
                check=True,
                capture_output=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="set_volume",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output=f"Volume set to {level}%",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="set_volume",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def set_brightness(self, level: int) -> SkillResult:
        """Set screen brightness (0-100)"""
        start_time = time.time()
        
        try:
            level = max(0, min(100, level))
            script = f'tell application "System Preferences" to set current brightness to {level}'
            subprocess.run(
                ['osascript', '-e', script],
                check=True,
                capture_output=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="set_brightness",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output=f"Brightness set to {level}%",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError:
            logger.warning("Brightness control may require permissions")
            return SkillResult(
                skill_name="set_brightness",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.PERMISSION_DENIED,
                error="Brightness control requires system permissions",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def get_battery_status(self) -> SkillResult:
        """Get battery percentage and status"""
        start_time = time.time()
        
        try:
            result = subprocess.run(
                ['pmset', '-g', 'batt'],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="get_battery_status",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output=result.stdout,
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="get_battery_status",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def lock_screen(self) -> SkillResult:
        """Lock the screen"""
        start_time = time.time()
        
        try:
            script = 'tell application "System Events" to key code 12 using command down'
            subprocess.run(
                ['osascript', '-e', script],
                check=True,
                capture_output=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="lock_screen",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output="Screen locked",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="lock_screen",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def sleep_mac(self) -> SkillResult:
        """Put Mac to sleep"""
        start_time = time.time()
        
        try:
            subprocess.run(
                ['osascript', '-e', 'tell application "System Events" to sleep'],
                check=True,
                capture_output=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="sleep_mac",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output="Mac going to sleep",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="sleep_mac",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def get_screen_resolution(self) -> SkillResult:
        """Get current screen resolution"""
        start_time = time.time()
        
        try:
            script = 'tell application "Finder" to get bounds of window of desktop'
            result = subprocess.run(
                ['osascript', '-e', script],
                check=True,
                capture_output=True,
                text=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="get_screen_resolution",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output=result.stdout.strip(),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="get_screen_resolution",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def open_app(self, app_name: str) -> SkillResult:
        """Open an application"""
        start_time = time.time()
        
        try:
            script = f'tell application "{app_name}" to activate'
            subprocess.run(
                ['osascript', '-e', script],
                check=True,
                capture_output=True,
                timeout=10,
            )
            
            return SkillResult(
                skill_name="open_app",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.SUCCESS,
                output=f"Opened {app_name}",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError:
            return SkillResult(
                skill_name="open_app",
                category=SkillCategory.SYSTEM_CONTROL,
                status=SkillStatus.FAILED,
                error=f"Could not open {app_name}",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    # ========== MEDIA CONTROL SKILLS ==========
    
    def spotify_play(self) -> SkillResult:
        """Play on Spotify"""
        start_time = time.time()
        
        try:
            if not self.spotify_client:
                raise RuntimeError("Spotify client not initialized")
            
            self.spotify_client.start_playback()
            
            return SkillResult(
                skill_name="spotify_play",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.SUCCESS,
                output="Playing",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except Exception as e:
            return SkillResult(
                skill_name="spotify_play",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def spotify_pause(self) -> SkillResult:
        """Pause Spotify"""
        start_time = time.time()
        
        try:
            if not self.spotify_client:
                raise RuntimeError("Spotify client not initialized")
            
            self.spotify_client.pause_playback()
            
            return SkillResult(
                skill_name="spotify_pause",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.SUCCESS,
                output="Paused",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except Exception as e:
            return SkillResult(
                skill_name="spotify_pause",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def spotify_next(self) -> SkillResult:
        """Skip to next track"""
        start_time = time.time()
        
        try:
            if not self.spotify_client:
                raise RuntimeError("Spotify client not initialized")
            
            self.spotify_client.next_track()
            
            return SkillResult(
                skill_name="spotify_next",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.SUCCESS,
                output="Skipped to next",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except Exception as e:
            return SkillResult(
                skill_name="spotify_next",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def spotify_previous(self) -> SkillResult:
        """Previous track"""
        start_time = time.time()
        
        try:
            if not self.spotify_client:
                raise RuntimeError("Spotify client not initialized")
            
            self.spotify_client.previous_track()
            
            return SkillResult(
                skill_name="spotify_previous",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.SUCCESS,
                output="Previous track",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except Exception as e:
            return SkillResult(
                skill_name="spotify_previous",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def spotify_set_volume(self, level: int) -> SkillResult:
        """Set Spotify volume"""
        start_time = time.time()
        
        try:
            if not self.spotify_client:
                raise RuntimeError("Spotify client not initialized")
            
            level = max(0, min(100, level))
            self.spotify_client.volume(level)
            
            return SkillResult(
                skill_name="spotify_set_volume",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.SUCCESS,
                output=f"Volume set to {level}%",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except Exception as e:
            return SkillResult(
                skill_name="spotify_set_volume",
                category=SkillCategory.MEDIA_CONTROL,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    # ========== BROWSER AUTOMATION SKILLS ==========
    
    def open_browser(self, url: str) -> SkillResult:
        """Open URL in browser"""
        start_time = time.time()
        
        try:
            subprocess.run(
                ['open', url],
                check=True,
                capture_output=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="open_browser",
                category=SkillCategory.BROWSER_AUTOMATION,
                status=SkillStatus.SUCCESS,
                output=f"Opened {url}",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="open_browser",
                category=SkillCategory.BROWSER_AUTOMATION,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def close_browser_tab(self) -> SkillResult:
        """Close browser tab"""
        start_time = time.time()
        
        try:
            script = 'tell application "Google Chrome" to close active tab of window 1'
            subprocess.run(
                ['osascript', '-e', script],
                check=True,
                capture_output=True,
                timeout=5,
            )
            
            return SkillResult(
                skill_name="close_browser_tab",
                category=SkillCategory.BROWSER_AUTOMATION,
                status=SkillStatus.SUCCESS,
                output="Tab closed",
                execution_time_ms=(time.time() - start_time) * 1000,
            )
        except subprocess.CalledProcessError as e:
            return SkillResult(
                skill_name="close_browser_tab",
                category=SkillCategory.BROWSER_AUTOMATION,
                status=SkillStatus.FAILED,
                error=str(e),
                execution_time_ms=(time.time() - start_time) * 1000,
            )
    
    def get_available_skills(self) -> Dict[str, Dict[str, Any]]:
        """Get list of available skills"""
        return self.skills
    
    def get_skills_by_category(self, category: SkillCategory) -> Dict[str, Dict[str, Any]]:
        """Get skills by category"""
        return {
            name: info for name, info in self.skills.items()
            if info["category"] == category
        }
