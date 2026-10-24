"""
Phase 10: The Watcher (Proactive Monitoring)
──────────────────────────────────────────────

The Watcher provides background error monitoring and proactive notifications.
It watches for errors in terminal/VS Code, searches the knowledge graph for
similar patterns, and notifies the user of solutions.

Features:
- Background monitoring loop
- Error pattern detection
- VisionService integration for screenshot + OCR
- GraphRAG queries to Neo4j for similar errors
- Auto-notification system
- Learning extraction for new errors

This module integrates:
- VisionService (core/vision_service.py)
- GraphRAG (core/graphrag_overlay.py)
- LearningExtractor (core/learning_extractor.py)
"""

import asyncio
import logging
import re
from typing import Optional, Dict, List, Any
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger("Watcher")


class ErrorSeverity(str, Enum):
    """Severity level of detected errors."""
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    CRITICAL = "critical"


@dataclass
class DetectedError:
    """An error detected in output."""
    error_text: str
    error_type: str  # e.g., "SyntaxError", "ConnectionError"
    severity: ErrorSeverity
    timestamp: float
    source: str  # "stderr", "stdout", "screenshot"
    context: Dict[str, Any]


@dataclass
class KnownErrorMatch:
    """A known error pattern from the knowledge graph."""
    error_pattern: str
    solution: str
    language: str
    confidence: float
    occurrence_count: int


