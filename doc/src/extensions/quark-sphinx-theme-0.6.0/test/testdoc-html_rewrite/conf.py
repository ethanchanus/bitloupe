import os
import sys

master_doc = 'contents'
html4_writer = True

if tags.has('test_html_compat_alias'):
    extensions = ['quark_sphinx_theme.ext.html_compat']
else:
    extensions = ['quark_sphinx_theme.ext.html_rewrite']

sys.path.append(os.path.abspath(os.path.join(os.pardir, 'testdoc-extensions')))
extensions.append('custom_qthelp')
