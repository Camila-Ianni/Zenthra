import asyncio
import re
import threading
import json
import sys
import traceback
import logging
from pathlib import Path

import sounddevice as sd
from google import genai
from google.genai import types
from ui import JarvisUI
from memory.memory_manager import (
    load_memory, update_memory, format_memory_for_prompt,
)

from actions.flight_finder     import flight_finder
from actions.open_app          import open_app
from actions.weather_report    import weather_action
from actions.send_message      import send_message
from actions.reminder          import reminder
from actions.computer_settings import computer_settings
from actions.screen_processor  import screen_process
from actions.youtube_video     import youtube_video
from actions.desktop           import desktop_control
from actions.browser_control   import browser_control
from actions.file_controller   import file_controller
from actions.code_helper       import code_helper
from actions.dev_agent         import dev_agent
from actions.web_search        import web_search as web_search_action
from actions.computer_control  import computer_control
from actions.game_updater      import game_updater
from actions.clap_listener import ClapListener, on_clap_detected
from typing import Optional, Union, Dict, Any

# ── Phase 4: MCP Infrastructure Integration ────────────────────────────────
from core.mcp_orchestration_adapter import (
    get_mcp_orchestration_adapter,
    setup_mcp_with_orchestrator,
)
from core.orchestrator import Orchestrator
from core.agent_registry import get_agent_registry
from core.global_state import get_global_state

# ── Phase 5: Model Router & AI Enhancement ──────────────────────────────────
from core.model_router import get_model_router, TaskComplexity, ExecutionTrace
from core.language_experts import get_language_expert_provider
from core.linter_agent import get_linter_agent, IssueSeverity
from core.rag_local import get_rag_local
import time



def get_base_dir():
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent


BASE_DIR        = get_base_dir()
API_CONFIG_PATH = BASE_DIR / "config" / "api_keys.json"
PROMPT_PATH     = BASE_DIR / "core" / "prompt.txt"
LIVE_MODEL = "models/gemini-2.5-flash-native-audio-latest"
CHANNELS            = 1
SEND_SAMPLE_RATE    = 16000
RECEIVE_SAMPLE_RATE = 24000
CHUNK_SIZE          = 1024

# ── Phase 5: Logging configuration ──────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format='[%(name)s] %(levelname)s: %(message)s'
)
logger = logging.getLogger("JARVIS")


def _get_api_key() -> str:
    import os
    env_key = os.getenv("GOOGLE_API_KEY")
    if env_key and env_key.strip():
        return env_key.strip()
    try:
        with open(API_CONFIG_PATH, "r", encoding="utf-8") as f:
            return json.load(f).get("gemini_api_key", "")
    except Exception:
        return ""


def _load_system_prompt() -> str:
    try:
        return PROMPT_PATH.read_text(encoding="utf-8")
    except Exception:
        return (
            "You are JARVIS, Tony Stark's AI assistant. "
            "Be concise, direct, and always use the provided tools to complete tasks. "
            "Never simulate or guess results — always call the appropriate tool."
        )


# ── Transkripsiyon temizleyici ─────────────────────────────────────────────────
_CTRL_RE = re.compile(r"<ctrl\d+>", re.IGNORECASE)

def _clean_transcript(text: str) -> str:
    """Gemini'nin ürettiği <ctrlXX> artefaktlarını ve kontrol karakterlerini temizler."""
    text = _CTRL_RE.sub("", text)
    text = re.sub(r"[\x00-\x08\x0b-\x1f]", "", text)
    return text.strip()


