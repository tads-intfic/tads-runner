set(commonsrc
	ler.c
	mcm.c
	mcs.c
    mch.c
	obj.c
	cmd.c
	errmsg.c
	fioxor.c
	os0.c
	argize.c
	oserr.c
	cmap.c
	regex.c
	osifc.c
)

if (WIN32)
	set(commonsrc
		${commonsrc}
		msdos/oswinmem.c
	)
endif()

set(cmnrunsrc_nohtml
	${commonsrc}
	dat.c
	lst.c
    out.c
	voc.c
	bif.c
    suprun.c
)

if (WIN32)
	set(cmnrunsrc_nohtml
		${cmnrunsrc_nohtml}
		msdos/osdosex.c
	)
endif()

set(cmnrunsrc
	${cmnrunsrc_nohtml}
	output.c
    indlg_os.c
)

set(runsrc_extra
	vocab.c
	execmd.c
	runstat.c
    ply.c
    linfdum.c
	fio.c
	getstr.c
    qas.c
	dbgtr.c
    askf_os.c
)

if (WIN32)
	set(runsrc_extra
		${runsrc_extra}
		msdos/osdosstk.c
		msdos/ossdos32.c
		msdos/osdoscon.c
	)
endif()

add_library(tr32h STATIC
	${cmnrunsrc}
	${runsrc_extra}
	run.c
    trd.c
	
	bifgdum.c
	osnoui.c
	osstzprs.c
)


target_compile_definitions(tr32h PRIVATE
	DBG_OFF
	HTMLTADS
	USE_HTML
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(tr32h PRIVATE
		__x86_64__
	)
endif()

target_include_directories(tr32h PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (WIN32)
	target_sources(tr32h PRIVATE
		msdos/oswin.c
        msdos/osdosnui.c
		msdos/ostzw32.c
	)
	target_include_directories(tr32h PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/msdos
	)
	target_compile_definitions(tr32h PRIVATE
		MICROSOFT
		RUNTIME
		MSDOS
		_WINDOWS
		T_WIN32
	)
else()
	target_include_directories(tr32h PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/unix
	)
	target_compile_definitions(tr32h PRIVATE
		UNIX
		LINUX_386
		OSANSI
	)
endif()

add_library(Tads::tr32h ALIAS tr32h)


###

set(dbg_extra
	vocab.c
	execmd.c
	runstat.c
    ply.c
	fio.c
	sup.c
    linf.c
	prs.c
	emt.c
	tok.c
	tdd.c
	dbg.c
	dbgrun.c
	objcomp.c
	tokth.c
    voccomp.c
	getstr.c
    qas.c
    run.c
    askf_os.c
)

if (WIN32)
	set(dbg_extra
		${dbg_extra}
		msdos/os0td.c
		msdos/osdbg.c
		msdos/osdosstk.c
		msdos/ossdos32.c
		msdos/osdoscon.c
	)
endif()

add_library(trd32h STATIC
	${cmnrunsrc}
	${dbg_extra}
	run.c
    trd.c
	
	bifgdum.c
	osnoui.c
	osstzprs.c
)


target_compile_definitions(trd32h PRIVATE
	HTMLTADS
	USE_HTML
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(trd32h PRIVATE
		__x86_64__
	)
endif()

target_include_directories(trd32h PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (WIN32)
	target_sources(trd32h PRIVATE
		msdos/oswin.c
        msdos/osdosnui.c
		msdos/ostzw32.c
	)
	target_include_directories(trd32h PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/msdos
	)
	target_compile_definitions(trd32h PRIVATE
		MICROSOFT
		RUNTIME
		MSDOS
		_WINDOWS
		T_WIN32
	)
else()
	target_include_directories(trd32h PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/unix
	)
	target_compile_definitions(trd32h PRIVATE
		UNIX
		LINUX_386
		OSANSI
	)
endif()

add_library(Tads::trd32h ALIAS trd32h)


if (WIN32)
	add_executable(maketrx32
		msdos/dosmktrx.c
	)

	target_compile_definitions(maketrx32 PRIVATE
		MKTRX_SETICON
	)

	add_executable(Tads::maketrx32 ALIAS maketrx32)
	
	install(TARGETS 
		maketrx32
		EXPORT TadsTargets
		DESTINATION .
	)
endif()
