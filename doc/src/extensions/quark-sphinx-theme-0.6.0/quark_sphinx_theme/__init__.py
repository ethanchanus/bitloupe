"""A Sphinx theme designed for QTextBrowser"""

import os

__version_info__ = (0, 6, 0)
__version__ = '.'.join(map(str, __version_info__))

def get_path():
    """Get theme path."""
    return os.path.dirname(os.path.abspath(__file__))

def setup(app):
    app.add_html_theme('quark', os.path.join(get_path(), 'quark'))

__all__ = ["__version__", "__version_info__", "get_path", "setup"]
