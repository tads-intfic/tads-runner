cmake_minimum_required(VERSION 3.17)
project(HtmlTads64 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.")
include(TadsFunctions)

# The vendored libpng under ../htmltads enables the ASM language unconditionally
# in its project() call.  On Windows the only candidate assembler is MSVC cl, and
# CMake >= 4.1 emits a CMP0194 developer warning about that.  libpng leaves the
# policy unset, so hook its project() and set the policy in its own scope - see
# cmake/libpng-policy-fixup.cmake for the details.
set(CMAKE_PROJECT_libpng_INCLUDE_BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libpng-policy-fixup.cmake")

# Group targets into solution folders in IDEs like Visual Studio.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type: Debug" FORCE)
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../htmltads)
	message(STATUS "htmltads directory found")
	set(WITH_HTMLTADS ON)
	set(HTMLTADS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../htmltads)
else()
	message(STATUS "no htmltads directory in ${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()

if (EMSCRIPTEN)
	add_subdirectory(curl)
	set(CURL_FOUND TRUE CACHE BOOL "CURL_FOUND" FORCE)
endif()
add_subdirectory(tads2)
add_subdirectory(tads3)
add_subdirectory(tests)

if (WITH_HTMLTADS)
	add_subdirectory(../htmltads htmltads)
endif()

install(EXPORT TadsTargets
	FILE TadsTargets.cmake
	NAMESPACE Tads::
	DESTINATION cmake/Tads
	COMPONENT Classic
)

include (CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/TadsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TadsConfig.cmake
  INSTALL_DESTINATION "cmake/Tads"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/TadsConfigVersion.cmake"
  VERSION "${Tads_VERSION_MAJOR}.${Tads_VERSION_MINOR}.${Tads_VERSION_PATCH}"
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/TadsConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/TadsConfigVersion.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/TadsFunctions.cmake
  DESTINATION cmake/Tads
  COMPONENT Classic
)

export(EXPORT TadsTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/TadsTargets.cmake"
  NAMESPACE Tads::
)

set(CPACK_GENERATOR ZIP)

# Package the classic runner/compiler/workbench tools and the guit3
# interpreter as two separate zips instead of one combined archive.
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL Classic)

# CPACK_SYSTEM_NAME is normally only defaulted inside the CPack module
# itself (during include(CPack) below), so compute it the same way here
# to be able to reference it in CPACK_ARCHIVE_GUIT3_FILE_NAME already.
if (NOT CPACK_SYSTEM_NAME)
	if (CMAKE_SYSTEM_NAME MATCHES "Windows")
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(CPACK_SYSTEM_NAME win64)
		else()
			set(CPACK_SYSTEM_NAME win32)
		endif()
	else()
		set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
	endif()
endif()

if (WITH_HTMLTADS)
	list(APPEND CPACK_COMPONENTS_ALL GuiT3)
	# GuiT3's zip is named after itself rather than the HtmlTads64 project name.
	set(CPACK_ARCHIVE_GUIT3_FILE_NAME "GuiT3-${PROJECT_VERSION}-${CPACK_SYSTEM_NAME}")
endif()

include(CPack)
