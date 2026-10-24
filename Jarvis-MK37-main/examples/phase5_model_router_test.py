"""
Phase 5 Model Router - End-to-End Test Suite
Validates complete Model Router integration with language experts, linter, RAG

Tests:
  1. Model Router initialization
  2. Complexity calculation
  3. Provider health checks
  4. Language expert selection
  5. Linter validation
  6. RAG context retrieval
  7. Full end-to-end flow
"""

import asyncio
import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from core.model_router import get_model_router, TaskComplexity
from core.language_experts import get_language_expert_provider
from core.linter_agent import get_linter_agent, IssueSeverity
from core.rag_local import get_rag_local


class TestPhase5ModelRouter:
    """Test suite for Phase 5 Model Router."""
    
    def __init__(self):
        self.tests_passed = 0
        self.tests_failed = 0
        self.results = []
    
    def log(self, message: str, status: str = "INFO"):
        """Log test message."""
        emoji = {
            "PASS": "✅",
            "FAIL": "❌",
            "INFO": "ℹ️",
            "WARN": "⚠️"
        }.get(status, "•")
        
        print(f"{emoji} {message}")
        self.results.append({"message": message, "status": status})
    
    async def test_1_model_router_init(self):
        """Test 1: Model Router initialization."""
        print("\n" + "="*70)
        print("TEST 1: Model Router Initialization")
        print("="*70)
        
        try:
            router = get_model_router()
            
            assert router is not None, "Router should not be None"
            assert router.config is not None, "Config should be loaded"
            assert "providers" in router.config, "Config should have providers"
            assert "routing_rules" in router.config, "Config should have routing rules"
            
            providers = router.config.get("providers", {})
            self.log(f"Loaded {len(providers)} providers: {', '.join(providers.keys())}")
            
            self.tests_passed += 1
            self.log("PASSED: Model Router initialized successfully", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def test_2_complexity_calculation(self):
        """Test 2: Task complexity calculation."""
        print("\n" + "="*70)
        print("TEST 2: Complexity Calculation")
        print("="*70)
        
        try:
            router = get_model_router()
            
            # Test simple task
            simple_complexity = await router.calculate_complexity(
                task_description="Write a hello world script",
                language="python"
            )
            assert 0 <= simple_complexity <= 1, "Complexity should be 0-1"
            self.log(f"Simple task complexity: {simple_complexity:.2f}", "INFO")
            
            # Test complex task
            complex_complexity = await router.calculate_complexity(
                task_description="Design a microservices architecture with distributed caching, load balancing, and auto-scaling",
                language="java",
                code_length=5000
            )
            assert 0 <= complex_complexity <= 1, "Complexity should be 0-1"
            self.log(f"Complex task complexity: {complex_complexity:.2f}", "INFO")
            
            # Complex should be > simple
            assert complex_complexity > simple_complexity, "Complex should be > simple"
            self.log("Complexity calculation working correctly", "PASS")
            
            self.tests_passed += 1
            self.log("PASSED: Complexity calculation correct", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def test_3_provider_health(self):
        """Test 3: Provider health checks."""
        print("\n" + "="*70)
        print("TEST 3: Provider Health Checks")
        print("="*70)
        
        try:
            router = get_model_router()
            
            # Check provider health
            health = await router._check_provider_health()
            
            assert isinstance(health, dict), "Health should be dict"
            assert len(health) > 0, "Should check at least one provider"
            
            for provider, is_healthy in health.items():
                status = "✓" if is_healthy else "✗"
                self.log(f"Provider {provider}: {status}", "INFO")
            
            self.tests_passed += 1
            self.log("PASSED: Provider health checks completed", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def test_4_language_experts(self):
        """Test 4: Language expert selection."""
        print("\n" + "="*70)
        print("TEST 4: Language Experts")
        print("="*70)
        
        try:
            expert_provider = get_language_expert_provider()
            
            languages = expert_provider.get_supported_languages()
            assert len(languages) > 0, "Should support at least one language"
            self.log(f"Supported languages: {', '.join(languages)}", "INFO")
            
            # Test expertise for Python
            python_expertise = expert_provider.get_expertise("python")
            assert python_expertise is not None, "Python expertise should exist"
            assert len(python_expertise.best_practices) > 0, "Should have best practices"
            assert len(python_expertise.performance_tips) > 0, "Should have perf tips"
            self.log(f"Python expertise: {len(python_expertise.architectural_patterns)} patterns", "INFO")
            
            # Test system prompt generation
            system_prompt = expert_provider.build_system_prompt("java")
            assert len(system_prompt) > 100, "System prompt should be substantial"
            self.log(f"Generated Java system prompt: {len(system_prompt)} chars", "INFO")
            
            self.tests_passed += 1
            self.log("PASSED: Language experts working correctly", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def test_5_linter_agent(self):
        """Test 5: Linter validation."""
        print("\n" + "="*70)
        print("TEST 5: Linter Agent")
        print("="*70)
        
        try:
            linter = get_linter_agent()
            
            # Test Python linting
            python_code = """
def hello():
    print("hello")

except:
    pass
"""
            
            result = await linter.lint_code(python_code, "python")
            assert result is not None, "Lint result should exist"
            assert hasattr(result, "total_issues"), "Should have issue count"
            self.log(f"Python linting: {result.total_issues} issues found", "INFO")
            
            # Test JavaScript linting
            js_code = """
var x = 5;
if (x == 5) {
    console.log("x is 5");
}
"""
            
            result = await linter.lint_code(js_code, "javascript")
            assert result.total_issues > 0, "Should find issues in JS code"
            self.log(f"JavaScript linting: {result.total_issues} issues found", "INFO")
            
            # Test autocorrection
            corrected = await linter.autocorrect(js_code, "javascript", result.issues)
            if corrected:
                self.log("Autocorrection available for JS code", "INFO")
            
            self.tests_passed += 1
            self.log("PASSED: Linter agent working correctly", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def test_6_rag_local(self):
        """Test 6: RAG local context retrieval."""
        print("\n" + "="*70)
        print("TEST 6: RAG Local Context")
        print("="*70)
        
        try:
            rag = get_rag_local(Path.cwd())
            
            # Build project tree
            tree = await rag.build_project_tree(max_files=20)
            assert tree is not None, "Tree should be built"
            assert tree.file_count > 0, "Should find at least one file"
            self.log(f"Project tree: {tree.file_count} files, {tree.total_size_bytes} bytes", "INFO")
            
            # Get critical files
            critical = await rag.get_critical_files()
            self.log(f"Critical files found: {len(critical)}", "INFO")
            
            # Build context
            context = await rag.build_context(
                target_file=None,
                language="python",
                max_context_length=2000
            )
            assert len(context) > 0, "Context should be generated"
            self.log(f"Generated context: {len(context)} chars", "INFO")
            
            # Get stats
            stats = rag.get_statistics()
            self.log(f"RAG stats: {stats}", "INFO")
            
            self.tests_passed += 1
            self.log("PASSED: RAG local working correctly", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def test_7_full_flow(self):
        """Test 7: Full end-to-end flow."""
        print("\n" + "="*70)
        print("TEST 7: Full End-to-End Flow")
        print("="*70)
        
        try:
            # Initialize all components
            router = get_model_router()
            experts = get_language_expert_provider()
            linter = get_linter_agent()
            rag = get_rag_local(Path.cwd())
            
            # Step 1: Routing decision
            decision = await router.route(
                task_type="quick_implementation",
                task_description="Write a Python function to calculate factorial",
                language="python",
                code_length=100,
                sensitive=False
            )
            assert decision is not None, "Routing decision should exist"
            self.log(f"Routing decision: {decision.primary_model}", "INFO")
            
            # Step 2: Get language expertise
            expertise = experts.get_expertise("python")
            self.log(f"Language expertise loaded for Python", "INFO")
            
            # Step 3: Simulated code
            code = """
def factorial(n):
    if n < 0:
        return None
    if n == 0:
        return 1
    return n * factorial(n - 1)

print(factorial(5))
"""
            
            # Step 4: Lint the code
            lint_result = await linter.lint_code(code, "python")
            self.log(f"Linting complete: {lint_result.total_issues} issues", "INFO")
            
            # Step 5: Get context
            context = await rag.build_context(language="python")
            self.log(f"Context built: {len(context)} chars", "INFO")
            
            self.tests_passed += 1
            self.log("PASSED: Full end-to-end flow working correctly", "PASS")
            return True
        
        except Exception as e:
            self.tests_failed += 1
            self.log(f"FAILED: {e}", "FAIL")
            return False
    
    async def run_all_tests(self):
        """Run all tests."""
        print("\n" + "╔" + "="*68 + "╗")
        print("║ PHASE 5 MODEL ROUTER - END-TO-END TEST SUITE".ljust(69) + "║")
        print("╚" + "="*68 + "╝")
        
        await self.test_1_model_router_init()
        await self.test_2_complexity_calculation()
        await self.test_3_provider_health()
        await self.test_4_language_experts()
        await self.test_5_linter_agent()
        await self.test_6_rag_local()
        await self.test_7_full_flow()
        
        # Summary
        print("\n" + "="*70)
        print("TEST SUMMARY")
        print("="*70)
        total = self.tests_passed + self.tests_failed
        pass_rate = (self.tests_passed / total * 100) if total > 0 else 0
        
        print(f"✅ Passed: {self.tests_passed}")
        print(f"❌ Failed: {self.tests_failed}")
        print(f"📊 Pass Rate: {pass_rate:.1f}%")
        
        if self.tests_failed == 0:
            print("\n🎉 ALL TESTS PASSED!")
            return True
        else:
            print(f"\n⚠️ {self.tests_failed} tests failed")
            return False


async def main():
    """Run test suite."""
    tester = TestPhase5ModelRouter()
    success = await tester.run_all_tests()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    asyncio.run(main())
