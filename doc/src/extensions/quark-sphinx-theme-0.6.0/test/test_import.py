import unittest


class TestModuleImport(unittest.TestCase):
    def assertImports(self, modname):
        try:
            __import__(modname)
        except ImportError as exc:
            self.fail('failed to import \'%s\': %s' % (modname, exc))

    def test_main_package(self):
        self.assertImports('quark_sphinx_theme')

    def test_ext(self):
        self.assertImports('quark_sphinx_theme.ext')

    def test_ext_html_rewrite(self):
        self.assertImports('quark_sphinx_theme.ext.html_rewrite')

    def test_ext_html_compat(self):
        self.assertImports('quark_sphinx_theme.ext.html_compat')


if __name__ == '__main__':
    unittest.main()
