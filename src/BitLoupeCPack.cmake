if(APPLE)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")
endif()

set(CPACK_PACKAGE_VENDOR BitLoupe)
set(CPACK_PACKAGE_VERSION ${bitloupe_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY BitLoupe)
set(CPACK_PACKAGE_EXECUTABLES ${APPLICATION_NAME} BitLoupe)

if(WIN32)
    if(NOT CPACK_GENERATOR)
        if(PORTABLE_BITLOUPE)
            set(CPACK_GENERATOR ZIP)
        else(PORTABLE_BITLOUPE)
            set(CPACK_GENERATOR NSIS)
        endif(PORTABLE_BITLOUPE)
    endif(NOT CPACK_GENERATOR)

    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../pkg/COPYING.rtf")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ${BIN_INSTALL_DIR})
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${BIN_INSTALL_DIR}\\\\${APPLICATION_NAME}.exe,0")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\resources\\\\bitloupe.ico")
    set(CPACK_NSIS_HELP_LINK "http://groups.google.com/group/bitloupe")
    set(CPACK_NSIS_URL_INFO_ABOUT "http://bitloupe.org")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL true)

    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "WriteRegStr SHCTX 'Software\\\\RegisteredApplications' 'BitLoupe' 'Software\\\\BitLoupe\\\\Capabilities'
         WriteRegStr SHCTX 'Software\\\\BitLoupe\\\\Capabilities' 'ApplicationDescription' 'BitLoupe is a high-precision scientific calculator'
         WriteRegStr SHCTX 'Software\\\\BitLoupe\\\\Capabilities' 'ApplicationName' 'BitLoupe'
         WriteRegStr SHCTX 'Software\\\\BitLoupe\\\\Capabilities\\\\UrlAssociations' 'calculator' 'BitLoupe.Url.calculator'

         WriteRegStr SHCTX 'Software\\\\Classes\\\\BitLoupe.Url.calculator' '' 'BitLoupe'
         WriteRegStr SHCTX 'Software\\\\Classes\\\\BitLoupe.Url.calculator' 'FriendlyTypeName' 'BitLoupe'
         WriteRegStr SHCTX 'Software\\\\Classes\\\\BitLoupe.Url.calculator\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\${BIN_INSTALL_DIR}\\\\${APPLICATION_NAME}.exe\\\"'
         WriteRegStr SHCTX 'Software\\\\Classes\\\\BitLoupe.Url.calculator\\\\Application' 'ApplicationCompany' 'BitLoupe'

         WriteRegStr SHCTX 'Software\\\\Classes\\\\Applications\\\\${APPLICATION_NAME}.exe' 'FriendlyAppName' 'BitLoupe'
         ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "DeleteRegValue SHCTX 'Software\\\\RegisteredApplications' 'BitLoupe'
         DeleteRegKey SHCTX 'Software\\\\BitLoupe'
         DeleteRegKey SHCTX 'Software\\\\Classes\\\\BitLoupe.Url.calculator'
         DeleteRegKey SHCTX 'Software\\\\Classes\\\\Applications\\\\${APPLICATION_NAME}.exe'
         ")

    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        # When building a 64-bit installer, we add a check to make it fail on 32-bit systems
        # because CPack doesn't do that by itself. Using this variable is probably a bit of a hack
        # though.
        set(CPACK_NSIS_DEFINES
            "!include 'x64.nsh'
             Function x64Check
             \\\${IfNot} \\\${RunningX64}
                 MessageBox MB_OK|MB_ICONEXCLAMATION 'This installer can only be run on 64-bit Windows.'
                 Quit
             \\\${EndIf}
             FunctionEnd

             Page custom x64Check
            ")
    endif()
elseif(APPLE)
     set(CPACK_GENERATOR "DragNDrop")
     set(CPACK_DMG_FORMAT "UDBZ")
     set(CPACK_DMG_VOLUME_NAME "${APPLICATION_NAME}")
     set(CPACK_PACKAGE_FILE_NAME "${APPLICATION_NAME}")
elseif(UNIX)
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

include(CPack)
