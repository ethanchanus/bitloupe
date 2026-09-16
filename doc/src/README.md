# HTML Manual
The SpeedCrunch manual and website is made using [Sphinx](http://sphinx-doc.org). It can be built either as a manual bundled
with the application, using a stripped-down theme for use with QtHelp and QTextBrowser, or as the full website with a more
complete theme.

## Important Tasks
* `build-bundled` - build all language versions of the bundled manual in the `_build-bundled` directory. This build can
  be used when building SpeedCrunch by setting the `HTML_DOCS_DIR` CMake variable to the build directory.
* `build-html` - build all language versions of the website in the `_build-html` directory.
* `update-prebuilt-manual` - update the prebuilt manual in the `build_html_embedded` directory. This is intended to be
  committed to Git and is used for default source builds.
* `update-translations` - extract translatable strings from all source files and update the .po files in the `locale` directory.

## Dependencies
Building the docs requires additional dependencies:
- [Sphinx](http://sphinx-doc.org) 3.0 or later
- [the Qt help builder extension](https://github.com/sphinx-doc/sphinxcontrib-qthelp) 1.0 or later

Building the bundled docs requires:
- the Quark theme (already bundled as an extension for convenience)
- [qhelpgenerator](https://doc.qt.io/qt-6/qthelp-framework.html) matching the Qt version used to build SpeedCrunch

Building the website requires:
- [the sphinx-bootstrap theme](https://pypi.org/project/sphinx-bootstrap-theme/)

Updating the translation templates requires:
- [the pygettext script shipped with Python](https://docs.python.org/3/library/gettext.html)
- [sphinx-intl](https://pypi.org/project/sphinx-intl/) with the `transifex` feature

## Adding new languages
* Add the new language to the `LANGUAGES` map at the top of `conf.py`
* Add the language to the `build-html` and `build-bundled` targets in the Makefile
* Copy the lines in `manual.qrc` for the new language
* Create a directory for the new language in `locale`
* Run `make update-translations` to create the translation files for the new language
