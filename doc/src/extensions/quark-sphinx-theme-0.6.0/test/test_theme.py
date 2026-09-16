import os
import sys
import unittest

DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, DIR)
from util import SphinxBuildFixture, with_document  # noqa


testdoc_theme = os.path.join(DIR, 'testdoc-theme')


class TestThemeEntrypoint(SphinxBuildFixture, unittest.TestCase):
    source_dir = os.path.join(DIR, 'testdoc-theme-entrypoint')

    @with_document('contents')
    def test_contents_nav_elements(self, doc):
        self.assertHasElement(doc, './/table[@class="nav-sidebar"]')
        self.assertHasElement(doc, './/table[@class="navbar navbar-top"]')
        self.assertHasElement(doc, './/table[@class="navbar navbar-bottom"]')


class TestThemeDefaults(SphinxBuildFixture, unittest.TestCase):
    source_dir = testdoc_theme

    def test_quark_css(self):
        self.assertSphinxCSSValid('quark.css')

    def _nav_elems(self, doc):
        self.assertHasElement(doc, './/table[@class="nav-sidebar"]')
        self.assertHasElement(doc, './/table[@class="navbar navbar-top"]')
        self.assertHasElement(doc, './/table[@class="navbar navbar-bottom"]')

    @with_document('contents')
    def test_contents_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('genindex')
    def test_genindex_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('genindex-A')
    def test_genindex_a_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('genindex-all')
    def test_genindex_all_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('py-modindex')
    def test_domainindex_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('search')
    def test_search_nav_elements(self, doc):
        self._nav_elems(doc)


class TestThemeExtraCss(SphinxBuildFixture, unittest.TestCase):
    source_dir = testdoc_theme
    config = {
        'html_theme_options.extra_css_files': '_static/extra1.css, '
                                              '_static/extra2.css'
    }

    def test_quark_css(self):
        self.assertSphinxCSSValid('quark.css')

    @with_document('contents')
    def test_contents_css_files(self, doc):
        extra_css = {
            s.strip() for s in
            self.config['html_theme_options.extra_css_files'].split(',')
        }
        for css in doc.findall('head/link[@rel="stylesheet"]'):
            extra_css.discard(css.get('href').strip())
        self.assertSetEqual(extra_css, set())


class TestThemeNoSidebar(SphinxBuildFixture, unittest.TestCase):
    source_dir = testdoc_theme
    config = {
        'html_theme_options.nosidebar': True,
    }

    def test_quark_css(self):
        self.assertSphinxCSSValid('quark.css')

    def _nav_elems(self, doc):
        self.assertNotElement(doc, './/table[@class="nav-sidebar"]')
        self.assertHasElement(doc, './/table[@class="navbar navbar-top"]')
        self.assertHasElement(doc, './/table[@class="navbar navbar-bottom"]')

    @with_document('contents')
    def test_contents_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('genindex')
    def test_genindex_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('genindex-A')
    def test_genindex_a_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('genindex-all')
    def test_genindex_all_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('py-modindex')
    def test_domainindex_nav_elements(self, doc):
        self._nav_elems(doc)

    @with_document('search')
    def test_search_nav_elements(self, doc):
        self._nav_elems(doc)


class TestThemeAllSettingsUnset(SphinxBuildFixture, unittest.TestCase):
    source_dir = testdoc_theme
    _SETTINGS = [
        'text_color',
        'body_font',
        'code_font',
        'body_font_size',
        'code_font_size',
        'li_extra_indent',

        'full_browser_extras',
        'extra_css_files',

        'sidebar_bgcolor',
        'sidebar_border',
        'navbar_color',
        'navbar_bgcolor',

        'link_color',
        'xref_color',
        'headerlink_color',
        'underline_links',
        'underline_navbar_links',

        'code_bgcolor',
        'pre_bgcolor',

        'admonition_border',
        'admonition_fgcolor',
        'admonition_bgcolor',
        'warning_fgcolor',
        'warning_bgcolor',
    ]
    config = {'html_theme_options.%s' % s: '' for s in _SETTINGS}

    def test_quark_css(self):
        self.assertSphinxCSSValid('quark.css')


if __name__ == '__main__':
    unittest.main()
