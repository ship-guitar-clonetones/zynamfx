# cmake/linux-toolchain.cmake

# Ensure pkg-config is available
find_package(PkgConfig REQUIRED)

# GTK3: JUCE's native file dialogs on Linux.
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
