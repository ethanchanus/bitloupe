cmake_minimum_required(VERSION 3.16)
project(bitloupe)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message("Using C++${CMAKE_CXX_STANDARD}")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
message("Using C${CMAKE_C_STANDARD}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")

find_package(Qt6 COMPONENTS Core Widgets Help Network Sql Test QUIET)
if(Qt6_FOUND)
    message(STATUS "Found Qt version: ${Qt6_VERSION}")
endif()

if (MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wno-shift-negative-value)
endif()
add_compile_options(-Wall -pedantic)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(bitloupe_VERSION "1.0")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MACOSX_RPATH ON)

ADD_DEFINITIONS("-DBITLOUPE_VERSION=\"${bitloupe_VERSION}\"")
ADD_DEFINITIONS(-DQT_USE_QSTRINGBUILDER)

include(SourceFiles.cmake)

# Keep the externally editable configuration and bundled register data beside build
# artifacts as whole folders, so any file later added under conf/ or data/ is deployed
# automatically without further CMakeLists.txt changes.
file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/settings.conf")
file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/socregs.conf")
file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/sc.json")
file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/SupportedSoC.json")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/conf" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
# Legacy GNU bc number implementation uses K&R-style definitions.
# Keep global warnings strict while silencing C23 prototype deprecations for this file.
    set_source_files_properties(math/number.c PROPERTIES
        COMPILE_OPTIONS "-Wno-deprecated-non-prototype;-Wno-strict-prototypes")
    set_source_files_properties(tests/testfloatnum.c PROPERTIES
        COMPILE_OPTIONS "-Wno-strict-prototypes")
endif()

