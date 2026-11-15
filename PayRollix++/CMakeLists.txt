cmake_minimum_required(VERSION 4.1)
project(Payrollix VERSION 1.0 LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Payrollix++ is Windows 11-only. Use Visual Studio 2026 on Windows. Portable checks: cmake -S tests -B build-tests")
endif()
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Use the x64 MSVC tools in Visual Studio 2026.")
endif()
if(CMAKE_GENERATOR MATCHES "^Visual Studio" AND NOT CMAKE_VS_PLATFORM_TOOLSET STREQUAL "v145")
    message(FATAL_ERROR "Use Visual Studio 2026 with -T v145.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/FindPayrollQt.cmake)
payrollix_find_qt(_payrollix_qt)
if(NOT "${PAYROLLIX_SELECTED_QT}" STREQUAL "${_payrollix_qt}")
    # Prevent cached package directories from mixing two Qt installations.
    get_cmake_property(_cache_variables CACHE_VARIABLES)
    foreach(_variable IN LISTS _cache_variables)
        if(_variable MATCHES "^Qt6.*_DIR$")
            unset(${_variable} CACHE)
        endif()
    endforeach()
endif()
set(PAYROLLIX_SELECTED_QT "${_payrollix_qt}" CACHE INTERNAL "Selected Qt kit" FORCE)
set(Qt6_DIR "${_payrollix_qt}/lib/cmake/Qt6" CACHE PATH "Qt package folder" FORCE)
list(PREPEND CMAKE_PREFIX_PATH "${_payrollix_qt}")
message(STATUS "Using Qt kit: ${_payrollix_qt}")
find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()
qt_add_executable(PayrollApp WIN32
    src/main.cpp src/AppWindow.cpp src/EmployeeViews.cpp src/AdminViews.cpp
    src/ReportViews.cpp src/Store.cpp src/Security.cpp src/Platform.cpp
    src/PayrollCalculator.cpp app.manifest
)
target_include_directories(PayrollApp PRIVATE include)
target_compile_definitions(PayrollApp PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_compile_options(PayrollApp PRIVATE /W4 /permissive- /utf-8 /bigobj)
target_link_libraries(PayrollApp PRIVATE Qt6::Widgets winsqlite3 bcrypt comdlg32
    gdi32 shell32 ole32 user32)
set_target_properties(PayrollApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)
install(TARGETS PayrollApp RUNTIME DESTINATION .)
qt_generate_deploy_app_script(TARGET PayrollApp OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
