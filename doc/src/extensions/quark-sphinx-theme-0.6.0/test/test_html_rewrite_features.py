import unittest

from quark_sphinx_theme.ext.html_rewrite import get_overrides
from quark_sphinx_theme.ext.html_rewrite.compat import *
from quark_sphinx_theme.ext.html_rewrite.boxes import *
from quark_sphinx_theme.ext.html_rewrite.literal_blocks import *


class TestCreateTranslatorClass(unittest.TestCase):
    def test_no_enabled_no_disabled(self):
        overrides = get_overrides([], [])
        self.assertEqual(overrides, [])

    def test_some_enabled_no_disabled(self):
        overrides = get_overrides(['compat'], [])
        self.assertEqual(overrides, [
            (citation, (visit_citation, depart_citation)),
            (footnote, (visit_footnote, depart_footnote)),
        ])

    def test_no_enabled_some_disabled(self):
        overrides = get_overrides([], ['boxes', 'literal_blocks'])
        self.assertEqual(overrides, [])

    def test_some_enabled_some_disabled(self):
        overrides = get_overrides(['compat', 'boxes'],
                                  ['literal_blocks', 'compat'])
        self.assertEqual(overrides, [
            (admonition, (visit_admonition, depart_admonition)),
            (topic, (visit_topic, depart_topic)),
            (sidebar, (visit_sidebar, depart_sidebar)),
        ])


if __name__ == '__main__':
    unittest.main()
