"""
RAG Local - Retrieval Augmented Generation local simple
Genera árbol del proyecto, lee archivos importados, optimiza contexto
Simula el contexto que IDE profesionales (Copilot, Cursor) tienen
"""

import os
import re
from pathlib import Path
from typing import Optional, List, Dict, Any, Set, Tuple
from dataclasses import dataclass, field
import logging

logger = logging.getLogger("RAGLocal")


@dataclass
class ProjectTree:
    """Árbol de archivos del proyecto."""
    root: Path
    files: List[Path] = field(default_factory=list)
    directories: List[Path] = field(default_factory=list)
    file_count: int = 0
    total_size_bytes: int = 0
    
    def to_string(self, max_depth: int = 3) -> str:
        """Convertir árbol a string representación."""
        return self._build_tree_string(self.root, 0, max_depth)
    
    def _build_tree_string(self, path: Path, depth: int, max_depth: int) -> str:
        """Construir representación string del árbol."""
        if depth > max_depth:
            return ""
        
        lines = []
        indent = "  " * depth
        
        # Skip hidden directories y common build dirs
        skip_dirs = {'.git', '.venv', 'node_modules', '__pycache__', '.pytest_cache', 
                     'dist', 'build', '.next', '.nuxt', 'venv'}
        
        try:
            items = sorted(path.iterdir(), key=lambda x: (not x.is_dir(), x.name))
            
            for i, item in enumerate(items):
                if item.name.startswith('.') and item.name not in {'.env.example', '.gitignore'}:
                    continue
                
                if item.is_dir() and item.name in skip_dirs:
                    continue
                
                is_last = i == len(items) - 1
                prefix = "└── " if is_last else "├── "
                
                lines.append(f"{indent}{prefix}{item.name}/")
                
                if item.is_dir():
                    subtree = self._build_tree_string(item, depth + 1, max_depth)
                    lines.append(subtree)
        
        except PermissionError:
            pass
        
        return '\n'.join(filter(None, lines))


@dataclass
class ImportedFile:
    """Un archivo importado/referenciado."""
    path: Path
    language: str
    content: str
    is_critical: bool = False  # Archive importante para entender contexto


