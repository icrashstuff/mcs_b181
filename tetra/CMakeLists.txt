cmake_minimum_required(VERSION 3.27)

# Make option() honor normal variables
cmake_policy(SET CMP0077 NEW)

project(tetra CXX C)

option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU specific compiler optimizations" OFF)
option(DISABLE_STACK_PROTECTOR "Useful for finding out where stack smashing is occurring (Debug builds only)" OFF )

# We don't necessarily have to disable most of the archivers but we also don't need them either
set(PHYSFS_BUILD_SHARED FALSE)
set(PHYSFS_ARCHIVE_GRP FALSE)
set(PHYSFS_ARCHIVE_WAD FALSE)
set(PHYSFS_ARCHIVE_CSM FALSE)
set(PHYSFS_ARCHIVE_HOG FALSE)
set(PHYSFS_ARCHIVE_MVL FALSE)
set(PHYSFS_ARCHIVE_QPAK FALSE)
set(PHYSFS_ARCHIVE_SLB FALSE)
set(PHYSFS_ARCHIVE_ISO9660 FALSE)
set(PHYSFS_ARCHIVE_VDF FALSE)
set(PHYSFS_ARCHIVE_LECARCHIVES FALSE)
set(PHYSFS_BUILD_DOCS FALSE)
set(PHYSFS_BUILD_TEST FALSE)
add_subdirectory(util/physfs/physfs-74c3054/)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TETRA_DIR tetra)

set(imgui_SRC
    ${TETRA_DIR}/gui/imgui-1.91.1/imgui.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/imgui_demo.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/imgui_draw.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/imgui_tables.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/imgui_widgets.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/misc/cpp/imgui_stdlib.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/backends/imgui_impl_sdl3.cpp
    ${TETRA_DIR}/gui/imgui-1.91.1/backends/imgui_impl_opengl3.cpp
)

set(tetra_SRC
    ${TETRA_DIR}/tetra.cpp

    ${TETRA_DIR}/gui/styles.cpp
    ${TETRA_DIR}/gui/console.cpp
    ${TETRA_DIR}/gui/gui_registrar.cpp
    ${TETRA_DIR}/gui/imgui_extracts.cpp
    ${TETRA_DIR}/gui/physfs_browser.cpp
    ${TETRA_DIR}/gui/overlay_loading.cpp
    ${TETRA_DIR}/gui/overlay_performance.cpp

    ${TETRA_DIR}/util/stbi.c
    ${TETRA_DIR}/util/misc.cpp
    ${TETRA_DIR}/util/convar.cpp
    ${TETRA_DIR}/util/cli_parser.cpp
    ${TETRA_DIR}/util/convar_file.cpp

    ${imgui_SRC}
    PARENT_SCOPE
)

function (add_tetra_executable target_name target_SRC)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(SDL3 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)

    add_executable(${target_name} "${target_SRC}" "${tetra_SRC}")

    target_compile_options(${target_name} PUBLIC -Wall -Wextra)

    # -WShadow spits out a warning for code like:
    # int i = 0;
    # for(int i = 0; ...)
    # ...
    #
    # If you ever need to find a flag to warn for specific behavior:
    # the clang flag -Weverything is useful for finding them
    target_compile_options(${target_name} PUBLIC -Wshadow)

    if(ENABLE_NATIVE_OPTIMIZATION AND NOT MSVC)
        target_compile_options(${target_name} PRIVATE -march=native -mtune=native)
    endif()

    if(DISABLE_STACK_PROTECTOR AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT MSVC)
        message(WARNING "Disabling stack protector")
        target_compile_options(${target_name} PRIVATE -fno-stack-protector)
    endif()

    target_link_libraries(${target_name} OpenGL::GL)
    target_link_libraries(${target_name} OpenGL::GLU)
    target_link_libraries(${target_name} GLEW::GLEW)
    target_link_libraries(${target_name} PhysFS::PhysFS-static)
    target_link_libraries(${target_name} SDL3::SDL3)
endfunction()
