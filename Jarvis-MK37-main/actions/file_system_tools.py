"""
Codebase exploration and analysis tools.
Provides functions for JARVIS to understand repository structure and code
before proposing changes. Enables "codebase awareness" for intelligent code generation.
"""

import os
import json
import subprocess
from pathlib import Path
from typing import Dict, List, Any


def list_files_recursive(parameters: dict, player=None) -> str:
    """
    Maps directory structure recursively.
    
    parameters:
        path: Starting directory (default: ".")
        max_depth: Max recursion level (default: 3)
    
    Returns: JSON string with file tree and statistics
    """
    path = parameters.get("path", ".")
    max_depth = parameters.get("max_depth", 3)
    
    # Directories to exclude
    exclude_dirs = {
        ".venv", "__pycache__", ".git", ".pytest_cache", 
        "node_modules", ".next", "dist", "build", ".env",
        ".DS_Store", "*.egg-info"
    }
    
    def explore_dir(start_path: str, current_depth: int = 0) -> dict:
        """Recursively explore directory structure"""
        if current_depth > max_depth:
            return None
        
        items = {"dirs": {}, "files": []}
        
        try:
            entries = sorted(os.listdir(start_path))
            
            for entry in entries:
                # Skip hidden files except important ones
                if entry.startswith(".") and entry not in {".github", ".gitignore", ".env.example"}:
                    continue
                
                # Skip excluded directories
                if entry in exclude_dirs or entry.endswith(".egg-info"):
                    continue
                
                full_path = os.path.join(start_path, entry)
                
                if os.path.isdir(full_path):
                    sub_items = explore_dir(full_path, current_depth + 1)
                    if sub_items:
                        items["dirs"][entry] = sub_items
                else:
                    try:
                        size = os.path.getsize(full_path)
                        items["files"].append({
                            "name": entry,
                            "size": size
                        })
                    except Exception:
                        pass
        
        except PermissionError:
            pass
        
        return items if (items["dirs"] or items["files"]) else None
    
    try:
        structure = explore_dir(path)
        
        # Count totals
        def count_files(node):
            if node is None:
                return 0
            total = len(node.get("files", []))
            for sub in node.get("dirs", {}).values():
                total += count_files(sub)
            return total
        
        result = {
            "structure": structure,
            "stats": {
                "total_files": count_files(structure) if structure else 0,
                "root": path
            }
        }
        
        print(f"[FilesystemTools] ✅ Explored: {path}")
        return json.dumps(result, indent=2)
    
    except Exception as e:
        print(f"[FilesystemTools] ⚠️ Error exploring {path}: {e}")
        return json.dumps({"error": str(e)})


def search_code(parameters: dict, player=None) -> str:
    """
    Searches for code patterns using grep (or fallback regex).
    
    parameters:
        query: Search pattern (e.g., "def email_validator" or "class Logger")
        extensions: File extensions to search (default: ["*.py"])
        path: Directory to search in (default: ".")
    
    Returns: JSON string with search results
    """
    query = parameters.get("query", "")
    extensions = parameters.get("extensions", ["*.py"])
    path = parameters.get("path", ".")
    
    if not query:
        return json.dumps({"error": "No search query provided"})
    
    results = {}
    
    try:
        # Try using grep command
        for ext in extensions:
            try:
                cmd = [
                    "grep",
                    "-r",
                    "-n",
                    "--include=" + ext,
                    query,
                    path
                ]
                
                output = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                
                if output.stdout:
                    for line in output.stdout.strip().split("\n"):
                        if not line or ":" not in line:
                            continue
                        
                        parts = line.split(":", 2)
                        if len(parts) < 3:
                            continue
                        
                        file_path = parts[0]
                        try:
                            line_num = int(parts[1])
                        except ValueError:
                            continue
                        
                        content = parts[2].strip()
                        
                        if file_path not in results:
                            results[file_path] = []
                        
                        results[file_path].append({
                            "line": line_num,
                            "content": content[:200]  # Truncate long lines
                        })
            
            except subprocess.TimeoutExpired:
                pass
            except FileNotFoundError:
                # grep not available, skip
                pass
    
    except Exception as e:
        print(f"[FilesystemTools] ⚠️ Search failed: {e}")
        return json.dumps({"error": str(e)})
    
    if results:
        print(f"[FilesystemTools] ✅ Found {len(results)} files matching '{query}'")
    
    return json.dumps(results, indent=2)


