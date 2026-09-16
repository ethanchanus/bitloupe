# Quark: a Sphinx theme for QTextBrowser

Quark is a Sphinx theme specifically designed to look and work well within the
limitations of the Qt toolkit's [QTextBrowser](https://doc.qt.io/qt-5/qtextbrowser.html).

This theme was originally designed for the bundled manual of [SpeedCrunch](http://speedcrunch.org).


## Installation

* Install the theme:
  ```bash
  $ pip install quark-sphinx-theme
  ```
* Enable it in your `conf.py`
  ```python
  html_theme = 'quark'
  # generate QTextBrowser-compatible HTML4 instead of something newer
  html4_writer = True
  ```
* Optional: enable improved design for some elements by rewriting HTML:
  ```python
  # To enable more QTextBrowser-compatible HTML generation:
  extensions = ['quark_sphinx_theme.ext.html_rewrite']
  ```

## Releasing

* set package version in `quark_sphinx_theme/__init__.py`
* update changelog
* tag release commit with `v<version>`, e.g. `v0.6.0`


## Changelog

* quark-sphinx-theme 0.6.0 *(2020-04-15)*
  - Bump minimum required Python version to 3.5.
  - Bump minimum required Sphinx version to 1.8.
  - Fix `html_rewrite` extension to properly load on other HTML-based
    builders (e.g. the `qthelp` builder).
  - Change the way the `html_rewrite` extension modifies the HTML builder to
    be less invasive.
  - Add a `sphinx.html_themes` entry point to allow loading the theme
    automatically, without setting `html_theme_path`.
  - Miscellaneous tooling changes:
    - The build system was changed from setuptools (setup.py) to flit.
    - The CI pipeline was expanded to catch more issues.
* quark-sphinx-theme 0.5.1 *(2018-04-30)*
  - Sphinx 1.7 compatibility:
    - An internal refactoring broke the integration tests. This has been fixed.
  - Every commit is now tested on every supported Python and Sphinx version
    using Gitlab CI.
  - The entire test suite is regularly re-run with the latest Sphinx version to
    more consistently discover compatibility issues.
* quark-sphinx-theme 0.5.0 *(2017-06-05)*
  - Sphinx 1.6 compatibility:
    - A change in Sphinx's HTML code broke the HTML rewriting extensions (see
      issue #1).
    - A change in the `css_files` variable in the basic theme's template broke
      the `extra_css_files` theme setting.
* quark-sphinx-theme 0.4.1 *(2016-11-22)*
  - Fix `python_requires` in setup.py.
* quark-sphinx-theme 0.4 *(2016-10-18)*
  - Add an explicit dependency on Sphinx.
  - Rename `quark_html_rewrite_features` to `quark_html_features`.
  - Add `quark_html_disabled_features` to explicitly turn off certain rewrite
    features.
  - Style changes:
    - More visually appealing code blocks on full browsers.
    - Add styling for compact lists produced by `::hlist` directive.
    - Correctly set width for topic blocks.
    - Clean up definition list margins.
* quark-sphinx-theme 0.3.2 "I'll get it right some day" *(2016-05-23)*
  - Include a copy of the lovelace style for compatibility with Pygments < 2.1.
* quark-sphinx-theme 0.3.1 *(2016-05-23)*
  - Skip CSS syntax tests if tinycss isn't available.
  - Make sure to include theme itself.
  - Include test/util.py in source packages.
* quark-sphinx-theme 0.3 *(2016-05-22)*
  - Remove `hide_sidebar_in_index` option.
  - Fix styling of index pages.
  - The `quark_sphinx_theme.ext.html_compat` extension has been renamed to
    `quark_sphinx_theme.ext.html_rewrite`. The old name remains supported for
    backwards compatibility.
  - The `html_rewrite` extension now supports wrapping admonitions in tables,
    allowing for more styling options. The theme has been updated to take
    advantage of this. Admonitions, topics, and sidebars look very different and
    much better. If `html_rewrite` is not enabled, a fallback style will be
    used for these.
  - `html_rewrite` supports wrapping literal blocks in tables. If enabled,
    this provides better styling for Pygments styles with non-white backgrounds.
  - Smaller design changes:
    - Don't use background color on code elements in headings and normal links.
    - Display terms in definition lists in bold.
    - Remove left and top margins for definition list bodies.
    - Switch default code color scheme to 'lovelace'.
* quark-sphinx-theme 0.2.1 *(2016-03-02)*
  - Change license to 2-clause BSD (in practice, it's the same thing).
* quark-sphinx-theme 0.2.0 *(2016-02-28)*
  - Add `quark_sphinx_theme.ext.html_compat` extension.
  - Add styling for citations, footnotes, table captions, and `rubric`
    directives.
* quark-sphinx-theme 0.1.2 *(2016-02-27)*
  - Fix compatibility with Jinja2 2.3.
* quark-sphinx-theme 0.1.1 *(2016-02-24)*
  - Fix spacing of navigation links.
* quark-sphinx-theme 0.1.0 *(2016-02-24)*
  - Initial release.
