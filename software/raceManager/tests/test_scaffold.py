"""Harness smoke test: proves pytest finds and imports the package.

Replace with real coverage once the first race-logic module lands.
"""

import race_manager


def test_package_imports_with_version():
    assert isinstance(race_manager.__version__, str)
    assert race_manager.__version__
