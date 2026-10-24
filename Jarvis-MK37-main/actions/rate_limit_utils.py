"""
Rate limit handling with exponential backoff.
Provides utilities for smart retry logic when hitting API limits.
"""

import time
import functools
from typing import Callable, Any, TypeVar, cast

F = TypeVar('F', bound=Callable[..., Any])


class RateLimitError(Exception):
    """Raised when API rate limit is encountered."""
    pass


def is_rate_limit_error(error: Exception) -> bool:
    """
    Detect if an error is a rate limit error from Google API.
    
    Args:
        error: Exception to check
    
    Returns:
        True if error is rate limit related
    """
    error_str = str(error).lower()
    return any(keyword in error_str for keyword in [
        "429", "quota", "resource_exhausted", "rate", "limit",
        "too many requests", "service_unavailable"
    ])


def exponential_backoff_retry(
    max_attempts: int = 3,
    initial_delay: float = 2.0,
    max_delay: float = 300.0,
    backoff_factor: float = 2.0,
) -> Callable[[F], F]:
    """
    Decorator for exponential backoff retry logic.
    
    On rate limit errors, waits with exponential backoff before retrying.
    
    Args:
        max_attempts: Maximum number of retry attempts
        initial_delay: Initial delay in seconds (default: 2s)
        max_delay: Maximum delay cap (default: 300s)
        backoff_factor: Multiplication factor for each retry (default: 2x)
    
    Returns:
        Decorated function with retry logic
    
    Example:
        @exponential_backoff_retry(max_attempts=3, initial_delay=30)
        def call_api():
            return gemini_model.generate_content(prompt)
    """
    def decorator(func: F) -> F:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            delay = initial_delay
            last_error = None
            
            for attempt in range(1, max_attempts + 1):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    last_error = e
                    
                    if not is_rate_limit_error(e):
                        # Non-rate-limit errors are not retried
                        raise
                    
                    if attempt < max_attempts:
                        # Calculate backoff delay
                        actual_delay = min(delay, max_delay)
                        print(f"[RateLimit] ⏳ Rate limit hit (attempt {attempt}/{max_attempts}), "
                              f"waiting {actual_delay}s before retry...")
                        time.sleep(actual_delay)
                        delay = min(delay * backoff_factor, max_delay)
                    else:
                        print(f"[RateLimit] ❌ Max retries exceeded ({max_attempts})")
            
            # If we get here, all retries failed
            raise last_error or RateLimitError(f"Failed after {max_attempts} attempts")
        
        return cast(F, wrapper)
    return decorator


def smart_sleep_between_calls(delay_seconds: float = 2.0) -> None:
    """
    Smart sleep between API calls to avoid hammering the API.
    
    Args:
        delay_seconds: Delay in seconds (default: 2s)
    """
    time.sleep(delay_seconds)
