import pytest
import os

# Run this file when you are one directory level above the testing,website, etc directories
if __name__ == "__main__":
    # pytest.main([f'testing/test_nonpersis.py'])
    pytest.main([f'testing/test_persis.py',
                f'testing/test_nonpersis.py'])