# ── Tool declarations ──────────────────────────────────────────────────────────
TOOL_DECLARATIONS = [
    {
        "name": "open_app",
        "description": (
            "Opens any application on the computer. "
            "Use this whenever the user asks to open, launch, or start any app, "
            "website, or program. Always call this tool — never just say you opened it."
        ),
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "app_name": {
                    "type": "STRING",
                    "description": "Exact name of the application (e.g. 'WhatsApp', 'Chrome', 'Spotify')"
                }
            },
            "required": ["app_name"]
        }
    },
    {
        "name": "web_search",
        "description": "Searches the web for any information.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "query":  {"type": "STRING", "description": "Search query"},
                "mode":   {"type": "STRING", "description": "search (default) or compare"},
                "items":  {"type": "ARRAY", "items": {"type": "STRING"}, "description": "Items to compare"},
                "aspect": {"type": "STRING", "description": "price | specs | reviews"}
            },
            "required": ["query"]
        }
    },
    {
        "name": "weather_report",
        "description": "Gives the weather report to user",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "city": {"type": "STRING", "description": "City name"}
            },
            "required": ["city"]
        }
    },
    {
        "name": "send_message",
        "description": "Sends a text message via WhatsApp, Telegram, or other messaging platform.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "receiver":     {"type": "STRING", "description": "Recipient contact name"},
                "message_text": {"type": "STRING", "description": "The message to send"},
                "platform":     {"type": "STRING", "description": "Platform: WhatsApp, Telegram, etc."}
            },
            "required": ["receiver", "message_text", "platform"]
        }
    },
    {
        "name": "reminder",
        "description": "Sets a timed reminder using Task Scheduler.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "date":    {"type": "STRING", "description": "Date in YYYY-MM-DD format"},
                "time":    {"type": "STRING", "description": "Time in HH:MM format (24h)"},
                "message": {"type": "STRING", "description": "Reminder message text"}
            },
            "required": ["date", "time", "message"]
        }
    },
    {
        "name": "youtube_video",
        "description": (
            "Controls YouTube. Use for: playing videos, summarizing a video's content, "
            "getting video info, or showing trending videos."
        ),
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "action": {"type": "STRING", "description": "play | summarize | get_info | trending (default: play)"},
                "query":  {"type": "STRING", "description": "Search query for play action"},
                "save":   {"type": "BOOLEAN", "description": "Save summary to Notepad (summarize only)"},
                "region": {"type": "STRING", "description": "Country code for trending e.g. TR, US"},
                "url":    {"type": "STRING", "description": "Video URL for get_info action"},
            },
            "required": []
        }
    },
    {
        "name": "code_helper",
        "description": "Writes, edits, explains, runs, or builds code files.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "action":      {"type": "STRING", "description": "write | edit | explain | run | build | auto (default: auto)"},
                "description": {"type": "STRING", "description": "What the code should do or what change to make"},
                "language":    {"type": "STRING", "description": "Programming language (default: python)"},
                "output_path": {"type": "STRING", "description": "Where to save the file"},
                "file_path":   {"type": "STRING", "description": "Path to existing file for edit/explain/run/build"},
                "code":        {"type": "STRING", "description": "Raw code string for explain"},
                "args":        {"type": "STRING", "description": "CLI arguments for run/build"},
                "timeout":     {"type": "INTEGER", "description": "Execution timeout in seconds (default: 30)"},
            },
            "required": ["action"]
        }
    },
    {
        "name": "dev_agent",
        "description": "Creates full projects with multiple languages and frameworks. Creates folder structure and files.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "language":    {"type": "STRING", "description": "Programming language: python | nodejs | php | java | go"},
                "project_name": {"type": "STRING", "description": "Project name"},
                "description": {"type": "STRING", "description": "Full project description"}
            },
            "required": ["language", "project_name"]
        }
    },
    {
        "name": "agent_task",
        "description": "Runs a complex task using multi-step planning and execution.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "description": {"type": "STRING", "description": "Detailed description of the task"},
                "language":    {"type": "STRING", "description": "Programming language if applicable"}
            },
            "required": ["description"]
        }
    },
    {
        "name": "computer_control",
        "description": "Advanced computer control: type, click, screenshot, find files, manage windows, etc.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "action": {"type": "STRING", "description": "Action to perform: type, click, screenshot, scroll, press, hotkey, etc."},
                "text": {"type": "STRING", "description": "Text to type or paste"},
                "x": {"type": "INTEGER", "description": "X coordinate for mouse"},
                "y": {"type": "INTEGER", "description": "Y coordinate for mouse"},
                "description": {"type": "STRING", "description": "Natural language description of UI element to find"},
                "key": {"type": "STRING", "description": "Key name for press action"},
                "keys": {"type": "STRING", "description": "Key combination for hotkey (e.g., 'ctrl+c')"},
                "direction": {"type": "STRING", "description": "Direction: up, down, left, right"},
                "title": {"type": "STRING", "description": "Window title to focus"},
                "path": {"type": "STRING", "description": "Screenshot save path"}
            },
            "required": ["action"]
        }
    },
    {
        "name": "game_updater",
        "description": "Checks, downloads, and installs game updates and patches.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "game_name": {"type": "STRING", "description": "Name of the game"}
            },
            "required": ["game_name"]
        }
    },
    {
        "name": "flight_finder",
        "description": "Searches for flights, shows prices, and books tickets.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "from_city": {"type": "STRING", "description": "Departure city"},
                "to_city":   {"type": "STRING", "description": "Destination city"},
                "date":      {"type": "STRING", "description": "Travel date (YYYY-MM-DD)"},
                "action":    {"type": "STRING", "description": "search | book"}
            },
            "required": ["from_city", "to_city"]
        }
    },
    {
        "name": "shutdown_jarvis",
        "description": "Shuts down JARVIS completely.",
        "parameters": {
            "type": "OBJECT",
            "properties": {}
        }
    },
    {
        "name": "save_memory",
        "description": "Saves important information to persistent memory for future sessions.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "key":   {"type": "STRING", "description": "Memory key"},
                "value": {"type": "STRING", "description": "Value to remember"}
            },
            "required": ["key", "value"]
        }
    },
    {
        "name": "search_code",
        "description": "Searches for functions, classes, or patterns in code. Use to find where specific functionality already exists.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "query": {"type": "STRING", "description": "Search term: function name, class name, or pattern"},
                "path":  {"type": "STRING", "description": "Directory to search (default: project root)"}
            },
            "required": ["query"]
        }
    },
    {
        "name": "read_multiple_files",
        "description": "Reads content from multiple files in one operation.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "paths": {
                    "type": "ARRAY",
                    "items": {"type": "STRING"},
                    "description": "Array of file paths to read"
                },
                "max_size": {
                    "type": "INTEGER",
                    "description": "Max bytes per file (default: 50000)"
                }
            },
            "required": ["paths"]
        }
    },
    {
        "name": "file_controller",
        "description": "File and folder management: list, read, write, find, create, delete, organize files.",
        "parameters": {
            "type": "OBJECT",
            "properties": {
                "action": {"type": "STRING", "description": "list | read | write | find | create_file | create_folder | delete | move | copy | rename | organize_desktop | disk_usage | info"},
                "path": {"type": "STRING", "description": "Directory path (desktop, downloads, documents, home, etc.)"},
                "name": {"type": "STRING", "description": "File or folder name"},
                "content": {"type": "STRING", "description": "Content for write action"},
                "destination": {"type": "STRING", "description": "Destination for move/copy"},
                "extension": {"type": "STRING", "description": "File extension filter for find"},
                "max_results": {"type": "INTEGER", "description": "Max results for find (default: 20)"},
                "new_name": {"type": "STRING", "description": "New name for rename action"}
            },
            "required": ["action"]
        }
    },
]



