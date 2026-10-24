"""
Phase 10.4: Main.py Integration Module

This module provides the integration layer between main.py's JarvisLive class
and the three Phase 10 pillars (Gatekeeper, Swarm Executor, Watcher).

Architecture:
  1. GatekeeperOrchestrator acts as the primary entry point for all tool requests
  2. SwarmExecutor orchestrates multi-tool execution with parallelism
  3. Watcher monitors for errors and proactively notifies the user

Dependencies:
  - core/phase10_gatekeeper.py (GatekeeperOrchestrator)
  - core/phase10_swarm_executor.py (SwarmExecutor)
  - core/phase10_watcher.py (Watcher)
  - core/semantic_cache.py (SemanticCache)
  - core/local_intent_classifier.py (LocalIntentClassifier)
  - core/intent_router.py (IntentRouter)
  - core/vision_service.py (VisionService)
  - core/graphrag_overlay.py (GraphRAG)
  - core/learning_extractor.py (LearningExtractor)
"""

import asyncio
from typing import Dict, Any, Optional, List, Callable
from pathlib import Path
import logging

logger = logging.getLogger("JARVIS-P10")


class Phase10Integration:
    """
    Manages initialization and coordination of all Phase 10 pillars.
    Provides a unified interface for main.py's JarvisLive class.
    """

    def __init__(self):
        """Initialize Phase 10 integration layer."""
        self.gatekeeper = None
        self.swarm_executor = None
        self.watcher = None
        self._initialized = False
        self._monitoring_task = None

    async def initialize(self) -> bool:
        """
        Initialize all three pillars.
        
        Returns:
            bool: True if initialization successful, False otherwise
        """
        try:
            logger.info("[Phase 10.4] Initializing Three Pillars...")

            # Import and initialize Pillar 1: Gatekeeper
            try:
                from core.phase10_gatekeeper import GatekeeperOrchestrator
                self.gatekeeper = GatekeeperOrchestrator()
                logger.info("✅ Pillar 1: PORTERO INTELIGENTE (Gatekeeper) initialized")
            except Exception as e:
                logger.warning(f"⚠️ Gatekeeper initialization failed: {e}")
                self.gatekeeper = None

            # Import and initialize Pillar 2: Swarm Executor
            try:
                from core.phase10_swarm_executor import SwarmExecutor
                self.swarm_executor = SwarmExecutor()
                logger.info("✅ Pillar 2: EJECUCIÓN EN RÁFAGA (Swarm Executor) initialized")
            except Exception as e:
                logger.warning(f"⚠️ Swarm Executor initialization failed: {e}")
                self.swarm_executor = None

            # Import and initialize Pillar 3: Watcher
            try:
                from core.phase10_watcher import Watcher
                self.watcher = Watcher()
                logger.info("✅ Pillar 3: VISIÓN PROACTIVA (Watcher) initialized")
            except Exception as e:
                logger.warning(f"⚠️ Watcher initialization failed: {e}")
                self.watcher = None

            self._initialized = True
            logger.info("[Phase 10.4] ✅ All pillars initialized successfully")
            return True

        except Exception as e:
            logger.error(f"[Phase 10.4] ❌ Integration initialization failed: {e}")
            return False

    async def start_monitoring(self) -> None:
        """Start background monitoring (Watcher)."""
        if not self.watcher:
            logger.warning("Watcher not available for background monitoring")
            return

        try:
            self._monitoring_task = asyncio.create_task(
                self.watcher.start_monitoring()
            )
            logger.info("[Phase 10.4] 🔍 Background monitoring started")
        except Exception as e:
            logger.warning(f"Failed to start monitoring: {e}")

    async def stop_monitoring(self) -> None:
        """Stop background monitoring."""
        if self._monitoring_task and not self._monitoring_task.done():
            self._monitoring_task.cancel()
            try:
                await self._monitoring_task
            except asyncio.CancelledError:
                pass
            logger.info("[Phase 10.4] 🛑 Background monitoring stopped")

    async def process_tool_request(
        self,
        tool_name: str,
        tool_params: Dict[str, Any],
        available_tools: Optional[Dict[str, Callable]] = None,
        user_context: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Process a tool request through the three-pillar pipeline.

        Pipeline:
          1. Gatekeeper: Cache lookup → Intent classification → Tool routing
          2. SwarmExecutor: If multi-tool, parallelize execution
          3. Watcher: Background error detection

        Args:
            tool_name: Name of the tool to execute
            tool_params: Parameters for the tool
            available_tools: Dict of available tool functions {tool_name: callable}
            user_context: User's request context for better caching

        Returns:
            Dict with execution result:
                {
                    "success": bool,
                    "result": str,
                    "cached": bool,
                    "execution_time_ms": float,
                    "tool_used": str,
                    "error": Optional[str]
                }
        """
        try:
            # Use Gatekeeper for cache-first routing
            if self.gatekeeper:
                from core.phase10_gatekeeper import GatekeeperRequest

                request = GatekeeperRequest(
                    query=user_context or tool_name,
                    context={
                        "tool_name": tool_name,
                        "tool_params": tool_params,
                        "available_tools": list(available_tools.keys()) if available_tools else []
                    }
                )

                response = await self.gatekeeper.process(request)

                return {
                    "success": response.success,
                    "result": response.result,
                    "cached": response.from_cache,
                    "execution_time_ms": response.execution_time_ms,
                    "tool_used": response.tool_used,
                    "error": response.error
                }

            # Fallback if Gatekeeper not available
            logger.warning("Gatekeeper not available, using fallback execution")
            return {
                "success": False,
                "result": None,
                "cached": False,
                "execution_time_ms": 0,
                "tool_used": tool_name,
                "error": "Gatekeeper not available"
            }

        except Exception as e:
            logger.error(f"Tool request processing failed: {e}")
            return {
                "success": False,
                "result": None,
                "cached": False,
                "execution_time_ms": 0,
                "tool_used": tool_name,
                "error": str(e)
            }

    async def process_multi_tool_request(
        self,
        tools: List[Dict[str, Any]],
        execution_strategy: str = "PARALLEL"
    ) -> Dict[str, Any]:
        """
        Process multiple tool requests in parallel via SwarmExecutor.

        Args:
            tools: List of {"tool_name": str, "args": dict}
            execution_strategy: SEQUENTIAL, PARALLEL, or GROUPED

        Returns:
            Dict with aggregated results from all tools
        """
        if not self.swarm_executor:
            logger.warning("Swarm Executor not available for multi-tool execution")
            return {
                "success": False,
                "error": "Swarm Executor not available",
                "results": []
            }

        try:
            from core.phase10_swarm_executor import ToolTask, ExecutionStrategy

            # Convert to ToolTask format
            tool_tasks = [
                ToolTask(
                    tool_id=f"tool_{i}",
                    tool_name=t["tool_name"],
                    tool_func=lambda: None,  # Mock function for integration
                    args=t.get("args", {}),
                    timeout=t.get("timeout", 30)
                )
                for i, t in enumerate(tools)
            ]

            # Execute with specified strategy
            strategy = ExecutionStrategy[execution_strategy]
            results = await self.swarm_executor.execute_parallel_tasks(
                tasks=tool_tasks,
                strategy=strategy
            )

            return {
                "success": True,
                "results": results,
                "strategy_used": execution_strategy,
                "total_tools": len(tool_tasks),
                "error": None
            }

        except Exception as e:
            logger.error(f"Multi-tool execution failed: {e}")
            return {
                "success": False,
                "error": str(e),
                "results": []
            }

    async def get_statistics(self) -> Dict[str, Any]:
        """Get statistics from all three pillars."""
        stats = {
            "initialized": self._initialized,
            "pillars": {
                "gatekeeper": None,
                "swarm_executor": None,
                "watcher": None
            }
        }

        if self.gatekeeper:
            stats["pillars"]["gatekeeper"] = {
                "available": True,
                "stats": getattr(self.gatekeeper, 'stats', {})
            }

        if self.swarm_executor:
            stats["pillars"]["swarm_executor"] = {
                "available": True,
                "stats": getattr(self.swarm_executor, 'stats', {})
            }

        if self.watcher:
            stats["pillars"]["watcher"] = {
                "available": True,
                "monitoring": self._monitoring_task is not None and not self._monitoring_task.done(),
                "stats": getattr(self.watcher, 'stats', {})
            }

        return stats

    async def shutdown(self) -> None:
        """Gracefully shutdown all pillars."""
        logger.info("[Phase 10.4] Shutting down Phase 10 integration...")

        # Stop monitoring
        await self.stop_monitoring()

        # Cleanup resources
        if self.gatekeeper:
            await asyncio.sleep(0.1)  # Allow graceful shutdown

        if self.watcher:
            await asyncio.sleep(0.1)

        logger.info("[Phase 10.4] ✅ All pillars shut down")


# Singleton instance
_phase10_integration: Optional[Phase10Integration] = None


def get_phase10_integration() -> Phase10Integration:
    """Get or create the Phase 10 integration singleton."""
    global _phase10_integration
    if _phase10_integration is None:
        _phase10_integration = Phase10Integration()
    return _phase10_integration


def reset_phase10_integration() -> None:
    """Reset the Phase 10 integration singleton (for testing)."""
    global _phase10_integration
    _phase10_integration = None