class RAGLocal:
    """
    Retrieval Augmented Generation Local
    
    Gestiona contexto del proyecto:
    1. Genera árbol de archivos
    2. Lee archivos importados
    3. Extrae dependencias
    4. Optimiza prompts con contexto local
    """
    
    # Extensiones por lenguaje
    LANGUAGE_EXTENSIONS = {
        "python": [".py"],
        "javascript": [".js", ".jsx"],
        "typescript": [".ts", ".tsx"],
        "java": [".java"],
        "php": [".php"],
        "go": [".go"],
        "rust": [".rs"],
        "cpp": [".cpp", ".cc", ".cxx", ".h", ".hpp"],
        "c": [".c", ".h"],
    }
    
    # Archivos críticos (siempre incluir si existen)
    CRITICAL_FILES = {
        "package.json",
        "requirements.txt",
        "go.mod",
        "Cargo.toml",
        "pom.xml",
        "setup.py",
        "pyproject.toml",
        ".env.example",
        "README.md",
    }
    
    def __init__(self, project_root: Optional[Path] = None):
        self.project_root = project_root or Path.cwd()
        self.project_tree: Optional[ProjectTree] = None
        self.file_cache: Dict[str, str] = {}
        self.import_graph: Dict[str, Set[str]] = {}
        
    async def build_project_tree(self, max_files: int = 100) -> ProjectTree:
        """
        Construir árbol del proyecto.
        
        Args:
            max_files: Máximo de archivos a incluir
        
        Returns:
            ProjectTree con estructura del proyecto
        """
        logger.info(f"Building project tree for {self.project_root}")
        
        tree = ProjectTree(root=self.project_root)
        skip_dirs = {'.git', '.venv', 'node_modules', '__pycache__', '.pytest_cache',
                     'dist', 'build', '.next', '.nuxt', 'venv'}
        
        file_count = 0
        total_size = 0
        
        for path in self.project_root.rglob('*'):
            # Skip hidden dirs
            if any(part.startswith('.') and part not in {'.env.example', '.gitignore'}
                   for part in path.parts):
                continue
            
            # Skip common build dirs
            if any(part in skip_dirs for part in path.parts):
                continue
            
            if path.is_dir():
                tree.directories.append(path)
            elif path.is_file():
                file_count += 1
                total_size += path.stat().st_size
                
                # Limitar archivos a incluir
                if file_count <= max_files:
                    tree.files.append(path)
        
        tree.file_count = file_count
        tree.total_size_bytes = total_size
        
        self.project_tree = tree
        logger.info(f"Project tree built: {file_count} files, {total_size} bytes")
        
        return tree
    
    async def read_file(self, file_path: Path) -> Optional[str]:
        """Leer contenido de archivo con caching."""
        if file_path in self.file_cache:
            return self.file_cache[file_path]
        
        try:
            content = file_path.read_text(encoding="utf-8")
            self.file_cache[str(file_path)] = content
            return content
        except Exception as e:
            logger.error(f"Failed to read {file_path}: {e}")
            return None
    
    async def extract_imports(self, 
                            file_path: Path,
                            content: str,
                            language: str) -> List[str]:
        """
        Extraer imports/dependencies de un archivo.
        
        Args:
            file_path: Path del archivo
            content: Contenido del archivo
            language: Lenguaje
        
        Returns:
            Lista de archivos importados
        """
        imports = []
        lines = content.split('\n')
        
        # Python imports
        if language in ["python", "py"]:
            patterns = [
                r'^from\s+([\w\.]+)\s+import',
                r'^import\s+([\w\.]+)',
            ]
            for pattern in patterns:
                for line in lines:
                    match = re.match(pattern, line)
                    if match:
                        imports.append(match.group(1))
        
        # JavaScript/TypeScript imports
        elif language in ["javascript", "js", "typescript", "ts"]:
            patterns = [
                r"from\s+['\"]([^'\"]+)['\"]",
                r"require\(['\"]([^'\"]+)['\"]\)",
            ]
            for pattern in patterns:
                for line in lines:
                    matches = re.findall(pattern, line)
                    imports.extend(matches)
        
        # Java imports
        elif language == "java":
            pattern = r'^import\s+([\w\.]+);'
            for line in lines:
                match = re.match(pattern, line)
                if match:
                    imports.append(match.group(1))
        
        # Go imports
        elif language == "go":
            in_import_block = False
            for line in lines:
                if 'import (' in line:
                    in_import_block = True
                elif in_import_block and ')' in line:
                    in_import_block = False
                elif in_import_block or line.startswith('import '):
                    match = re.search(r'["\']([^"\']+)["\']', line)
                    if match:
                        imports.append(match.group(1))
        
        return imports
    
    async def get_critical_files(self) -> List[ImportedFile]:
        """
        Obtener archivos críticos del proyecto.
        
        Retorna archivos como:
        - package.json / requirements.txt
        - README.md
        - .env.example
        """
        critical = []
        
        for filename in self.CRITICAL_FILES:
            file_path = self.project_root / filename
            
            if file_path.exists() and file_path.is_file():
                try:
                    content = await self.read_file(file_path)
                    
                    if content:
                        # Detectar lenguaje
                        lang = self._detect_language(filename)
                        
                        critical.append(ImportedFile(
                            path=file_path,
                            language=lang,
                            content=content,
                            is_critical=True
                        ))
                
                except Exception as e:
                    logger.error(f"Failed to read critical file {filename}: {e}")
        
        return critical
    
    def _detect_language(self, filename: str) -> str:
        """Detectar lenguaje basado en extensión/nombre."""
        ext = Path(filename).suffix.lower()
        
        for lang, extensions in self.LANGUAGE_EXTENSIONS.items():
            if ext in extensions:
                return lang
        
        # Fallback based on filename
        if 'requirements' in filename:
            return 'python'
        elif 'package' in filename:
            return 'javascript'
        elif 'go.mod' in filename:
            return 'go'
        
        return 'unknown'
    
    async def build_context(self,
                          target_file: Optional[Path] = None,
                          language: Optional[str] = None,
                          max_context_length: int = 4000) -> str:
        """
        Construir contexto para el prompt.
        
        Args:
            target_file: Archivo target (para contexto específico)
            language: Lenguaje principal
            max_context_length: Máximo de caracteres en contexto
        
        Returns:
            String con contexto optimizado
        """
        context_parts = []
        current_length = 0
        
        # 1. Project tree overview
        if not self.project_tree:
            await self.build_project_tree()
        
        tree_str = self.project_tree.to_string(max_depth=2)
        tree_section = f"""
PROJECT STRUCTURE:
═════════════════
{tree_str}
"""
        context_parts.append(tree_section)
        current_length += len(tree_section)
        
        # 2. Critical files
        if current_length < max_context_length:
            critical_files = await self.get_critical_files()
            
            for file in critical_files[:3]:  # Limitar a 3 archivos críticos
                file_section = f"""
FILE: {file.path.relative_to(self.project_root)}
═════════════════════════
{file.content[:500]}...
"""
                if current_length + len(file_section) < max_context_length:
                    context_parts.append(file_section)
                    current_length += len(file_section)
        
        # 3. Target file context (si aplica)
        if target_file and target_file.exists():
            try:
                target_content = await self.read_file(target_file)
                if target_content:
                    target_section = f"""
TARGET FILE: {target_file.relative_to(self.project_root)}
═════════════════════════
{target_content[:500]}...
"""
                    if current_length + len(target_section) < max_context_length:
                        context_parts.append(target_section)
            
            except Exception as e:
                logger.error(f"Failed to read target file: {e}")
        
        return "\n".join(context_parts)
    
    def get_statistics(self) -> Dict[str, Any]:
        """Obtener estadísticas del proyecto."""
        if not self.project_tree:
            return {}
        
        return {
            "root": str(self.project_tree.root),
            "files": self.project_tree.file_count,
            "directories": len(self.project_tree.directories),
            "total_size_mb": self.project_tree.total_size_bytes / (1024 * 1024),
            "cache_size": len(self.file_cache),
            "cached_bytes": sum(len(c) for c in self.file_cache.values())
        }


# Singleton instance
_rag_instance: Optional[RAGLocal] = None


def get_rag_local(project_root: Optional[Path] = None) -> RAGLocal:
    """Obtener instancia singleton de RAG Local."""
    global _rag_instance
    if _rag_instance is None:
        _rag_instance = RAGLocal(project_root)
    return _rag_instance
