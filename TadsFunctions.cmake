
function(make_t3r output)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r
		COMMAND Tads::t3res ARGS -create ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r -add ${ARGN}
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

function(em_package output)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.data
		COMMAND ${EMSCRIPTEN_ROOT_PATH}/tools/file_packager${EMCC_SUFFIX} ${output}.data --preload ${ARGN} --js-output=${output}.js
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	add_custom_target(build_${output}.data ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.data)
	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/${output}.data
		${CMAKE_CURRENT_BINARY_DIR}/${output}.js
		DESTINATION .
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