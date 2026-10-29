import asyncio
import inspect
import sys
import os

# Asegurar que la raíz de Jarvis-MK37 esté en sys.path
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

def pytest_pyfunc_call(pyfuncitem):
    """Ejecuta tests asíncronos nativamente con asyncio.run() sin requerir plugin externo."""
    testfunction = pyfuncitem.obj
    if inspect.iscoroutinefunction(testfunction):
        args = {arg: pyfuncitem.funcargs[arg] for arg in pyfuncitem._fixtureinfo.argnames}
        asyncio.run(testfunction(**args))
        return True
