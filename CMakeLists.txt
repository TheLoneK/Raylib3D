cmake_minimum_required(VERSION 3.16)
project(Raylib3D LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Put the game executable in the project root so it runs from the same place as before
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

#-----------------------------------------------------------------------------------
# flecs (C library, compiled once into a static library - it is big, ~30s to build)
#-----------------------------------------------------------------------------------
add_library(flecs STATIC third_party/flecs/flecs.c)
target_include_directories(flecs PUBLIC third_party/flecs)
set_target_properties(flecs PROPERTIES C_STANDARD 99)
if(WIN32)
    target_link_libraries(flecs PUBLIC ws2_32 dbghelp)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(flecs PUBLIC Threads::Threads)
endif()

#-----------------------------------------------------------------------------------
# raylib: use the prebuilt library in lib/ (headers in include/) if it is there,
# otherwise download and build raylib 6.2 automatically
#-----------------------------------------------------------------------------------
find_library(RAYLIB_LOCAL_LIBRARY NAMES raylib PATHS ${CMAKE_SOURCE_DIR}/lib NO_DEFAULT_PATH)

if(RAYLIB_LOCAL_LIBRARY)
    message(STATUS "Using local raylib: ${RAYLIB_LOCAL_LIBRARY}")
    add_library(raylib STATIC IMPORTED)
    set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION ${RAYLIB_LOCAL_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include)
    if(WIN32)
        set_property(TARGET raylib PROPERTY INTERFACE_LINK_LIBRARIES opengl32 gdi32 winmm)
    elseif(APPLE)
        set_property(TARGET raylib PROPERTY INTERFACE_LINK_LIBRARIES "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    else()
        set_property(TARGET raylib PROPERTY INTERFACE_LINK_LIBRARIES GL m pthread dl rt X11)
    endif()
else()
    message(STATUS "No raylib in lib/, downloading raylib 6.2")
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)

    # raylib leaves some common image formats out by default; the asset library imports them
    # (also needed for JPG textures embedded in .glb files)
    target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG=1 SUPPORT_FILEFORMAT_TGA=1
                                              SUPPORT_FILEFORMAT_PSD=1 SUPPORT_FILEFORMAT_HDR=1)
endif()

#-----------------------------------------------------------------------------------
# Game: every .cpp under src/ is compiled, so new files/folders need no changes here
#-----------------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(TestGame ${GAME_SOURCES})
target_include_directories(TestGame PRIVATE src include)
target_link_libraries(TestGame PRIVATE raylib flecs)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TestGame PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

#-----------------------------------------------------------------------------------
# Asset editor (Editor/): imports models/textures into Assets/ and picks which ones the
# enemies and weapons use. Shares the asset library code (src/assets) with the game.
#-----------------------------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Editor/*.cpp)
file(GLOB ASSET_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/assets/*.cpp)

add_executable(AssetEditor ${EDITOR_SOURCES} ${ASSET_SOURCES}
    src/enemies/EnemyDefs.cpp       # Slot names and sizes come from the game's tables
    src/weapons/Weapon.cpp)
target_include_directories(AssetEditor PRIVATE Editor src include)
target_link_libraries(AssetEditor PRIVATE raylib)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(AssetEditor PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

# MinGW: bundle the C++ runtime so the executables run without extra DLLs next to them
if(MINGW)
    target_link_options(TestGame PRIVATE -static-libgcc -static-libstdc++ -static)
    target_link_options(AssetEditor PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
