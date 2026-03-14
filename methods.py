import sys


def print_error(message):
    """Print error message to stderr."""
    print(f"\033[91mERROR: {message}\033[0m", file=sys.stderr)


def print_warning(message):
    """Print warning message to stderr."""
    print(f"\033[93mWARNING: {message}\033[0m", file=sys.stderr)


def print_info(message):
    """Print info message to stdout."""
    print(f"\033[94mINFO: {message}\033[0m", file=sys.stdout)