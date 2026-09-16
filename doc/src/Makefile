SPHINX_BUILD := sphinx-build
SPHINX_INTL := sphinx-intl
PYGETTEXT := pygettext3
QHELPGENERATOR := qhelpgenerator

default: build-bundled

update-prebuilt-manual: build-bundled
	rm -rf ../build_html_embedded
	cp -r _build-bundled ../build_html_embedded

update-translations: locale/pot
	$(SPHINX_INTL) update
	$(SPHINX_INTL) update-txconfig-resources

build-html: \
	_build-html/en_US

build-bundled: \
	_build-bundled/en_US \
	_build-bundled/manual.qrc

_build-html/%:
	$(SPHINX_BUILD) -b html -Dlanguage=$* . $@

_build-bundled/%:
	$(SPHINX_BUILD) -b qthelp -Dlanguage=$* -Dqthelp_basename=manual-$* -t sc_bundled_docs . $@
	$(QHELPGENERATOR) $@/manual-$*.qhcp

_build-bundled/manual.qrc: manual.qrc
	cp $< $@

locale/pot:
	$(SPHINX_BUILD) -b gettext . $@
	$(PYGETTEXT) -d extra-doc-strings -p $@ extensions/*.py

.PHONY: default update-prebuilt-manual update-translations build-bundled build-html