class JarvisLive:

    def __init__(self, ui: JarvisUI):
        self.ui             = ui
        self.session        = None
        self.audio_in_queue = None
        self.out_queue      = None
        self._loop          = None
        self._is_speaking   = False
        self._speaking_lock = threading.Lock()
        self.ui.on_text_command = self._on_text_command
        self._turn_done_event: asyncio.Optional[Event] = None
        
        # ────── PHASE 6: Initialize Three Pillars ──────────────
        self._initialize_phase6()
        
        # ────── PHASE 7: Initialize Neo4j GraphRAG ──────────────
        self._initialize_phase7()
    
    def _initialize_phase6(self) -> None:
        """Initialize Phase 6 three-pillar architecture"""
        try:
            from core.swarm_orchestrator import SwarmOrchestrator
            from core.knowledge_graph import KnowledgeGraph
            from core.kimi_supervisor import KimiSupervisor
            from actions.antigravity_skills import AntigravitySkills
            
            self.swarm_orchestrator = SwarmOrchestrator()
            self.knowledge_graph = KnowledgeGraph(storage_path="memory/knowledge_graph.json")
            self.kimi_supervisor = KimiSupervisor(blackboard_path="memory/blackboard.json")
            self.antigravity_skills = AntigravitySkills()
            
            logger.info("✅ Phase 6: All three pillars initialized")
            logger.info("   🧠 OpenMythos (Swarm Orchestration) ✓")
            logger.info("   🦾 Antigravity (Mac OS Skills) ✓")
            logger.info("   📑 Kimi/Moonshot (Long-term Memory) ✓")
            
        except Exception as e:
            logger.warning(f"⚠️ Phase 6 initialization failed: {str(e)}")
            self.swarm_orchestrator = None
            self.knowledge_graph = None
            self.kimi_supervisor = None
            self.antigravity_skills = None
    
    def _initialize_phase7(self) -> None:
        """Initialize Phase 7 Neo4j GraphRAG system"""
        try:
            from core.graph_db_manager import Neo4jGraphDB, get_graph_db
            from core.graphrag_overlay import GraphRAG, get_graphrag
            from core.learning_extractor import LearningExtractor
            
            # These will be initialized asynchronously in run()
            self.graph_db = None
            self.graphrag = None
            self.learning_extractor = None
            
            logger.info("✅ Phase 7: GraphRAG system registered for async initialization")
            logger.info("   🗄️ Graph Database (Neo4j + JSON fallback) ✓")
            logger.info("   🧠 GraphRAG Overlay (Semantic Intelligence) ✓")
            logger.info("   📚 Learning Extractor (Memory Persistence) ✓")
            
        except Exception as e:
            logger.warning(f"⚠️ Phase 7 initialization deferred: {str(e)}")

    def _on_text_command(self, text: str):
        if not self._loop or not self.session:
            return
        asyncio.run_coroutine_threadsafe(
            self.session.send_client_content(
                turns={"parts": [{"text": text}]},
                turn_complete=True
            ),
            self._loop
        )

    def set_speaking(self, value: bool):
        with self._speaking_lock:
            self._is_speaking = value
        if value:
            self.ui.set_state("SPEAKING")
        elif not self.ui.muted:
            self.ui.set_state("LISTENING")

    def speak(self, text: str):
        if not self._loop or not self.session:
            return
        asyncio.run_coroutine_threadsafe(
            self.session.send_client_content(
                turns={"parts": [{"text": text}]},
                turn_complete=True
            ),
            self._loop
        )

    def speak_error(self, tool_name: str, error: str):
        short = str(error)[:120]
        self.ui.write_log(f"ERR: {tool_name} — {short}")
        self.speak(f"Sir, {tool_name} encountered an error. {short}")

    def _build_config(self) -> types.LiveConnectConfig:
        from datetime import datetime

        memory     = load_memory()
        mem_str    = format_memory_for_prompt(memory)
        sys_prompt = _load_system_prompt()

        now      = datetime.now()
        time_str = now.strftime("%A, %B %d, %Y — %I:%M %p")
        time_ctx = (
            f"[CURRENT DATE & TIME]\n"
            f"Right now it is: {time_str}\n"
            f"Use this to calculate exact times for reminders.\n\n"
        )

        parts = [time_ctx]
        if mem_str:
            parts.append(mem_str)
        parts.append(sys_prompt)

        return types.LiveConnectConfig(
            response_modalities=["AUDIO"],
            output_audio_transcription={},
            input_audio_transcription={},
            system_instruction="\n".join(parts),
            tools=[{"function_declarations": TOOL_DECLARATIONS}],
            session_resumption=types.SessionResumptionConfig(),
            speech_config=types.SpeechConfig(
                voice_config=types.VoiceConfig(
                    prebuilt_voice_config=types.PrebuiltVoiceConfig(
                        voice_name="Charon"
                    )
                )
            ),
        )

    async def _execute_tool_dynamic(self, tool_name: str, params: Dict[str, Any]) -> Optional[str]:
        """Phase 4: Route tool execution through MCP adapter (if available)
        
        NOTE: This is a fallback. Most tools are handled in legacy execution below.
        MCP should only handle tools it fully supports.
        """
        # Skip MCP for known tools with special signatures
        known_legacy_tools = {
            'open_app', 'code_helper', 'weather_report', 'browser_control',
            'file_controller', 'send_message', 'reminder', 'youtube_video',
            'screen_process', 'computer_settings', 'desktop_control', 'dev_agent',
            'agent_task', 'web_search', 'computer_control', 'game_updater',
            'flight_finder', 'list_files_recursive', 'search_code',
            'read_multiple_files', 'get_file_stats', 'shutdown_jarvis'
        }
        
        if tool_name in known_legacy_tools:
            # Skip MCP for known tools - they'll be handled by legacy execution
            return None
        
        try:
            if hasattr(self, 'mcp_adapter'):
                result = self.mcp_adapter.execute_tool_unified(tool_name, params)
                if result.get("success"):
                    # Track in global state
                    if hasattr(self, 'global_state'):
                        self.global_state.set_context_var(f"last_tool: {tool_name}", {
                            "success": True,
                            "params": params
                        })
                    return result.get("output", "Tool executed successfully")
                else:
                    error_msg = result.get("error", "Tool execution failed")
                    print(f"[JARVIS] ❌ Tool error: {error_msg}")
                    return f"Error: {error_msg}"
        except Exception as e:
            print(f"[JARVIS] ⚠️ MCP tool execution failed: {e}")
        return None

    async def _execute_code_with_router(self, 
                                       code: str,
                                       language: str,
                                       task_description: str = "",
                                       code_length: int = 0) -> Dict[str, Any]:
        """
        Phase 5: Execute code through Model Router with linting.
        
        Args:
            code: Generated code to process
            language: Programming language
            task_description: What the code does
            code_length: Estimated code length
        
        Returns:
            Dict with linting results and recommendations
        """
        if not self.model_router or not self.linter_agent:
            return {"success": False, "reason": "Model Router not available"}
        
        try:
            start_time = time.time()
            
            # 1. Lint the code
            lint_result = await self.linter_agent.lint_code(
                code=code,
                language=language,
                filename="generated_code"
            )
            
            # 2. Try autocorrection if there are issues
            autocorrected = None
            if lint_result.issues:
                autocorrected = await self.linter_agent.autocorrect(
                    code=code,
                    language=language,
                    issues=lint_result.issues
                )
            
            # 3. Record in execution trace
            routing_decision = await self.model_router.route(
                task_type="quick_implementation",
                task_description=task_description,
                language=language,
                code_length=code_length,
                sensitive=False
            )
            
            trace = ExecutionTrace(
                task_id=f"code_{int(time.time() * 1000)}",
                routing_decision=routing_decision,
                model_used="linter_agent",
                provider_used="local",
                start_time=start_time,
                end_time=time.time(),
                success=lint_result.is_valid,
                error=None if lint_result.is_valid else "Linting issues found"
            )
            
            self.model_router.record_execution(trace)
            
            # 4. Format report
            report = self.linter_agent.format_report(lint_result)
            
            print(f"[JARVIS] 🧹 Linting complete: {lint_result.total_issues} issues")
            
            return {
                "success": lint_result.is_valid,
                "code": autocorrected or code,
                "lint_result": {
                    "total_issues": lint_result.total_issues,
                    "has_errors": lint_result.has_errors,
                    "has_warnings": lint_result.has_warnings
                },
                "report": report,
                "autocorrected": autocorrected is not None
            }
        
        except Exception as e:
            logger.error(f"Code router execution failed: {e}")
            return {
                "success": False,
                "code": code,
                "error": str(e)
            }

    async def _execute_tool(self, fc) -> types.FunctionResponse:
        name = fc.name
        args = dict(fc.args or {})

        print(f"[JARVIS] 🔧 {name}  {args}")
        self.ui.set_state("THINKING")

        # ── save_memory: sessiz ve hızlı ──────────────────────────────────────
        if name == "save_memory":
            category = args.get("category", "notes")
            key      = args.get("key", "")
            value    = args.get("value", "")
            if key and value:
                update_memory({category: {key: {"value": value}}})
                print(f"[Memory] 💾 save_memory: {category}/{key} = {value}")
            if not self.ui.muted:
                self.ui.set_state("LISTENING")
            return types.FunctionResponse(
                id=fc.id, name=name,
                response={"result": "ok", "silent": True}
            )

        loop   = asyncio.get_event_loop()
        result = "Done."

        try:
            # ── Phase 4: Try dynamic tool execution first ───────────────────────────
            dynamic_result = await self._execute_tool_dynamic(name, args)
            if dynamic_result is not None:
                result = dynamic_result
            # If dynamic execution failed or tool not in MCP, fall back to legacy execution
            elif name == "open_app":
                r = await loop.run_in_executor(None, lambda: open_app(parameters=args, response=None, player=self.ui))
                result = r or f"Opened {args.get('app_name')}."

            elif name == "weather_report":
                r = await loop.run_in_executor(None, lambda: weather_action(parameters=args, player=self.ui))
                result = r or "Weather delivered."

            elif name == "browser_control":
                r = await loop.run_in_executor(None, lambda: browser_control(parameters=args, player=self.ui))
                result = r or "Done."

            elif name == "file_controller":
                r = await loop.run_in_executor(None, lambda: file_controller(parameters=args, player=self.ui))
                result = r or "Done."

            elif name == "send_message":
                r = await loop.run_in_executor(None, lambda: send_message(parameters=args, response=None, player=self.ui, session_memory=None))
                result = r or f"Message sent to {args.get('receiver')}."

            elif name == "reminder":
                r = await loop.run_in_executor(None, lambda: reminder(parameters=args, response=None, player=self.ui))
                result = r or "Reminder set."

            elif name == "youtube_video":
                r = await loop.run_in_executor(None, lambda: youtube_video(parameters=args, response=None, player=self.ui))
                result = r or "Done."

            elif name == "screen_process":
                threading.Thread(
                    target=screen_process,
                    kwargs={"parameters": args, "response": None,
                            "player": self.ui, "session_memory": None},
                    daemon=True
                ).start()
                result = "Vision module activated. Stay completely silent — vision module will speak directly."

            elif name == "computer_settings":
                r = await loop.run_in_executor(None, lambda: computer_settings(parameters=args, response=None, player=self.ui))
                result = r or "Done."

            elif name == "desktop_control":
                r = await loop.run_in_executor(None, lambda: desktop_control(parameters=args, player=self.ui))
                result = r or "Done."

            elif name == "code_helper":
                r = await loop.run_in_executor(None, lambda: code_helper(parameters=args, player=self.ui, speak=self.speak))
                result = r or "Done."

            elif name == "dev_agent":
                r = await loop.run_in_executor(None, lambda: dev_agent(parameters=args, player=self.ui, speak=self.speak))
                result = r or "Done."

            elif name == "agent_task":
                from agent.task_queue import get_queue, TaskPriority
                priority_map = {"low": TaskPriority.LOW, "normal": TaskPriority.NORMAL, "high": TaskPriority.HIGH}
                priority = priority_map.get(args.get("priority", "normal").lower(), TaskPriority.NORMAL)
                task_id  = get_queue().submit(goal=args.get("goal", ""), priority=priority, speak=self.speak)
                result   = f"Task started (ID: {task_id})."

            elif name == "web_search":
                r = await loop.run_in_executor(None, lambda: web_search_action(parameters=args, player=self.ui))
                result = r or "Done."

            elif name == "computer_control":
                r = await loop.run_in_executor(None, lambda: computer_control(parameters=args, player=self.ui))
                result = r or "Done."

            elif name == "game_updater":
                r = await loop.run_in_executor(None, lambda: game_updater(parameters=args, player=self.ui, speak=self.speak))
                result = r or "Done."

            elif name == "flight_finder":
                r = await loop.run_in_executor(None, lambda: flight_finder(parameters=args, player=self.ui))
                result = r or "Done."

            elif name == "list_files_recursive":
                from actions.file_system_tools import list_files_recursive
                r = await loop.run_in_executor(None, lambda: list_files_recursive(parameters=args, player=self.ui))
                result = r or "Repository structure mapped."

            elif name == "search_code":
                from actions.file_system_tools import search_code
                r = await loop.run_in_executor(None, lambda: search_code(parameters=args, player=self.ui))
                result = r or "Search complete."

            elif name == "read_multiple_files":
                from actions.file_system_tools import read_multiple_files
                r = await loop.run_in_executor(None, lambda: read_multiple_files(parameters=args, player=self.ui))
                result = r or "Files read successfully."

            elif name == "get_file_stats":
                from actions.file_system_tools import get_file_stats
                r = await loop.run_in_executor(None, lambda: get_file_stats(parameters=args, player=self.ui))
                result = r or "Statistics gathered."

            elif name == "shutdown_jarvis":
                self.ui.write_log("SYS: Shutdown requested.")
                self.speak("Goodbye, sir.")
                def _shutdown():
                    import time, os
                    time.sleep(1)
                    os._exit(0)
                threading.Thread(target=_shutdown, daemon=True).start()

            else:
                result = f"Unknown tool: {name}"

        except Exception as e:
            result = f"Tool '{name}' failed: {e}"
            traceback.print_exc()
            self.speak_error(name, e)

        if not self.ui.muted:
            self.ui.set_state("LISTENING")

        print(f"[JARVIS] 📤 {name} → {str(result)[:80]}")
        return types.FunctionResponse(
            id=fc.id, name=name,
            response={"result": result}
        )

    async def _send_realtime(self):
        while True:
            msg = await self.out_queue.get()
            await self.session.send_realtime_input(media=msg)

    async def _listen_audio(self):
        print("[JARVIS] 🎤 Mic started")
        loop = asyncio.get_event_loop()

        def callback(indata, frames, time_info, status):
            with self._speaking_lock:
                jarvis_speaking = self._is_speaking
            if not jarvis_speaking and not self.ui.muted:
                data = indata.tobytes()
                loop.call_soon_threadsafe(
                    self.out_queue.put_nowait,
                    {"data": data, "mime_type": "audio/pcm"}
                )

        try:
            with sd.InputStream(
                samplerate=SEND_SAMPLE_RATE,
                channels=CHANNELS,
                dtype="int16",
                blocksize=CHUNK_SIZE,
                callback=callback,
            ):
                print("[JARVIS] 🎤 Mic stream open")
                while True:
                    await asyncio.sleep(0.1)
        except Exception as e:
            print(f"[JARVIS] ❌ Mic: {e}")
            raise

    async def _receive_audio(self):
        print("[JARVIS] 👂 Recv started")
        out_buf, in_buf = [], []

        try:
            while True:
                async for response in self.session.receive():

                    if response.data:
                        if self._turn_done_event and self._turn_done_event.is_set():
                            self._turn_done_event.clear()
                        self.audio_in_queue.put_nowait(response.data)

                    if response.server_content:
                        sc = response.server_content

                        if sc.output_transcription and sc.output_transcription.text:
                            txt = _clean_transcript(sc.output_transcription.text)
                            if txt:
                                out_buf.append(txt)

                        if sc.input_transcription and sc.input_transcription.text:
                            txt = _clean_transcript(sc.input_transcription.text)
                            if txt:
                                in_buf.append(txt)

                        if sc.turn_complete:
                            if self._turn_done_event:
                                self._turn_done_event.set()

                            full_in = " ".join(in_buf).strip()
                            if full_in:
                                self.ui.write_log(f"You: {full_in}")
                            in_buf = []

                            full_out = " ".join(out_buf).strip()
                            if full_out:
                                self.ui.write_log(f"Jarvis: {full_out}")
                            out_buf = []

                    if response.tool_call:
                        fn_responses = []
                        for fc in response.tool_call.function_calls:
                            print(f"[JARVIS] 📞 {fc.name}")
                            fr = await self._execute_tool(fc)
                            fn_responses.append(fr)
                        await self.session.send_tool_response(
                            function_responses=fn_responses
                        )

        except Exception as e:
            print(f"[JARVIS] ❌ Recv: {e}")
            traceback.print_exc()
            raise

    async def _play_audio(self):
        print("[JARVIS] 🔊 Play started")

        stream = sd.RawOutputStream(
            samplerate=RECEIVE_SAMPLE_RATE,
            channels=CHANNELS,
            dtype="int16",
            blocksize=CHUNK_SIZE,
        )
        stream.start()

        try:
            while True:
                try:
                    chunk = await asyncio.wait_for(
                        self.audio_in_queue.get(),
                        timeout=0.1
                    )
                except asyncio.TimeoutError:
                    if (
                        self._turn_done_event
                        and self._turn_done_event.is_set()
                        and self.audio_in_queue.empty()
                    ):
                        self.set_speaking(False)
                        self._turn_done_event.clear()
                    continue

                self.set_speaking(True)
                await asyncio.to_thread(stream.write, chunk)

        except Exception as e:
            print(f"[JARVIS] ❌ Play: {e}")
            raise
        finally:
            self.set_speaking(False)
            stream.stop()
            stream.close()

    async def run(self):
        # ── Phase 4: Initialize MCP Infrastructure ─────────────────────────────────
        print("\n[JARVIS] 🚀 Initializing MCP Infrastructure...")
        try:
            setup_mcp_with_orchestrator(auto_register=True)
            self.mcp_adapter = get_mcp_orchestration_adapter()
            self.orchestrator = Orchestrator(api_key=_get_api_key())
            self.agent_registry = get_agent_registry()
            self.global_state = get_global_state()
            print(f"[JARVIS] ✅ MCP Infrastructure ready: {self.mcp_adapter.get_statistics()['total_available']} tools loaded")
        except Exception as e:
            print(f"[JARVIS] ⚠️ MCP initialization failed: {e}")
            print("[JARVIS] ℹ️ Continuing with legacy tool execution...")
        
        # ── Phase 5: Initialize Model Router & AI Enhancement ────────────────────
        print("[JARVIS] 🤖 Initializing Model Router & AI Enhancement...")
        try:
            self.model_router = get_model_router()
            self.language_expert = get_language_expert_provider()
            self.linter_agent = get_linter_agent()
            self.rag_local = get_rag_local(Path.cwd())
            
            # Build project tree for context
            await self.rag_local.build_project_tree(max_files=50)
            rag_stats = self.rag_local.get_statistics()
            
            print(f"[JARVIS] ✅ Model Router ready: {len(self.model_router.config.get('providers', {}))} providers")
            print(f"[JARVIS] ✅ RAG Local ready: {rag_stats.get('files', 0)} files indexed")
            print(f"[JARVIS] 🧠 Language Experts: {', '.join(self.language_expert.get_supported_languages())}")
        except Exception as e:
            print(f"[JARVIS] ⚠️ Model Router initialization failed: {e}")
            print("[JARVIS] ℹ️ Continuing with standard Gemini fallback...")
            self.model_router = None
            self.language_expert = None
            self.linter_agent = None
            self.rag_local = None
        
        # ── Phase 7: Initialize GraphRAG (Neo4j + Memory Intelligence) ───────────────
        print("[JARVIS] 📊 Initializing GraphRAG Neo4j System...")
        try:
            from core.graph_db_manager import get_graph_db
            from core.graphrag_overlay import get_graphrag
            from core.learning_extractor import LearningExtractor
            
            # Initialize graph database
            self.graph_db = await get_graph_db()
            print(f"[JARVIS] ✅ Graph Database: {self.graph_db.is_neo4j_available and 'Neo4j' or 'JSON Fallback'}")
            
            # Initialize GraphRAG
            self.graphrag = await get_graphrag()
            graphrag_stats = await self.graphrag.get_statistics()
            print(f"[JARVIS] ✅ GraphRAG Overlay: {graphrag_stats.get('mode', 'unknown')} mode")
            
            # Initialize learning extractor
            self.learning_extractor = LearningExtractor(graph_db=self.graph_db)
            print("[JARVIS] ✅ Learning Extractor: Ready for memory persistence")
            
        except Exception as e:
            print(f"[JARVIS] ⚠️ GraphRAG initialization failed: {e}")
            print("[JARVIS] ℹ️ Continuing without GraphRAG memory persistence...")
            self.graph_db = None
            self.graphrag = None
            self.learning_extractor = None
        
        client = genai.Client(
            api_key=_get_api_key(),
            http_options={"api_version": "v1beta"}
        )

        while True:
            try:
                print("[JARVIS] 🔌 Connecting...")
                self.ui.set_state("THINKING")
                config = self._build_config()

                async with (
                    client.aio.live.connect(model=LIVE_MODEL, config=config) as session,
                    asyncio.TaskGroup() as tg,
                ):
                    self.session        = session
                    self._loop          = asyncio.get_event_loop()
                    self.audio_in_queue = asyncio.Queue()
                    self.out_queue      = asyncio.Queue(maxsize=10)
                    self._turn_done_event = asyncio.Event()

                    print("[JARVIS] ✅ Connected.")
                    self.ui.set_state("LISTENING")
                    self.ui.write_log("SYS: JARVIS online.")

                    tg.create_task(self._send_realtime())
                    tg.create_task(self._listen_audio())
                    tg.create_task(self._receive_audio())
                    tg.create_task(self._play_audio())

            except Exception as e:
                print(f"[JARVIS] ⚠️ {e}")
                traceback.print_exc()

            self.set_speaking(False)
            self.ui.set_state("THINKING")
            print("[JARVIS] 🔄 Reconnecting in 3s...")
            await asyncio.sleep(3)
    
    async def cleanup(self) -> None:
        """Clean up resources and persist graph data"""
        try:
            # Save graph data
            if self.graph_db:
                await self.graph_db.disconnect()
                print("[JARVIS] ✅ Graph database disconnected and data saved")
            
            # Close GraphRAG
            from core.graphrag_overlay import close_graphrag
            await close_graphrag()
            
            print("[JARVIS] ✅ GraphRAG resources cleaned up")
        except Exception as e:
            print(f"[JARVIS] ⚠️ Cleanup error: {e}")


