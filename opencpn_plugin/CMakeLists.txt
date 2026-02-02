##---------------------------------------------------------------------------
## Saildrop Chart Stream Plugin for OpenCPN
## Copyright (C) 2024-2025 Davide Gessa
##---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.10)

project(saildrop_pi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(OCPN_FLATPAK "Build for Flatpak" OFF)

# Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base net)
include(${wxWidgets_USE_FILE})

# Find OpenGL (optional, for overlay support)
find_package(OpenGL)

# Find OpenCPN
# The plugin API headers can be found in the OpenCPN source or installed headers
# Try to find via pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(OPENCPN opencpn QUIET)
endif()

# If not found via pkg-config, look for the headers directly
if(NOT OPENCPN_FOUND)
    # Check common locations for OpenCPN headers
    find_path(OPENCPN_INCLUDE_DIR
        NAMES ocpn_plugin.h
        PATHS
            /usr/include/opencpn
            /usr/local/include/opencpn
            /opt/local/include/opencpn
            ${CMAKE_SOURCE_DIR}/../opencpn/include
            ${CMAKE_SOURCE_DIR}/../../opencpn/include
            $ENV{HOME}/opencpn/include
        PATH_SUFFIXES opencpn
    )

    if(OPENCPN_INCLUDE_DIR)
        set(OPENCPN_FOUND TRUE)
        message(STATUS "Found OpenCPN headers: ${OPENCPN_INCLUDE_DIR}")
    else()
        message(WARNING "OpenCPN headers not found. Using bundled headers if available.")
        # For development, you can copy ocpn_plugin.h to include/
        set(OPENCPN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
    endif()
endif()

# Source files
set(SOURCES
    src/saildrop_pi.cpp
    src/chartstream_client.cpp
    src/chart_renderer.cpp
    src/settings_dialog.cpp
)

set(HEADERS
    include/saildrop_pi.h
    include/chartstream_client.h
    include/chart_renderer.h
    include/settings_dialog.h
    include/protocol.h
)

# Create shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OPENCPN_INCLUDE_DIR}
    ${wxWidgets_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${wxWidgets_LIBRARIES}
)

if(OPENGL_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()

# Set output name without lib prefix on all platforms
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${PROJECT_NAME}"
)

# Platform-specific settings
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".dylib"
        INSTALL_RPATH "@loader_path/../Frameworks"
    )
elseif(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".dll"
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".so"
    )
endif()

# Installation
if(APPLE)
    set(PLUGIN_INSTALL_DIR "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/PlugIns")
elseif(WIN32)
    set(PLUGIN_INSTALL_DIR "$ENV{APPDATA}/OpenCPN/plugins")
else()
    set(PLUGIN_INSTALL_DIR "$ENV{HOME}/.local/lib/opencpn")
endif()

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    RUNTIME DESTINATION ${PLUGIN_INSTALL_DIR}
)

# Install data files (icon)
install(DIRECTORY data/
    DESTINATION "${PLUGIN_INSTALL_DIR}/../share/opencpn/plugins/saildrop_pi"
    FILES_MATCHING PATTERN "*.png"
)

# Print configuration summary
message(STATUS "")
message(STATUS "Saildrop Chart Stream Plugin Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  wxWidgets: ${wxWidgets_VERSION_STRING}")
message(STATUS "  OpenCPN headers: ${OPENCPN_INCLUDE_DIR}")
message(STATUS "  Install dir: ${PLUGIN_INSTALL_DIR}")
message(STATUS "")