class Watcher:
    """
    Monitors system activity for errors and provides proactive assistance.
    
    Runs in background and:
    1. Captures stderr/stdout or takes screenshots
    2. Detects error patterns
    3. Queries knowledge graph for similar errors
    4. Notifies user of solutions
    5. Extracts new errors to graph for learning
    """

    def __init__(
        self,
        vision_service=None,
        graphrag_overlay=None,
        learning_extractor=None,
        check_interval: float = 0.5,
    ):
        """
        Initialize the watcher.
        
        Args:
            vision_service: VisionService for screenshot/OCR
            graphrag_overlay: GraphRAG for knowledge queries
            learning_extractor: For extracting new errors to graph
            check_interval: How often to check for errors (seconds)
        """
        self.vision_service = vision_service
        self.graphrag_overlay = graphrag_overlay
        self.learning_extractor = learning_extractor
        self.check_interval = check_interval

        self.is_running = False
        self.monitor_task: Optional[asyncio.Task] = None
        self.detected_errors: List[DetectedError] = []
        self.stats = {
            "errors_detected": 0,
            "patterns_matched": 0,
            "solutions_found": 0,
            "notifications_sent": 0,
        }

        # Common error patterns
        self.error_patterns = {
            "SyntaxError": re.compile(r"SyntaxError|syntax error", re.IGNORECASE),
            "ImportError": re.compile(r"ImportError|import error", re.IGNORECASE),
            "ConnectionError": re.compile(r"ConnectionError|connection error|connection refused", re.IGNORECASE),
            "TimeoutError": re.compile(r"TimeoutError|timeout", re.IGNORECASE),
            "PermissionError": re.compile(r"PermissionError|permission denied", re.IGNORECASE),
            "ValueError": re.compile(r"ValueError|invalid value", re.IGNORECASE),
            "KeyError": re.compile(r"KeyError|key error", re.IGNORECASE),
            "DatabaseError": re.compile(r"database error|db error|sql error", re.IGNORECASE),
        }

    def _classify_error_type(self, error_text: str) -> str:
        """Classify error type based on patterns."""
        for error_type, pattern in self.error_patterns.items():
            if pattern.search(error_text):
                return error_type
        return "UnknownError"

    def _classify_severity(self, error_text: str) -> ErrorSeverity:
        """Classify error severity."""
        critical_keywords = ["crash", "fatal", "segfault", "panic", "critical"]
        high_keywords = ["error", "failed", "exception", "abort"]
        medium_keywords = ["warning", "deprecated", "outdated"]

        error_lower = error_text.lower()

        if any(kw in error_lower for kw in critical_keywords):
            return ErrorSeverity.CRITICAL
        elif any(kw in error_lower for kw in high_keywords):
            return ErrorSeverity.HIGH
        elif any(kw in error_lower for kw in medium_keywords):
            return ErrorSeverity.MEDIUM
        else:
            return ErrorSeverity.LOW

    async def capture_stderr_output(self, output: str) -> List[DetectedError]:
        """
        Capture and analyze stderr output.
        
        Args:
            output: Raw stderr/stdout text
            
        Returns:
            List of detected errors
        """
        import time
        errors = []

        # Split by lines and search for errors
        for line in output.split("\n"):
            if not line.strip():
                continue

            # Check if line looks like an error
            if any(pattern.search(line) for pattern in self.error_patterns.values()):
                error_type = self._classify_error_type(line)
                severity = self._classify_severity(line)

                error = DetectedError(
                    error_text=line,
                    error_type=error_type,
                    severity=severity,
                    timestamp=time.time(),
                    source="stderr",
                    context={"language": "unknown"},
                )

                errors.append(error)
                self.detected_errors.append(error)
                self.stats["errors_detected"] += 1
                logger.info(f"Detected error: {error_type} - {line[:100]}")

        return errors

    async def capture_screenshot(self) -> List[DetectedError]:
        """
        Capture screenshot and detect errors via OCR.
        """
        if not self.vision_service:
            return []

        try:
            # Take screenshot of active window
            screenshot_text = await self.vision_service.capture_active_window()

            # Analyze for errors
            return await self.capture_stderr_output(screenshot_text)

        except Exception as e:
            logger.warning(f"Screenshot capture failed: {e}")
            return []

    async def query_similar_errors(self, error: DetectedError) -> List[KnownErrorMatch]:
        """
        Query knowledge graph for similar errors.
        
        Returns:
            List of known error matches with solutions
        """
        if not self.graphrag_overlay:
            return []

        try:
            # Query for similar errors in the graph
            matches = await self.graphrag_overlay.query_cross_language_errors(
                error_pattern=error.error_type,
                source_language="python",  # Default
                target_languages=["python", "javascript", "java", "go"],
            )

            # Convert to KnownErrorMatch objects
            known_errors = []
            for match in matches or []:
                known_errors.append(
                    KnownErrorMatch(
                        error_pattern=match.get("error", ""),
                        solution=match.get("solution", ""),
                        language=match.get("language", "unknown"),
                        confidence=match.get("confidence", 0.0),
                        occurrence_count=match.get("count", 0),
                    )
                )

            self.stats["patterns_matched"] += len(known_errors)
            return known_errors

        except Exception as e:
            logger.warning(f"Graph query failed: {e}")
            return []

    async def notify_user(self, error: DetectedError, solutions: List[KnownErrorMatch]) -> bool:
        """
        Notify user of detected error and solutions.
        
        Returns:
            True if notification was successful
        """
        if not solutions:
            return False

        # Format notification
        notification = f"""
🚨 WATCHER: Detected {error.error_type} ({error.severity.value})
Error: {error.error_text[:100]}

Found {len(solutions)} similar errors in knowledge base:
"""

        for i, solution in enumerate(solutions[:3], 1):
            notification += f"\n{i}. [{solution.language}] {solution.solution[:80]}"
            notification += f" (Confidence: {solution.confidence:.0%})"

        logger.info(f"Notification: {notification}")
        self.stats["notifications_sent"] += 1
        self.stats["solutions_found"] += len(solutions)

        return True

    async def extract_and_learn_error(self, error: DetectedError) -> bool:
        """
        Extract new error to knowledge graph for learning.
        """
        if not self.learning_extractor:
            return False

        try:
            # Extract error as learning
            persisted = await self.learning_extractor.extract_and_persist_learnings(
                task_id=f"error-{error.timestamp}",
                task_title=f"Error Detection: {error.error_type}",
                result_text=f"{error.error_type}: {error.error_text}",
            )

            return persisted is not None

        except Exception as e:
            logger.warning(f"Failed to persist error learning: {e}")
            return False

    async def start_monitoring(self) -> None:
        """Start background monitoring loop."""
        if self.is_running:
            logger.warning("Watcher already running")
            return

        self.is_running = True
        self.monitor_task = asyncio.create_task(self._monitor_loop())
        logger.info("Watcher started")

    async def stop_monitoring(self) -> None:
        """Stop background monitoring loop."""
        self.is_running = False

        if self.monitor_task:
            self.monitor_task.cancel()
            try:
                await self.monitor_task
            except asyncio.CancelledError:
                pass

        logger.info("Watcher stopped")

    async def _monitor_loop(self) -> None:
        """Main monitoring loop (runs in background)."""
        while self.is_running:
            try:
                # Capture screenshot periodically
                errors = await self.capture_screenshot()

                for error in errors:
                    # Query for solutions
                    solutions = await self.query_similar_errors(error)

                    if solutions:
                        # Notify user
                        await self.notify_user(error, solutions)

                    # Learn new errors
                    await self.extract_and_learn_error(error)

                # Wait before next check
                await asyncio.sleep(self.check_interval)

            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Monitor loop error: {e}")
                await asyncio.sleep(self.check_interval)

    def get_stats(self) -> Dict[str, Any]:
        """Get watcher statistics."""
        return self.stats.copy()

    async def reset_stats(self) -> None:
        """Reset statistics."""
        self.stats = {
            "errors_detected": 0,
            "patterns_matched": 0,
            "solutions_found": 0,
            "notifications_sent": 0,
        }


# Singleton instance
_watcher_instance: Optional[Watcher] = None


def get_watcher(
    vision_service=None,
    graphrag_overlay=None,
    learning_extractor=None,
) -> Watcher:
    """
    Get or create the watcher singleton.
    
    Args:
        vision_service: VisionService instance
        graphrag_overlay: GraphRAG instance
        learning_extractor: LearningExtractor instance
        
    Returns:
        Watcher singleton
    """
    global _watcher_instance

    if _watcher_instance is None:
        _watcher_instance = Watcher(
            vision_service=vision_service,
            graphrag_overlay=graphrag_overlay,
            learning_extractor=learning_extractor,
        )

    return _watcher_instance


def reset_watcher() -> None:
    """Reset the watcher singleton (for testing)."""
    global _watcher_instance
    _watcher_instance = None
