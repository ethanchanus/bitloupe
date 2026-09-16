from sphinx.builders.html import StandaloneHTMLBuilder

from ... import __version__
from . import boxes, literal_blocks, compat


# List so it has a stable order. Descending order of precedence.
FEATURE_OVERRIDES = [
    ('boxes', boxes.OVERRIDES),
    ('literal_blocks', literal_blocks.OVERRIDES),
    ('compat', compat.OVERRIDES),
]

ALL_FEATURES = [f[0] for f in FEATURE_OVERRIDES]
DEFAULT_FEATURES = ALL_FEATURES

def get_overrides(enabled_features, disabled_features):
    features = []
    for feature, overrides in FEATURE_OVERRIDES:
        if feature in enabled_features and feature not in disabled_features:
            features.extend(overrides)
    return features

def setup_html_visitors(app):
    overrides = get_overrides(
        enabled_features=app.config.quark_html_features,
        disabled_features=app.config.quark_html_disabled_features,
    )
    for cls, htmlvisitor in overrides:
        app.add_node(cls, override=True, **{app.builder.name: htmlvisitor})

def setup(app):
    app.add_config_value('quark_html_features', DEFAULT_FEATURES, 'html')
    app.add_config_value('quark_html_disabled_features', [], 'html')
    app.connect('builder-inited', setup_html_visitors)
    return {
        'version': __version__,
        'parallel_read_safe': True,
    }
