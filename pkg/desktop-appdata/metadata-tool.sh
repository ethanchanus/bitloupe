#!/bin/sh
set -eu

SRC_DIR=$(dirname $0)

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1" >&2
        exit 1
    fi
}

require_extract_tools() {
    require_cmd intltool-extract
    require_cmd xgettext
}

require_merge_tools() {
    require_cmd msgmerge
}

require_metadata_merge_tools() {
    require_cmd intltool-merge
}

run_stats() {
    stats_script="$SRC_DIR/translation-stats.py"
    if [ ! -x "$stats_script" ]; then
        echo "Missing or non-executable stats script: $stats_script" >&2
        exit 1
    fi
    "$stats_script"
}

extract_strings() {
    require_extract_tools
    intltool-extract --local --type gettext/xml $SRC_DIR/org.speedcrunch.SpeedCrunch.metainfo.xml.in
    intltool-extract --local --type gettext/ini $SRC_DIR/org.speedcrunch.SpeedCrunch.desktop.in
    xgettext tmp/*.h -o desktop-appdata.pot -cTRANSLATORS -a
}

case "$1" in
extract-strings)
    # Extract translatable strings from the appdata XML and desktop template
    # into temporary gettext headers under tmp/*.h, then generate the POT
    # template used to update/merge language PO files.
    extract_strings
    ;;
update-po)
    require_merge_tools

    # Merge all language PO files with the latest POT template.
    # If the POT template does not exist yet, generate it first.
    if [ ! -f desktop-appdata.pot ]; then
        extract_strings
    fi

    for po in "$SRC_DIR"/*.po; do
        msgmerge --update --backup=none "$po" desktop-appdata.pot
    done
    ;;
update-all)
    require_merge_tools
    require_metadata_merge_tools

    # Full translation pipeline:
    # 1) extract current source strings into desktop-appdata.pot
    # 2) merge all PO files with the updated POT
    # 3) regenerate translated metadata output files
    extract_strings

    for po in "$SRC_DIR"/*.po; do
        msgmerge --update --backup=none "$po" desktop-appdata.pot
    done

    intltool-merge --xml-style $SRC_DIR $SRC_DIR/org.speedcrunch.SpeedCrunch.metainfo.xml.in $SRC_DIR/../org.speedcrunch.SpeedCrunch.metainfo.xml
    intltool-merge --desktop-style $SRC_DIR $SRC_DIR/org.speedcrunch.SpeedCrunch.desktop.in $SRC_DIR/../org.speedcrunch.SpeedCrunch.desktop
    ;;
update-metadata-files)
    require_metadata_merge_tools

    # Merge existing PO translations into distributable metadata outputs:
    # - metainfo.xml (AppStream)
    # - .desktop launcher file
    # This consumes PO files in this directory and writes merged files to
    # the parent pkg/ directory.
    intltool-merge --xml-style $SRC_DIR $SRC_DIR/org.speedcrunch.SpeedCrunch.metainfo.xml.in $SRC_DIR/../org.speedcrunch.SpeedCrunch.metainfo.xml
    intltool-merge --desktop-style $SRC_DIR $SRC_DIR/org.speedcrunch.SpeedCrunch.desktop.in $SRC_DIR/../org.speedcrunch.SpeedCrunch.desktop
    ;;
stats)
    run_stats
    ;;
*)
    echo Commands:
    echo "    extract-strings - generate .pot file"
    echo "    update-po - merge all .po files with desktop-appdata.pot"
    echo "    update-all - extract, merge .po files, update metadata files"
    echo "    update-metadata-files - update desktop and appdata files"
    echo "    stats - show translation statistics"
    ;;
esac