def main():
    ui = JarvisUI("face.png")
    jarvis_instance = None
    jarvis_running = False
    music_played = False
    clap_listener = None

    def start_jarvis():
        """Inicia JARVIS (llamado desde el listener de aplausos o manualmente)."""
        nonlocal jarvis_instance, jarvis_running
        if jarvis_running:
            print("[JARVIS] ⚠️ Ya está corriendo.")
            return
        
        print("[JARVIS] 🚀 Iniciando JARVIS...")
        jarvis_running = True
        jarvis_instance = JarvisLive(ui)
        try:
            asyncio.run(jarvis_instance.run())
        except KeyboardInterrupt:
            print("\n🔴 Shutting down...")
        finally:
            jarvis_running = False

    def on_clap_activation():
        """Callback cuando se detectan dos aplausos: reproduce música + abre JARVIS."""
        nonlocal music_played, clap_listener

        # Solo hacer esto una vez
        if not music_played:
            print("\n" + "="*50)
            print("🎸 APLAUSOS DETECTADOS - REPRODUCIENDO MÚSICA 🎸")
            print("="*50)
            
            from actions.clap_listener import play_activation_song
            play_activation_song()
            music_played = True
            
            # Pequeña pausa dramática
            import time
            time.sleep(2)
            
            # Iniciar JARVIS
            ui.write_log("👏👏 Activado por aplausos")
            start_jarvis()
            
            # Detener el listener de aplausos (no queremos más detecciones)
            if clap_listener:
                clap_listener.stop()

    # Iniciar el listener de aplausos en un hilo separado
    print("🎤 Modo aplausos activado. Da dos aplausos para iniciar JARVIS...")
    print("   (También se iniciará automáticamente si prefieres)")
    
    clap_listener = ClapListener(on_clap_activation)
    clap_listener.start()

    # Iniciar JARVIS automáticamente después de 3 segundos si no hay aplausos
    def auto_start():
        import time
        time.sleep(3)
        if not jarvis_running:
            print("[JARVIS] 🚀 Inicio automático (sin aplausos detectados)")
            start_jarvis()
    
    threading.Thread(target=auto_start, daemon=True).start()
    
    ui.root.mainloop()
    
    # Limpiar al cerrar
    clap_listener.stop()


if __name__ == "__main__":
    main()
