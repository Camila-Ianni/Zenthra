# PHASE 4 ROADMAP: Main.py Integration

## 🎯 Objective
Integrar el sistema MCP completo con main.py para que el Orchestrator dinámico sea el núcleo de JARVIS.

---

## 📋 Integration Steps

### Step 1: Import Core Components
**Location:** `main.py` (lines 1-50)

```python
# Add after existing imports:
from core.mcp_orchestration_adapter import (
    get_mcp_orchestration_adapter,
    setup_mcp_with_orchestrator
)
from core.orchestrator import Orchestrator
from core.agent_registry import get_agent_registry
from core.global_state import get_global_state
```

### Step 2: Initialize MCP at Startup
**Location:** `main.py` → `async def run()` (around line 885)

```python
async def run():
    print("\n[JARVIS] 🚀 Initializing MCP Infrastructure...")
    setup_mcp_with_orchestrator(auto_register=True)
    
    adapter = get_mcp_orchestration_adapter()
    print(f"[JARVIS] ✅ {adapter.get_statistics()['total_available']} tools loaded")
    
    # Rest of initialization...
```

### Step 3: Register Agent Capabilities
**Location:** `main.py` → after MCP initialization

```python
registry = get_agent_registry()

# Register entertainment tools
registry.add_capability("entertainment", "play_music", "spotify_player")
registry.add_capability("entertainment", "take_screenshot", "screenshot")

# Register coder tools
registry.add_capability("coder", "write_code", "code_helper")
registry.add_capability("coder", "execute", "executor")

# etc...
```

### Step 4: Replace Tool Execution Loop
**Location:** `main.py` → `_receive_audio()` loop (around line 600-827)

**Before:**
```python
if result.function_calls:
    for fc in result.function_calls:
        tool_name = fc.name
        params = fc.args
        if tool_name == "code_helper":
            # ...execute code_helper
        elif tool_name == "web_search":
            # ...execute web_search
        # ... 50+ elif statements (HARDCODED)
```

**After:**
```python
if result.function_calls:
    for fc in result.function_calls:
        tool_name = fc.name
        params = fc.args
        
        # Use Orchestrator's executor
        result = orchestrator.run(
            user_input=f"Execute {tool_name}",
            tool_params=params,
            tool_executor=adapter.get_orchestrator_tool_executor()
        )
        
        # Handle response...
```

### Step 5: Connect Global State
**Location:** `main.py` → anywhere we track execution

```python
from core.global_state import get_global_state

state = get_global_state()

# Track user commands
state.set_context_var("last_user_input", transcription)
state.set_context_var("last_tool_used", tool_name)

# Get insights for next planning
insights = state.get_agent_insights()
```

---

## 🔧 Code Changes Needed

### File: `main.py`
**Sections to modify:**

1. **Lines 1-50** (Imports)
   - Add MCP imports

2. **Lines 79-520** (Tool declarations)
   - Keep as-is for now (backward compat)
   - Add comment: "These will be replaced by dynamic loading"

3. **Lines 885-900** (run() startup)
   - Call setup_mcp_with_orchestrator()
   - Initialize Orchestrator
   - Print statistics

4. **Lines 597-827** (_receive_audio loop)
   - Replace hardcoded tool if/elif with dynamic executor

5. **Lines 850-884** (Response handling)
   - Add global state tracking

---

## 🧪 Testing Strategy

### Test 1: Tool Discovery
```python
# Verify all tools are loaded
adapter = get_mcp_orchestration_adapter()
tools = adapter.list_available_tools()
assert len(tools) > 10, "Tools not loaded"
```

### Test 2: Dynamic Execution
```python
# Verify tools execute dynamically
result = adapter.execute_tool_unified("code_helper", params)
assert result["success"], "Tool execution failed"
```

### Test 3: Orchestrator Integration
```python
# Verify Orchestrator can use tools
orchestrator = Orchestrator(api_key=KEY)
result = orchestrator.run("Write code", tool_executor=executor)
assert result["status"] == "completed"
```

### Test 4: Live Audio
```python
# Full end-to-end
# 1. Clap twice
# 2. Say: "Write Fibonacci code"
# 3. Verify: code_helper loads dynamically
#           executes automatically
#           returns result
```