def read_multiple_files(parameters: dict, player=None) -> str:
    """
    Reads multiple source files for context analysis.
    
    parameters:
        paths: Array of file paths to read
        max_size: Maximum bytes per file (default: 50000)
    
    Returns: JSON string with file contents (truncated if needed)
    """
    paths = parameters.get("paths", [])
    max_size = parameters.get("max_size", 50000)
    
    if not paths:
        return json.dumps({"error": "No paths provided", "files": {}})
    
    result = {"files": {}, "truncated": []}
    
    for path_str in paths:
        try:
            p = Path(path_str)
            
            if not p.exists():
                result["files"][path_str] = f"[FILE NOT FOUND: {path_str}]"
                continue
            
            if not p.is_file():
                result["files"][path_str] = f"[NOT A FILE: {path_str}]"
                continue
            
            content = p.read_text(encoding="utf-8", errors="replace")
            
            if len(content) > max_size:
                result["files"][path_str] = content[:max_size] + f"\n\n... [FILE TRUNCATED - {len(content) - max_size} more bytes]"
                result["truncated"].append(path_str)
            else:
                result["files"][path_str] = content
        
        except Exception as e:
            result["files"][path_str] = f"[ERROR READING FILE: {e}]"
    
    print(f"[FilesystemTools] ✅ Read {len([f for f in result['files'] if not f.startswith('[')])} files")
    return json.dumps(result, indent=2)


def get_file_stats(parameters: dict, player=None) -> str:
    """
    Analyzes repository code structure and metrics.
    
    parameters:
        path: Directory to analyze (default: ".")
    
    Returns: JSON string with file counts, languages, LOC, etc.
    """
    path = parameters.get("path", ".")
    
    stats = {
        "total_files": 0,
        "by_extension": {},
        "total_lines": 0,
        "largest_files": [],
        "path": path
    }
    
    files_with_info = []
    
    try:
        for root, dirs, files in os.walk(path):
            # Skip excluded directories
            excluded = {".venv", "__pycache__", ".git", "node_modules", "dist", "build"}
            dirs[:] = [d for d in dirs if d not in excluded]
            
            for file in files:
                full_path = os.path.join(root, file)
                
                try:
                    size = os.path.getsize(full_path)
                    ext = os.path.splitext(file)[1] or "no_ext"
                    
                    # Count by extension
                    stats["by_extension"][ext] = stats["by_extension"].get(ext, 0) + 1
                    stats["total_files"] += 1
                    
                    # Count lines for source files
                    source_exts = {".py", ".js", ".ts", ".java", ".cpp", ".c", ".go", ".rb", ".php", ".rs"}
                    if ext in source_exts:
                        try:
                            with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                                lines = len(f.readlines())
                                stats["total_lines"] += lines
                                files_with_info.append((full_path, size, lines, ext))
                        except Exception:
                            pass
                    else:
                        files_with_info.append((full_path, size, 0, ext))
                
                except Exception:
                    pass
        
        # Get top 5 largest
        files_with_info.sort(key=lambda x: x[1], reverse=True)
        stats["largest_files"] = [
            {
                "file": f.replace(path, "."),
                "size_kb": round(s / 1024, 1),
                "lines": l,
                "ext": e
            }
            for f, s, l, e in files_with_info[:5]
        ]
    
    except Exception as e:
        print(f"[FilesystemTools] ⚠️ Stats analysis failed: {e}")
        return json.dumps({"error": str(e)})
    
    print(f"[FilesystemTools] ✅ Analyzed {stats['total_files']} files, {stats['total_lines']} LOC")
    return json.dumps(stats, indent=2)
