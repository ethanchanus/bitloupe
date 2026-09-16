import os
import unittest

import quark_sphinx_theme


class TestGetPath(unittest.TestCase):
    def test_get_path(self):
        p = quark_sphinx_theme.get_path()
        self.assertTrue(os.path.isdir(p))
        conf = os.path.join(p, 'quark', 'theme.conf')
        self.assertTrue(os.path.isfile(conf))


if __name__ == '__main__':
    unittest.main()
