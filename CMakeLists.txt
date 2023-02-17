cmake_minimum_required(VERSION 3.15)
project(TmoTA)

option (CGV_BUILD_EXAMPLES off)
option (CGV_STDCPP17 off)

include(FetchContent)
FetchContent_Declare(
        cgv
        GIT_REPOSITORY https://github.com/sgumhold/cgv.git
        GIT_TAG origin/develop
)
FetchContent_MakeAvailable(cgv)

file(GLOB_RECURSE SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cxx")
file(GLOB_RECURSE HEADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.h")
file(GLOB_RECURSE SHADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.gl*")

cgv_add_target(TmoTA
	TYPE			plugin
	SOURCES         ${SOURCES}
	HEADERS         ${HEADERS}
	SHADER_SOURCES  ${SHADERS}
	DEPENDENCIES    cgv_viewer cgv_utils cgv_type cgv_signal cgv_reflect cgv_data cgv_os cgv_base
					cgv_math cgv_media cgv_gui cgv_render cgv_gl cgv_reflect_types tt_gl_font
					cg_ext cg_fltk cmi_io cmf_tt_gl_font
	ADDITIONAL_CMDLINE_ARGS
					"config:\"${CMAKE_CURRENT_LIST_DIR}/TmoTA.def\""
)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TmoTA)
