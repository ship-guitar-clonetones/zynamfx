# cmake/linux-toolchain.cmake

# Ensure pkg-config is available
find_package(PkgConfig REQUIRED)

# GTK3: JUCE's native file dialogs on Linux.
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

# WebKit2GTK: only the legacy webview UI (-DT3K_NATIVE_UI=OFF) compiles
# against it; the native UI builds with JUCE_WEB_BROWSER=0 and must configure
# on a machine without the -dev package. The option is declared before
# project(), so it is visible here; CMake's try_compile probes re-include
# this file without the cache, where it stays undefined and is skipped.
if(DEFINED T3K_NATIVE_UI AND NOT T3K_NATIVE_UI)
    pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.1)
    include_directories(${WEBKIT2GTK_INCLUDE_DIRS})
    link_directories(${WEBKIT2GTK_LIBRARY_DIRS})
endif()
