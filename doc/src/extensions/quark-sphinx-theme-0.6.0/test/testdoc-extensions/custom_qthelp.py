try:
    from sphinxcontrib.qthelp import QtHelpBuilder
except ImportError:
    from sphinx.builders.qthelp import QtHelpBuilder

class CustomQtHelpBuilder(QtHelpBuilder):
    name = 'custom-qthelp'
    search = False

def setup(app):
    app.add_builder(CustomQtHelpBuilder)
    return {'version': '0.1'}
