"""
Path normalization utilities to prevent duplications and ensure consistent absolute paths.
Handles Desktop path resolution and standardization across all actions.
"""

import sys
from pathlib import Path


DESKTOP_PATH = Path.home() / "Desktop"


def normalize_path(path_input: str, base_path: Path = None) -> Path:
    """
    Normalize a path input to an absolute Path object.
    
    Handles:
    - User saying "desktop" or "escritorio" → /Users/cami/Desktop
    - Relative paths → converted to absolute using base_path
    - Already absolute paths → returned as-is
    - Prevents "desktop/desktop" duplication
    
    Args:
        path_input: User input (can be relative, "desktop", "escritorio", etc)
        base_path: Base directory for relative paths (default: Desktop)
    
    Returns:
        Absolute Path object
    
    Examples:
        normalize_path("desktop") → /Users/cami/Desktop
        normalize_path("Desktop/myproject") → /Users/cami/Desktop/myproject
        normalize_path("escritorio") → /Users/cami/Desktop
        normalize_path("/Users/cami/Desktop/file.txt") → /Users/cami/Desktop/file.txt
    """
    if base_path is None:
        base_path = DESKTOP_PATH
    
    if not path_input:
        return base_path
    
    path_lower = path_input.lower().strip()
    
    # Handle special keywords for Desktop
    if path_lower in ("desktop", "escritorio", "~/desktop", "~/escritorio"):
        return DESKTOP_PATH
    
    # Convert to Path object
    p = Path(path_input)
    
    # If already absolute, check for duplication and return
    if p.is_absolute():
        path_str = str(p)
        # Check for "desktop/desktop" duplication
        if "desktop/desktop" in path_str.lower() or "desktop\\desktop" in path_str.lower():
            # Remove the duplicate
            path_str = path_str.lower().replace("desktop/desktop", "desktop")
            path_str = path_str.lower().replace("desktop\\desktop", "desktop")
            return Path(path_str)
        return p
    
    # Handle relative paths that start with "desktop"
    if path_lower.startswith("desktop") or path_lower.startswith("escritorio"):
        # Remove the "desktop" prefix and use DESKTOP_PATH as base
        relative_part = path_input[len("desktop"):].lstrip("/\\")
        if not relative_part:
            return DESKTOP_PATH
        return DESKTOP_PATH / relative_part
    
    # Regular relative path → resolve relative to base_path
    return base_path / path_input


def resolve_desktop_path(filename: str) -> Path:
    """
    Resolve a filename/path to Desktop, handling common variations.
    
    Args:
        filename: Name or relative path
    
    Returns:
        Absolute path under Desktop
    """
    return normalize_path(filename, DESKTOP_PATH)


def standardize_paths(paths: list) -> list:
    """
    Standardize a list of paths to absolute paths.
    
    Args:
        paths: List of path strings
    
    Returns:
        List of absolute Path objects
    """
    return [normalize_path(p) for p in paths]