---

## 📝 Documentation Updates

### File: `README.md`
Add section:

```markdown
## MCP Infrastructure

JARVIS now supports 90+ tools via dynamic MCP loading:

- **config/tools.json** - Tool definitions
- **config/mcp_providers.json** - External providers
- **actions/mcp_client.py** - Dynamic loader
- **core/mcp_orchestration_adapter.py** - Orchestrator bridge

See PHASE3_MCP_INFRASTRUCTURE.md for details.
```

### File: `MULTI_AGENT_ARCHITECTURE.txt`
Update diagram to show MCP infrastructure.

---

## 🚀 Rollout Plan

### Phase 4.0: Preparation (1-2 hours)
- [ ] Review main.py structure
- [ ] Understand current tool execution flow
- [ ] Plan changes (this document)

### Phase 4.1: Core Integration (2-3 hours)
- [ ] Add imports
- [ ] Initialize MCP at startup
- [ ] Replace tool execution loop
- [ ] Add state tracking

### Phase 4.2: Testing (1-2 hours)
- [ ] Test tool discovery
- [ ] Test dynamic execution
- [ ] Test Orchestrator integration
- [ ] Test Live Audio end-to-end

### Phase 4.3: Cleanup (30 mins)
- [ ] Remove hardcoded tool if/elif
- [ ] Update documentation
- [ ] Final verification

---

## ⚠️ Critical Points

### 1. Backward Compatibility
✅ Keep TOOL_DECLARATIONS in main.py for now
✅ Gradually migrate to dynamic loading
✅ Don't break existing functionality

### 2. Error Handling
✅ If MCP setup fails, fall back to single tool
✅ If tool not found in config, warn but continue
✅ Preserve existing error handling

### 3. Performance
✅ Lazy-load tools (don't import all at startup)
✅ Cache loaded implementations
✅ Profile if too slow

### 4. Authentication
✅ Load tokens from environment variables
✅ Don't hardcode credentials
✅ Fail gracefully if tokens missing

---

## 📊 Expected Improvements

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Lines in tool section | 500+ | ~50 | -90% |
| Time to add tool | 30 min | 5 min | -83% |
| Tools supported | 10 | 90+ | +800% |
| Maintainability | Low | High | ✅ |
| Flexibility | Fixed | Dynamic | ✅ |

---

## 🎓 Learning Path

If you're not familiar with the changes:

1. Read: **PHASE3_MCP_INFRASTRUCTURE.md** (20 mins)
2. Run: **examples/mcp_example.py** (10 mins)
3. Review: **config/tools.json** (10 mins)
4. Study: **actions/mcp_client.py** (20 mins)
5. Review: **core/mcp_orchestration_adapter.py** (20 mins)

Total: ~1.5 hours to understand everything

---

## 🔄 Alternative Approach (If Phase 4 Too Complex)

If integrating into main.py is complex, you can:

### Option A: Parallel System
```python
# Keep old tool execution (main.py)
# Plus new Orchestrator system (separate entry point)
# gradually migrate users to Orchestrator

# Use: examples/orchestrator_example.py as template
```

### Option B: Gradual Migration
```python
# Phase 4.0: Deploy MCP infrastructure (done ✅)
# Phase 4.1: Enable for new features only
# Phase 4.2: Migrate one tool at a time
# Phase 4.3: Deprecate old system
```

---

## 📞 Support

- Architecture: **MULTI_AGENT_ARCHITECTURE.txt**
- Examples: **examples/mcp_example.py** & **examples/orchestrator_example.py**
- Config: **config/tools.json** & **config/mcp_providers.json**
- Documentation: **PHASE3_MCP_INFRASTRUCTURE.md**

---

## ✅ Checklist

Before starting Phase 4:

- [ ] Phase 3 complete (✅ DONE)
- [ ] All files compile (✅ VERIFIED)
- [ ] Examples run (test locally)
- [ ] You understand MCP architecture
- [ ] You understand Orchestrator flow
- [ ] You have API key ready
- [ ] You tested Live Audio API

---

**Next Step:** Start Phase 4 with Step 1 (Import Core Components)

Ready? Let's build! 🚀
