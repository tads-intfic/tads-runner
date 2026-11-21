
# Mirrors the on-disk directory layout in the IDE (e.g. Visual Studio's
# Solution Explorer) instead of CMake's default flat Source/Header filters.
# root defaults to CMAKE_SOURCE_DIR; pass it explicitly for targets whose
# sources live outside the main project tree (e.g. the sibling htmltads repo).
function(tads_source_group target)
	set(root ${ARGV1})
	if (NOT root)
		set(root ${CMAKE_SOURCE_DIR})
	endif()
	get_target_property(target_sources ${target} SOURCES)
	source_group(TREE ${root} FILES ${target_sources})
endfunction()

function(make_t3r output)
	# CMAKE_CROSSCOMPILING_EMULATOR (empty when not cross-compiling) runs
	# t3res through node when t3res.js is itself an Emscripten build - same
	# pattern build_game() below uses for t3make. $<TARGET_FILE:...> is used
	# instead of the plain target-name COMMAND form so the emulator prefix
	# has a real path to run.
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r
		COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:Tads::t3res> -create ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r -add ${ARGN}
		DEPENDS Tads::t3res
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	add_custom_target(build_${output}.t3r DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r)
endfunction()

function(trx_target_file name output)
	set(${output} ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${name}${CMAKE_EXECUTABLE_SUFFIX} PARENT_SCOPE)
endfunction()

function(make_trx output input)
	trx_target_file(${output} trx_target)
	add_custom_command(OUTPUT ${trx_target}
		COMMAND Tads::maketrx32 -type EXRS $<TARGET_FILE:${input}> ${ARGN} ${trx_target}
		DEPENDS Tads::maketrx32 ${input} ${ARGN}
	)
	add_custom_target(${output} ALL DEPENDS ${trx_target})
endfunction()

# em_package(<output> <source@virtual/path>... [COMPONENT <component>])
# COMPONENT names the install component the generated .data/.js pair belongs
# to (e.g. so CPack puts it into that component's archive); without it they
# land in CMake's default "Unspecified" component.
function(em_package output)
	cmake_parse_arguments(EMP "" "COMPONENT" "" ${ARGN})
	set(em_package_files ${EMP_UNPARSED_ARGUMENTS})

	# Make the package depend on its inputs ("source@virtual/path" arguments;
	# directories are globbed recursively) so it is repackaged when they
	# change, not just when the .data file is missing.
	set(em_package_deps)
	foreach(arg ${em_package_files})
		string(REGEX REPLACE "@.*$" "" src "${arg}")
		if (IS_DIRECTORY "${src}")
			file(GLOB_RECURSE dir_files CONFIGURE_DEPENDS "${src}/*")
			list(APPEND em_package_deps ${dir_files})
		else()
			list(APPEND em_package_deps "${src}")
		endif()
	endforeach()
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.data
		COMMAND ${EMSCRIPTEN_ROOT_PATH}/tools/file_packager${EMCC_SUFFIX} ${output}.data --quiet --preload ${em_package_files} --js-output=${output}.js
		DEPENDS ${em_package_deps}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	add_custom_target(build_${output}.data ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.data)
	set(em_package_component)
	if (EMP_COMPONENT)
		set(em_package_component COMPONENT ${EMP_COMPONENT})
	endif()
	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/${output}.data
		${CMAKE_CURRENT_BINARY_DIR}/${output}.js
		DESTINATION .
		${em_package_component}
	)
endfunction()

function(build_game game)
	get_target_property(t3make Tads::t3make IMPORTED_LOCATION_RELEASE)
	if (NOT t3make)
		get_target_property(t3make Tads::t3make IMPORTED_LOCATION_RELWITHDEBINFO)
	endif()
	if (NOT t3make)
		get_target_property(t3make Tads::t3make IMPORTED_LOCATION_DEBUG)
	endif()
	if (NOT t3make)
		get_target_property(t3make Tads::t3make IMPORTED_LOCATION_MINSIZEREL)
	endif()
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${game}.t3
		COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${t3make} -FI ${Tads_ROOT_DIR}/include -FL ${Tads_ROOT_DIR}/lib -f ${CMAKE_CURRENT_SOURCE_DIR}/${game}.t3m -o ${CMAKE_CURRENT_BINARY_DIR}/${game}.t3
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	add_custom_target(build_${game}.t3 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${game}.t3)
	if (EMSCRIPTEN)
		em_package(${game} ${CMAKE_CURRENT_BINARY_DIR}/${game}.t3@${game}.t3)
	endif()
endfunction()