if(APPLE)
    set(APP_ICON_FILE bitloupe.icns)
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/${APP_ICON_FILE}")
    set(APPLICATION_NAME BitLoupe)
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/Info.plist")
    set(MACOSX_BUNDLE_ICON_FILE ${APP_ICON_FILE})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${bitloupe_VERSION})
    set(MACOSX_BUNDLE_VERSION ${bitloupe_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING Version ${bitloupe_VERSION})
    set_source_files_properties(${APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
elseif(HAIKU)
    set(APPLICATION_NAME BitLoupe)
else()
    set(APPLICATION_NAME bitloupe)
endif()

if(WIN32)
    set(WIN32_RES_FILE ${CMAKE_CURRENT_BINARY_DIR}/bitloupe.rc.obj)
    if(MINGW)
        add_custom_command(OUTPUT ${WIN32_RES_FILE}
                           COMMAND windres.exe ${CMAKE_CURRENT_SOURCE_DIR}/resources/bitloupe.rc ${WIN32_RES_FILE})
    else()
        add_custom_command(OUTPUT ${WIN32_RES_FILE}
                           COMMAND rc.exe /fo ${WIN32_RES_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/resources/bitloupe.rc)
    endif()
endif()

# Embedded manual
set(HTML_DOCS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../doc/build_html_embedded"
    CACHE PATH
    "Path to the HTML manual. Defaults to the bundled prebuilt copy.")
set(bitloupe_RESOURCES ${bitloupe_RESOURCES} ${HTML_DOCS_DIR}/manual.qrc)

if(WIN32)
    add_executable(${APPLICATION_NAME} WIN32 ${bitloupe_SOURCES} ${bitloupe_RESOURCES} ${WIN32_RES_FILE})
elseif(APPLE)
    add_executable(${APPLICATION_NAME} MACOSX_BUNDLE ${bitloupe_SOURCES} ${bitloupe_RESOURCES} ${APP_ICON})
    set_target_properties(${APPLICATION_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../pkg/Info.plist)
else()
    add_executable(${APPLICATION_NAME} ${bitloupe_SOURCES} ${bitloupe_RESOURCES})
endif()

add_custom_command(TARGET ${APPLICATION_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -f
            "$<TARGET_FILE_DIR:${APPLICATION_NAME}>/settings.conf"
            "$<TARGET_FILE_DIR:${APPLICATION_NAME}>/socregs.conf"
            "$<TARGET_FILE_DIR:${APPLICATION_NAME}>/sc.json"
            "$<TARGET_FILE_DIR:${APPLICATION_NAME}>/SupportedSoC.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/conf"
                "$<TARGET_FILE_DIR:${APPLICATION_NAME}>/conf"
            COMMENT "Copying conf/ configuration folder")

add_custom_command(TARGET ${APPLICATION_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_FILE_DIR:${APPLICATION_NAME}>/data"
        COMMENT "Copying data/ register catalog folder")

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}" REQUIRED)
    add_custom_command(TARGET ${APPLICATION_NAME} POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
        --no-translations
        --dir "$<TARGET_FILE_DIR:${APPLICATION_NAME}>"
        "$<TARGET_FILE:${APPLICATION_NAME}>"
    COMMENT "Deploying Qt runtime dependencies")
endif()

set_property(TARGET ${APPLICATION_NAME} PROPERTY CXX_STANDARD 11)

# Temporary SoC Regs crash-investigation logging (file-based qWarning/qDebug
# sink) and crash-dump/call-stack capture. Off by default -- re-enable with
# -DBITLOUPE_SOCREGS_DIAGNOSTICS=ON if the issue needs to be chased again.
option(BITLOUPE_SOCREGS_DIAGNOSTICS "Enable SoC Regs debug log + crash dump/call stack capture" OFF)
if(BITLOUPE_SOCREGS_DIAGNOSTICS)
    target_compile_definitions(${APPLICATION_NAME} PRIVATE BITLOUPE_SOCREGS_DIAGNOSTICS)
    if(MSVC)
        # A stack trace would otherwise show raw addresses only: /Zi keeps
        # matching debug info in the object files; /DEBUG tells the linker to
        # still emit a full bitloupe.pdb next to the .exe in Release too.
        target_compile_options(${APPLICATION_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
        target_link_options(${APPLICATION_NAME} PRIVATE
            $<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
        target_link_libraries(${APPLICATION_NAME} Dbghelp)
    endif()
endif()

target_compile_definitions(${APPLICATION_NAME}
    PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_INFO_OUTPUT>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_WARNING_OUTPUT>
)

add_custom_target(confclean COMMAND rm -rf Makefile CMakeFiles/ CMakeCache.txt cmake_install.cmake DartTestfile.txt install_manifest.txt)

set(QT_LIBRARIES Qt6::Widgets Qt6::Help Qt6::Network Qt6::Sql Qt6::Core)
target_link_libraries(${APPLICATION_NAME} ${QT_LIBRARIES})

enable_testing()

add_executable(testhmath ${testhmath_SOURCES})
target_link_libraries(testhmath ${QT_LIBRARIES})
add_test(testhmath testhmath)

add_executable(testevaluator ${testevaluator_SOURCES} ${testevaluator_HEADERS_MOC})
target_link_libraries(testevaluator ${QT_LIBRARIES})
add_test(testevaluator testevaluator)

add_executable(testfloatnum ${testfloatnum_SOURCES})
set_property(TARGET testfloatnum APPEND PROPERTY COMPILE_DEFINITIONS _FLOATNUMTEST)
add_test(testfloatnum testfloatnum)

add_executable(testcmath ${testcmath_SOURCES})
target_link_libraries(testcmath ${QT_LIBRARIES})
add_test(testcmath testcmath)

add_executable(testdmath ${testdmath_SOURCES})
target_link_libraries(testdmath ${QT_LIBRARIES})
add_test(testdmath testdmath)

add_executable(testserial ${testserial_SOURCES})
target_link_libraries(testserial ${QT_LIBRARIES})
add_test(testserial testserial)

add_executable(testeditorui ${testeditorui_SOURCES})
target_link_libraries(testeditorui ${QT_LIBRARIES} Qt6::Test)
add_test(NAME testeditorui COMMAND testeditorui)

add_executable(testoklch ${testoklch_SOURCES})
target_link_libraries(testoklch ${QT_LIBRARIES} Qt6::Test)
add_test(NAME testoklch COMMAND testoklch)

add_executable(testdocksui ${testdocksui_SOURCES})
target_link_libraries(testdocksui ${QT_LIBRARIES} Qt6::Test)
add_test(NAME testdocksui COMMAND testdocksui)

add_executable(testdisplayui ${testdisplayui_SOURCES})
target_link_libraries(testdisplayui ${QT_LIBRARIES} Qt6::Test)
add_test(NAME testdisplayui COMMAND testdisplayui)

set_property(TARGET ${APPLICATION_NAME} APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:EVALUATOR_DEBUG>)

include_directories(${CMAKE_BINARY_DIR} thirdparty core gui math)

################################# INSTALL ######################################

if(UNIX AND NOT APPLE AND NOT HAIKU)
    set(SHARE_DIR "share")
    set(BIN_INSTALL_DIR "bin")

    set(MENU_DIR "${SHARE_DIR}/applications/")
    set(APP_DATA_DIR "${SHARE_DIR}/metainfo/")
    set(ICON_DIR "${SHARE_DIR}/pixmaps/")
elseif(APPLE)
    set(BIN_INSTALL_DIR ".")
    set(DOC_INSTALL_DIR ${APPLICATION_NAME}.app/Contents/Resources)
elseif(HAIKU)
    set(BIN_INSTALL_DIR "apps")
    set(DOC_INSTALL_DIR "documentation/BitLoupe")
else()
    set(BIN_INSTALL_DIR ".")
    set(DOC_INSTALL_DIR ".")
endif()

option(PORTABLE_BITLOUPE "Enable to build in portable mode" OFF)
if(PORTABLE_BITLOUPE)
    add_definitions(-DBITLOUPE_PORTABLE)
endif(PORTABLE_BITLOUPE)

install(TARGETS ${APPLICATION_NAME} DESTINATION ${BIN_INSTALL_DIR})
install(DIRECTORY conf/ DESTINATION ${BIN_INSTALL_DIR}/conf)
install(DIRECTORY data/ DESTINATION ${BIN_INSTALL_DIR}/data)

if(UNIX AND NOT APPLE AND NOT HAIKU)
    install(FILES ../pkg/org.bitloupe.BitLoupe.desktop DESTINATION ${MENU_DIR})
    install(FILES resources/bitloupe.png DESTINATION ${ICON_DIR} RENAME org.bitloupe.BitLoupe.png)
    install(FILES ../pkg/org.bitloupe.BitLoupe.metainfo.xml DESTINATION ${APP_DATA_DIR})
elseif(WIN32)
    install(FILES ../pkg/COPYING.rtf DESTINATION ${DOC_INSTALL_DIR})
endif()

if(WIN32)
    set(QTWIN32_DEPLOY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QtWin32Deploy.cmake")
    if(EXISTS "${QTWIN32_DEPLOY_SCRIPT}")
        include("${QTWIN32_DEPLOY_SCRIPT}")
        if(COMMAND qtwin32_deploy_modules)
            qtwin32_deploy_modules(${BIN_INSTALL_DIR} qtwin32_Qt6Widgets qtwin32_Qt6Help qtwin32_Qt6Network)
        endif()
        if(COMMAND qtwin32_deploy_default_qt_conf)
            qtwin32_deploy_default_qt_conf(${BIN_INSTALL_DIR})
        endif()
    else()
        message(WARNING "QtWin32Deploy.cmake not found at ${QTWIN32_DEPLOY_SCRIPT}; skipping Windows deployment helper.")
    endif()
elseif(APPLE)
    include(MacdeployQt)
    macdeployqt(${APPLICATION_NAME}.app ${APPLICATION_NAME})
endif(WIN32)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${BIN_INSTALL_DIR})
include(InstallRequiredSystemLibraries)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

include(BitLoupeCPack.cmake)

################################ UNINSTALL #####################################

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
                "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
                IMMEDIATE @ONLY)

add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
