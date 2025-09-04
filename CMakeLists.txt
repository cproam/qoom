cmake_minimum_required(VERSION 3.20)

project(qoom VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW (windowing/input)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

## GLAD2 (OpenGL loader)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.6
)
FetchContent_Populate(glad)

# Bring GLAD cmake functions and options into scope
add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR}/cmake)

# Generate a GL 3.3 core loader target
glad_add_library(glad_gl_core_33 STATIC
    LANGUAGE c
    LOADER
    API gl:core=3.3
)

## GLM (math)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

## tinygltf (header-only)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.5
)
FetchContent_MakeAvailable(tinygltf)

add_executable(qoom
    src/main.cpp
    src/shader.cpp
    src/assimp_model.cpp
)

## Assimp (model loading)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.1
)
FetchContent_MakeAvailable(assimp)

target_link_libraries(qoom PRIVATE glfw glad_gl_core_33 glm::glm assimp::assimp)

target_include_directories(qoom PRIVATE
    ${tinygltf_SOURCE_DIR}
)

if (WIN32)
    target_link_libraries(qoom PRIVATE opengl32)
endif()

if (APPLE)
    target_link_libraries(qoom PRIVATE "-framework OpenGL")
endif()

if (UNIX AND NOT APPLE)
    # Some platforms may require these
    find_package(Threads REQUIRED)
    target_link_libraries(qoom PRIVATE dl Threads::Threads)
endif()

if (MSVC)
    target_compile_options(qoom PRIVATE /W4 /permissive-)
else()
    target_compile_options(qoom PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(qoom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Copy shaders to build dir for runtime
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_DIR} $<TARGET_FILE_DIR:qoom>/shaders
    DEPENDS ${SHADER_DIR}
)
add_dependencies(qoom copy_shaders)

# Copy assets for runtime
set(ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR} $<TARGET_FILE_DIR:qoom>/assets
    DEPENDS ${ASSETS_DIR}
)
add_dependencies(qoom copy_assets)
