# Shared, metadata-only discovery for the PowerShell launcher and Visual Studio.
# Does not run executables from candidate installations.
function(payrollix_check_qt root version_out reason_out)
    set(_reason "")
    foreach(_file lib/cmake/Qt6/Qt6Config.cmake
                  lib/cmake/Qt6Widgets/Qt6WidgetsConfig.cmake
                  mkspecs/qconfig.pri bin/windeployqt.exe
                  bin/Qt6Core.dll bin/Qt6Gui.dll bin/Qt6Widgets.dll
                  lib/Qt6Core.lib lib/Qt6Gui.lib lib/Qt6Widgets.lib
                  plugins/platforms/qwindows.dll)
        if(NOT EXISTS "${root}/${_file}")
            set(_reason "missing ${_file} (incomplete kit or not a shared MSVC desktop kit)")
            break()
        endif()
    endforeach()
    set(_version "")
    if(NOT _reason)
        file(READ "${root}/mkspecs/qconfig.pri" _metadata)
        if(NOT _metadata MATCHES "(^|\n)QT_ARCH[ \t]*=[ \t]*x86_64[ \t]*(\r?\n|$)")
            set(_reason "the kit is not x64 (QT_ARCH must be x86_64)")
        endif()
        if(_metadata MATCHES "(^|\n)QT_VERSION[ \t]*=[ \t]*(6\\.[0-9]+\\.[0-9]+)[ \t]*(\r?\n|$)")
            set(_version "${CMAKE_MATCH_2}")
            if(_version VERSION_LESS "6.9.0")
                set(_reason "Qt ${_version} is older than the required Qt 6.9")
            endif()
        else()
            set(_reason "cannot identify a stable Qt 6 version in mkspecs/qconfig.pri")
        endif()
    endif()
    set(${version_out} "${_version}" PARENT_SCOPE)
    set(${reason_out} "${_reason}" PARENT_SCOPE)
endfunction()

function(payrollix_expand_qt hint output)
    file(TO_CMAKE_PATH "${hint}" _root)
    string(REGEX REPLACE "/+$" "" _root "${_root}")
    if(_root MATCHES "/lib/cmake/Qt6$")
        get_filename_component(_root "${_root}/../../.." ABSOLUTE)
    elseif(_root MATCHES "/bin$")
        get_filename_component(_root "${_root}/.." ABSOLUTE)
    endif()
    # Bounded search: kit itself, version/kit, or install-root/version/kit.
    file(GLOB _configs LIST_DIRECTORIES FALSE
        "${_root}/*/lib/cmake/Qt6/Qt6Config.cmake"
        "${_root}/*/*/lib/cmake/Qt6/Qt6Config.cmake")
    set(_roots "${_root}")
    foreach(_config IN LISTS _configs)
        get_filename_component(_kit "${_config}/../../../.." ABSOLUTE)
        list(APPEND _roots "${_kit}")
    endforeach()
    set(${output} "${_roots}" PARENT_SCOPE)
endfunction()

function(payrollix_choose_qt output)
    set(_valid "")
    foreach(_hint IN LISTS ARGN)
        if(_hint STREQUAL "")
            continue()
        endif()
        payrollix_expand_qt("${_hint}" _roots)
        foreach(_root IN LISTS _roots)
            payrollix_check_qt("${_root}" _version _reason)
            if(NOT _reason)
                list(APPEND _valid "${_version}|${_root}")
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _valid)
    list(SORT _valid COMPARE NATURAL ORDER DESCENDING)
    if(_valid)
        list(GET _valid 0 _best)
        string(REGEX REPLACE "^[^|]+\\|" "" _best "${_best}")
    else()
        set(_best "")
    endif()
    set(${output} "${_best}" PARENT_SCOPE)
endfunction()

function(payrollix_find_qt output)
    if(PAYROLLIX_QT_ROOT)
        # An explicit override must work; never silently select another kit.
        get_filename_component(_explicit "${PAYROLLIX_QT_ROOT}" ABSOLUTE)
        payrollix_check_qt("${_explicit}" _version _reason)
        if(_reason)
            message(FATAL_ERROR "The selected Qt folder '${_explicit}' is not usable: ${_reason}.\nSelect a Qt 6.9+ MSVC x64 desktop kit, not MinGW or ARM64.")
        endif()
        set(${output} "${_explicit}" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED PAYROLLIX_QT_SEARCH_ROOTS)
        # Optional bounded roots for custom installations and isolated tests.
        payrollix_choose_qt(_selected ${PAYROLLIX_QT_SEARCH_ROOTS})
    else()
        file(TO_CMAKE_PATH "$ENV{CMAKE_PREFIX_PATH}" _environment_prefix)
        payrollix_choose_qt(_selected "$ENV{QTDIR}" "$ENV{QT_ROOT_DIR}"
            "$ENV{Qt6_DIR}" "${Qt6_DIR}" ${CMAKE_PREFIX_PATH} ${_environment_prefix})
        if(NOT _selected)
            # Qt tools on PATH identify their kit without executing the tools.
            file(TO_CMAKE_PATH "$ENV{PATH}" _path)
            set(_path_hints "")
            foreach(_directory IN LISTS _path)
                foreach(_tool qmake.exe qmake6.exe qtpaths.exe qtpaths6.exe windeployqt.exe)
                    if(EXISTS "${_directory}/${_tool}")
                        list(APPEND _path_hints "${_directory}")
                        break()
                    endif()
                endforeach()
            endforeach()
            payrollix_choose_qt(_selected ${_path_hints})
        endif()
        if(NOT _selected)
            set(_common "$ENV{USERPROFILE}/Qt" "$ENV{LOCALAPPDATA}/Qt"
                "$ENV{ProgramFiles}/Qt" "$ENV{ProgramFiles}/Qt6"
                "$ENV{ProgramW6432}/Qt" "$ENV{SystemDrive}/Qt")
            foreach(_drive C D E F G H I J K L M N O P Q R S T U V W X Y Z)
                if(IS_DIRECTORY "${_drive}:/Qt")
                    list(APPEND _common "${_drive}:/Qt")
                endif()
            endforeach()
            payrollix_choose_qt(_selected ${_common})
        endif()
    endif()
    if(NOT _selected)
        message(FATAL_ERROR
            "Qt was not found, or no compatible installation was found.\n"
            "Install Qt 6.9 or newer with the MSVC x64 desktop kit (for example msvc2022_64), "
            "including Widgets and deployment tools. MinGW, ARM64 and incomplete kits are not supported.\n"
            "Searched Qt environment settings, Qt tools on PATH, and common Qt installation folders "
            "(including <drive>:/Qt and your user Qt folder).\n"
            "For a custom location, run build.bat -QtRoot \"C:/your/Qt/6.9.3/msvc2022_64\" "
            "or configure CMake with -DPAYROLLIX_QT_ROOT=\"C:/your/Qt/6.9.3/msvc2022_64\".\n"
            "No software has been installed automatically.")
    endif()
    set(${output} "${_selected}" PARENT_SCOPE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    payrollix_find_qt(_qt)
    payrollix_check_qt("${_qt}" _version _reason)
    message(STATUS "Using Qt ${_version}: ${_qt}")
    if(PAYROLLIX_QT_RESULT)
        get_filename_component(_directory "${PAYROLLIX_QT_RESULT}" DIRECTORY)
        file(MAKE_DIRECTORY "${_directory}")
        file(WRITE "${PAYROLLIX_QT_RESULT}" "${_qt}")
    endif()
endif()
