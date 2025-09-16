
function(make_t3r output)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r
		COMMAND Tads::t3res ARGS -create ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r -add ${ARGN}
		DEPENDS Tads::t3res
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	add_custom_target(${output}.t3r DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${output}.t3r)
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
	add_custom_target(${output} DEPENDS ${trx_target})
endfunction()
