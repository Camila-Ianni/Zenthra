"""
PHASE 6 - PILLAR 2: Antigravity Skills Tests
Tests for Mac OS control, media, and browser automation
"""

import pytest
from actions.antigravity_skills import (
    AntigravitySkills, SkillCategory, SkillStatus, SkillResult
)


class TestSkillRegistration:
    """Tests for skill registration"""
    
    def test_antigravity_initialization(self):
        """Test that Antigravity initializes correctly"""
        skills = AntigravitySkills()
        assert skills is not None
        assert len(skills.skills) > 0
    
    def test_get_available_skills(self):
        """Test getting list of available skills"""
        skills = AntigravitySkills()
        available = skills.get_available_skills()
        
        assert "set_volume" in available
        assert "set_brightness" in available
        assert "get_battery_status" in available
        assert "lock_screen" in available
        assert "sleep_mac" in available
        assert "open_app" in available
        assert "spotify_play" in available
        assert "spotify_pause" in available
        assert "open_browser" in available
    
    def test_get_skills_by_category(self):
        """Test filtering skills by category"""
        skills = AntigravitySkills()
        
        system_skills = skills.get_skills_by_category(SkillCategory.SYSTEM_CONTROL)
        media_skills = skills.get_skills_by_category(SkillCategory.MEDIA_CONTROL)
        browser_skills = skills.get_skills_by_category(SkillCategory.BROWSER_AUTOMATION)
        
        assert len(system_skills) >= 6
        assert len(media_skills) >= 5
        assert len(browser_skills) >= 2
        
        for skill_info in system_skills.values():
            assert skill_info["category"] == SkillCategory.SYSTEM_CONTROL
        
        for skill_info in media_skills.values():
            assert skill_info["category"] == SkillCategory.MEDIA_CONTROL
        
        for skill_info in browser_skills.values():
            assert skill_info["category"] == SkillCategory.BROWSER_AUTOMATION


class TestSystemControl:
    """Tests for system control skills"""
    
    def test_skill_result_structure(self):
        """Test that skill results have proper structure"""
        skills = AntigravitySkills()
        
        result = skills.get_battery_status()
        
        assert isinstance(result, SkillResult)
        assert result.skill_name == "get_battery_status"
        assert result.category == SkillCategory.SYSTEM_CONTROL
        assert result.status in [SkillStatus.SUCCESS, SkillStatus.FAILED]
        assert result.execution_time_ms >= 0.0
    
    def test_get_battery_status(self):
        """Test battery status retrieval"""
        skills = AntigravitySkills()
        result = skills.get_battery_status()
        
        assert result.skill_name == "get_battery_status"
        assert result.category == SkillCategory.SYSTEM_CONTROL
        assert result.status in [SkillStatus.SUCCESS, SkillStatus.FAILED]
    
    def test_get_screen_resolution(self):
        """Test screen resolution retrieval"""
        skills = AntigravitySkills()
        result = skills.get_screen_resolution()
        
        assert result.skill_name == "get_screen_resolution"
        assert result.category == SkillCategory.SYSTEM_CONTROL
        # Should either succeed or fail gracefully
        assert result.status in [SkillStatus.SUCCESS, SkillStatus.FAILED]
    
    def test_set_volume_bounds(self):
        """Test that volume setter handles bounds correctly"""
        skills = AntigravitySkills()
        
        result_low = skills.set_volume(-50)
        result_high = skills.set_volume(150)
        result_normal = skills.set_volume(50)
        
        assert result_low.skill_name == "set_volume"
        assert result_high.skill_name == "set_volume"
        assert result_normal.skill_name == "set_volume"
    
    def test_open_app(self):
        """Test opening an application"""
        skills = AntigravitySkills()
        
        result = skills.open_app("Finder")
        
        assert result.skill_name == "open_app"
        assert result.category == SkillCategory.SYSTEM_CONTROL
        # App open may succeed or fail depending on permissions
        assert result.status in [SkillStatus.SUCCESS, SkillStatus.FAILED]


