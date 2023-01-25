cmake_minimum_required(VERSION 3.15)
project(TmoTA)

find_package(CGV REQUIRED)

file(GLOB_RECURSE SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cxx")
file(GLOB_RECURSE HEADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.h")
file(GLOB_RECURSE SHADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.gl*")

shader_test("${CGV_DIR}"
        ST_FILES
        ST_INCLUDES
        ST_INSTALL_DIR
        ${SHADERS})

set(ALL_SOURCES ${SOURCES} ${ST_FILES} ${SHADERS})

add_library(TmoTA SHARED ${ALL_SOURCES})
target_link_libraries(TmoTA cgv_utils cgv_type cgv_signal cgv_reflect
cgv_data cgv_os cgv_base cgv_math cgv_media cgv_gui cgv_render cgv_gl
cgv_reflect_types)
add_dependencies(TmoTA cgv_viewer cg_fltk cmi_io cg_ext)
