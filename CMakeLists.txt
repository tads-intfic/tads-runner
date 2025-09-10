cmake_minimum_required(VERSION 3.17)
project(Tads VERSION 3.1.3)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type: Debug" FORCE)
endif()

if (EXISTS ../htmltads)
	set(WITH_HTMLTADS ON)
	set(HTMLTADS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../htmltads)
endif()

add_subdirectory(tads3)

if (WITH_HTMLTADS)
	add_subdirectory(../htmltads htmltads)
endif()

install(EXPORT TadsTargets
	FILE TadsTargets.cmake
	DESTINATION cmake/Tads
)

include (CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/TadsConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/TadsConfig.cmake"
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
  DESTINATION cmake/Tads
)

export(EXPORT TadsTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/TadsTargets.cmake"
)
