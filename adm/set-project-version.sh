VERSION=${1:-1.0}
sed -i '/set(bitloupe_VERSION/c\set(bitloupe_VERSION "'"$VERSION"'")' ../src/CMakeLists.txt
sed -i '/DEFINES += BITLOUPE_VERSION/c\DEFINES += BITLOUPE_VERSION=\\\\\\"'"$VERSION"'\\\\\\"' ../src/bitloupe.pro
sed -i '/RELEASE_VERSION=/c\RELEASE_VERSION='"$VERSION" ../pkg/cross-linux/build-bitloupe.sh
sed -i '/version =/c\version = '"'$VERSION'" ../doc/src/conf.py
sed -i '/release =/c\release = '"'$VERSION'" ../doc/src/conf.py
