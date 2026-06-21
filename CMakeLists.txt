cmake_minimum_required(VERSION 3.26)
project(klondike-solitaire)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android-specific raylib root (override if needed)
set(RAYLIB_ANDROID_ROOT "$ENV{RAYLIB_ANDROID_ROOT}" CACHE PATH "Root of prebuilt raylib for Android (include/ lib/)")

# Configure static linking (keep if you build other libs from source)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Add nlohmann/json as a header-only library
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(json)

# Create directory for json.hpp
file(MAKE_DIRECTORY ${json_SOURCE_DIR}/nlohmann)
file(COPY ${json_SOURCE_DIR}/json.hpp DESTINATION ${json_SOURCE_DIR}/nlohmann)

# Add executable (use WIN32 subsystem for Release on Windows like before)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_executable(${PROJECT_NAME} WIN32
        src/main.cpp
        src/Card.cpp
        src/Solitaire.cpp
    )
else()
    add_executable(${PROJECT_NAME}
        src/main.cpp
        src/Card.cpp
        src/Solitaire.cpp
    )
endif()

#
# Platform / toolchain specific raylib handling
#
if(MSVC)
    # MSVC / Windows: expose cache variables so users can override them via CMake GUI or -D
    set(RAYLIB_DEBUG_INCLUDE   "C:/raylib/debug/include")
    set(RAYLIB_DEBUG_LIB       "C:/raylib/debug/lib/raylib.lib")
    set(RAYLIB_RELEASE_INCLUDE "C:/raylib/release/include")
    set(RAYLIB_RELEASE_LIB     "C:/raylib/release/lib/raylib.lib")

    target_include_directories(${PROJECT_NAME} PRIVATE 
        $<$<CONFIG:Debug>:${RAYLIB_DEBUG_INCLUDE}>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${RAYLIB_RELEASE_INCLUDE}>
        ${json_SOURCE_DIR}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE 
        $<$<CONFIG:Debug>:${RAYLIB_DEBUG_LIB}>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${RAYLIB_RELEASE_LIB}>
        winmm
        gdi32
        opengl32
    )

elseif(ANDROID)
    # Android toolchain: expect a prebuilt raylib under RAYLIB_ANDROID_ROOT (set RAYLIB_ANDROID_ROOT or env var)
    if(NOT RAYLIB_ANDROID_ROOT)
        message(FATAL_ERROR "RAYLIB_ANDROID_ROOT not set. Set RAYLIB_ANDROID_ROOT to a directory containing include/ and lib/ for Android.")
    endif()

    find_path(RAYLIB_ANDROID_INCLUDE raylib.h PATHS "${RAYLIB_ANDROID_ROOT}/include" NO_DEFAULT_PATH)
    find_library(RAYLIB_ANDROID_LIB NAMES raylib PATHS "${RAYLIB_ANDROID_ROOT}/lib" NO_DEFAULT_PATH)

    if(NOT RAYLIB_ANDROID_INCLUDE OR NOT RAYLIB_ANDROID_LIB)
        message(FATAL_ERROR "Could not find raylib include/lib under ${RAYLIB_ANDROID_ROOT}")
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_ANDROID_INCLUDE} ${json_SOURCE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_ANDROID_LIB})

else()
    # Other platforms (Linux, macOS, custom toolchains): try find_package, then fall back to common system paths
    find_package(raylib QUIET)

    if(raylib_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${raylib_INCLUDE_DIRS} ${json_SOURCE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${raylib_LIBRARIES})
    else()
        find_path(RAYLIB_SYSTEM_INCLUDE raylib.h PATHS /usr/local/include /usr/include)
        find_library(RAYLIB_SYSTEM_LIB NAMES raylib PATHS /usr/local/lib /usr/lib)

        if(RAYLIB_SYSTEM_INCLUDE AND RAYLIB_SYSTEM_LIB)
            target_include_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_SYSTEM_INCLUDE} ${json_SOURCE_DIR})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_SYSTEM_LIB} m pthread dl)
        else()
            message(WARNING "raylib not found via find_package or system locations. Please install raylib or set RAYLIB paths for your platform.")
            target_include_directories(${PROJECT_NAME} PRIVATE ${json_SOURCE_DIR})
        endif()
    endif()
endif()

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Copy assets folder to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
) 

# Create zip file of bin directory contents
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND PowerShell -Command "Compress-Archive -Path '${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe','${CMAKE_BINARY_DIR}/assets' -DestinationPath '${CMAKE_BINARY_DIR}/${PROJECT_NAME}.zip' -Force"
        COMMENT "Creating ${PROJECT_NAME}.zip with correct folder structure"
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.zip" 
            --format=zip 
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe"
            "${CMAKE_BINARY_DIR}/assets"
        COMMENT "Creating ${PROJECT_NAME} with correct folder structure"
    )
endif()