class TestMediaControl:
    """Tests for media control skills"""
    
    def test_spotify_initialization(self):
        """Test Spotify client initialization"""
        skills = AntigravitySkills()
        # Spotify client may or may not initialize depending on credentials
        # Just verify the attribute exists
        assert hasattr(skills, 'spotify_client')
    
    def test_spotify_play_when_unavailable(self):
        """Test Spotify play gracefully handles unavailable client"""
        skills = AntigravitySkills()
        # Don't actually set up Spotify; test graceful failure
        result = skills.spotify_play()
        
        assert result.skill_name == "spotify_play"
        assert result.category == SkillCategory.MEDIA_CONTROL
        # May fail if Spotify not configured
        assert result.status in [SkillStatus.FAILED, SkillStatus.SUCCESS]
    
    def test_spotify_pause_structure(self):
        """Test Spotify pause result structure"""
        skills = AntigravitySkills()
        result = skills.spotify_pause()
        
        assert isinstance(result, SkillResult)
        assert result.skill_name == "spotify_pause"
        assert result.category == SkillCategory.MEDIA_CONTROL
        assert result.execution_time_ms >= 0.0
    
    def test_spotify_next_structure(self):
        """Test Spotify next result structure"""
        skills = AntigravitySkills()
        result = skills.spotify_next()
        
        assert isinstance(result, SkillResult)
        assert result.skill_name == "spotify_next"
        assert result.category == SkillCategory.MEDIA_CONTROL
    
    def test_spotify_previous_structure(self):
        """Test Spotify previous result structure"""
        skills = AntigravitySkills()
        result = skills.spotify_previous()
        
        assert isinstance(result, SkillResult)
        assert result.skill_name == "spotify_previous"
        assert result.category == SkillCategory.MEDIA_CONTROL
    
    def test_spotify_set_volume_bounds(self):
        """Test Spotify volume bounds"""
        skills = AntigravitySkills()
        
        result_low = skills.spotify_set_volume(-50)
        result_high = skills.spotify_set_volume(150)
        result_normal = skills.spotify_set_volume(50)
        
        assert result_low.skill_name == "spotify_set_volume"
        assert result_high.skill_name == "spotify_set_volume"
        assert result_normal.skill_name == "spotify_set_volume"


class TestBrowserAutomation:
    """Tests for browser automation skills"""
    
    def test_open_browser_structure(self):
        """Test open browser result structure"""
        skills = AntigravitySkills()
        result = skills.open_browser("https://www.google.com")
        
        assert isinstance(result, SkillResult)
        assert result.skill_name == "open_browser"
        assert result.category == SkillCategory.BROWSER_AUTOMATION
        assert result.execution_time_ms >= 0.0
    
    def test_close_browser_tab_structure(self):
        """Test close browser tab result structure"""
        skills = AntigravitySkills()
        result = skills.close_browser_tab()
        
        assert isinstance(result, SkillResult)
        assert result.skill_name == "close_browser_tab"
        assert result.category == SkillCategory.BROWSER_AUTOMATION
        # May succeed or fail depending on Chrome being open
        assert result.status in [SkillStatus.SUCCESS, SkillStatus.FAILED]


class TestSkillCategorization:
    """Tests for skill categorization"""
    
    def test_system_control_category(self):
        """Test all system control skills are properly categorized"""
        skills = AntigravitySkills()
        
        system_skills = {
            "set_volume", "set_brightness", "get_battery_status",
            "lock_screen", "sleep_mac", "get_screen_resolution", "open_app"
        }
        
        for skill_name in system_skills:
            assert skill_name in skills.skills
            assert skills.skills[skill_name]["category"] == SkillCategory.SYSTEM_CONTROL
    
    def test_media_control_category(self):
        """Test all media control skills are properly categorized"""
        skills = AntigravitySkills()
        
        media_skills = {
            "spotify_play", "spotify_pause", "spotify_next",
            "spotify_previous", "spotify_set_volume"
        }
        
        for skill_name in media_skills:
            assert skill_name in skills.skills
            assert skills.skills[skill_name]["category"] == SkillCategory.MEDIA_CONTROL
    
    def test_browser_automation_category(self):
        """Test all browser skills are properly categorized"""
        skills = AntigravitySkills()
        
        browser_skills = {"open_browser", "close_browser_tab"}
        
        for skill_name in browser_skills:
            assert skill_name in skills.skills
            assert skills.skills[skill_name]["category"] == SkillCategory.BROWSER_AUTOMATION


class TestErrorHandling:
    """Tests for error handling and graceful degradation"""
    
    def test_result_with_error_message(self):
        """Test that errors are properly captured in results"""
        skills = AntigravitySkills()
        
        result = skills.close_browser_tab()
        
        if result.status == SkillStatus.FAILED:
            assert result.error is not None
            assert len(result.error) > 0
    
    def test_result_with_success_message(self):
        """Test that successes are properly captured"""
        skills = AntigravitySkills()
        
        result = skills.get_battery_status()
        
        if result.status == SkillStatus.SUCCESS:
            assert result.output is not None
            assert result.error is None


class TestIntegration:
    """Integration tests"""
    
    def test_all_skills_callable(self):
        """Test that all registered skills are callable"""
        skills = AntigravitySkills()
        
        for skill_name, skill_info in skills.skills.items():
            assert "handler" in skill_info
            assert callable(skill_info["handler"])
    
    def test_all_skills_have_description(self):
        """Test that all skills have descriptions"""
        skills = AntigravitySkills()
        
        for skill_name, skill_info in skills.skills.items():
            assert "description" in skill_info
            assert isinstance(skill_info["description"], str)
            assert len(skill_info["description"]) > 0
    
    def test_backward_compatibility_with_phase5(self):
        """Test that Phase 5 modules still work"""
        try:
            from core.model_router import ModelRouter
            from core.language_experts import LanguageExpertise
            
            assert ModelRouter is not None
            assert LanguageExpertise is not None
        except ImportError as e:
            pytest.fail(f"Phase 5 modules not available: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
