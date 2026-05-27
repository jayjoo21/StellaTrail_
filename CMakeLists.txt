cmake_minimum_required(VERSION 3.16)
project(StellaTrail VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Find SDL2 ----
# Supports: vcpkg, MSYS2, manual deps/ folder, system install
find_package(SDL2 QUIET CONFIG)
find_package(SDL2_image QUIET CONFIG)
find_package(SDL2_ttf QUIET CONFIG)

if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()
if(NOT SDL2_image_FOUND)
    find_package(SDL2_image QUIET)
endif()
if(NOT SDL2_ttf_FOUND)
    find_package(SDL2_ttf QUIET)
endif()

# Manual fallback: deps/ subdirectory
if(NOT TARGET SDL2::SDL2 AND NOT SDL2_FOUND)
    set(SDL2_INC "${CMAKE_SOURCE_DIR}/deps/SDL2/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2/lib/x64/SDL2.lib")
        set(SDL2MAIN_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2/lib/x64/SDL2main.lib")
    else()
        set(SDL2_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2/lib/x86/SDL2.lib")
        set(SDL2MAIN_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2/lib/x86/SDL2main.lib")
    endif()
    add_library(SDL2::SDL2 IMPORTED SHARED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_IMPLIB "${SDL2_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INC}")
    add_library(SDL2::SDL2main IMPORTED STATIC)
    set_target_properties(SDL2::SDL2main PROPERTIES
        IMPORTED_LOCATION "${SDL2MAIN_LIB}")
endif()

if(NOT TARGET SDL2_image::SDL2_image AND NOT SDL2_image_FOUND)
    set(SDL2IMG_INC "${CMAKE_SOURCE_DIR}/deps/SDL2_image/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2IMG_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2_image/lib/x64/SDL2_image.lib")
    else()
        set(SDL2IMG_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2_image/lib/x86/SDL2_image.lib")
    endif()
    add_library(SDL2_image::SDL2_image IMPORTED SHARED)
    set_target_properties(SDL2_image::SDL2_image PROPERTIES
        IMPORTED_IMPLIB "${SDL2IMG_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2IMG_INC}")
endif()

if(NOT TARGET SDL2_ttf::SDL2_ttf AND NOT SDL2_ttf_FOUND)
    set(SDL2TTF_INC "${CMAKE_SOURCE_DIR}/deps/SDL2_ttf/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2TTF_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2_ttf/lib/x64/SDL2_ttf.lib")
    else()
        set(SDL2TTF_LIB "${CMAKE_SOURCE_DIR}/deps/SDL2_ttf/lib/x86/SDL2_ttf.lib")
    endif()
    add_library(SDL2_ttf::SDL2_ttf IMPORTED SHARED)
    set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
        IMPORTED_IMPLIB "${SDL2TTF_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2TTF_INC}")
endif()

# ---- Sources ----
set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/Player.cpp
    src/Physics.cpp
    src/TileMap.cpp
    src/PuzzleSystem.cpp
    src/SolarMap.cpp
    src/UI.cpp
)

add_executable(StellaTrail ${SOURCES})
target_include_directories(StellaTrail PRIVATE src)

# Link SDL2 — handle both old (variables) and new (targets) find_package styles
if(TARGET SDL2::SDL2)
    target_link_libraries(StellaTrail PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(StellaTrail PRIVATE SDL2::SDL2main)
    endif()
else()
    target_include_directories(StellaTrail PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(StellaTrail PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_image::SDL2_image)
    target_link_libraries(StellaTrail PRIVATE SDL2_image::SDL2_image)
elseif(SDL2_image_FOUND)
    target_include_directories(StellaTrail PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_link_libraries(StellaTrail PRIVATE ${SDL2_IMAGE_LIBRARIES})
endif()

if(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(StellaTrail PRIVATE SDL2_ttf::SDL2_ttf)
elseif(SDL2_ttf_FOUND)
    target_include_directories(StellaTrail PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(StellaTrail PRIVATE ${SDL2_TTF_LIBRARIES})
endif()

# Copy assets to build directory
add_custom_command(TARGET StellaTrail POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:StellaTrail>/assets"
    COMMENT "Copying assets..."
)

# On Windows with MSVC/manual deps, copy DLLs
if(WIN32)
    foreach(LIB SDL2 SDL2_image SDL2_ttf)
        set(DLL_SEARCH "${CMAKE_SOURCE_DIR}/deps/${LIB}/lib/x64/${LIB}.dll")
        if(EXISTS "${DLL_SEARCH}")
            add_custom_command(TARGET StellaTrail POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL_SEARCH}" "$<TARGET_FILE_DIR:StellaTrail>")
        endif()
    endforeach()
endif()
