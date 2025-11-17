set(tads2_dir ../tads2)
set(tads3_dir .)

add_library(tads_settings INTERFACE)

target_include_directories(tads_settings 
	INTERFACE 
	${tads2_dir}
	${tads3_dir}
)

option(VM_TARGET_T3 "Compile with T3 VM" ON)
option(VM_TARGET_JS "Compile with JS VM" OFF)
if (WIN32)
set(NETWORK_DEFAULT ON)
else()
find_package(CURL)
set(NETWORK_DEFAULT ${CURL_FOUND})
endif()
option(VM_WITH_NETWORK "Compile with network module" ${NETWORK_DEFAULT})

if (WIN32)
	target_include_directories(tads_settings INTERFACE
		${tads2_dir}/msdos
		${tads3_dir}/win32
	)
	
	target_compile_definitions(tads_settings INTERFACE
		MICROSOFT
		MSDOS
		T_WIN32
	)
	
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		target_compile_definitions(tads_settings INTERFACE
		__x86_64__
	)
	endif()
else()
	find_package(Curses)
	#for now, deactivate it
	set(Curses_FOUND FALSE)
	
	target_include_directories(tads_settings INTERFACE
		${tads2_dir}/unix
		${tads3_dir}/unix
	)
	
	if (NOT Curses_FOUND)
		set(USE_STDIO ON)
		target_compile_definitions(tads_settings INTERFACE
			USE_STDIO
		)
	endif()
	
	target_compile_definitions(tads_settings INTERFACE
		UNIX
		OS_ANSI
	)
	if (NOT EMSCRIPTEN)
		target_compile_definitions(tads_settings INTERFACE
		LINUX_386
	)
	endif()
endif()

if (VM_TARGET_T3)
	target_compile_definitions(tads_settings
		INTERFACE
		TC_TARGET_T3
	)
endif()

if (VM_WITH_NETWORK)
	target_compile_definitions(tads_settings
		INTERFACE
		TADSNET
	)
endif()

target_compile_definitions(tads_settings
    INTERFACE
      $<$<CONFIG:Debug>:VMGLOB_PARAM>
	  $<$<CONFIG:Debug>:T3_DEBUG>
      $<$<CONFIG:RelWithDebInfo>:VMGLOB_VARS>
      $<$<CONFIG:Release>:VMGLOB_VARS>
      $<$<CONFIG:MinSizeRel>:VMGLOB_VARS>
)

if (VM_WITH_NETWORK)
set(builtin_char vmbifregn.cpp)
set(builtin_html vmbifregxn.cpp)
else()
set(builtin_char vmbifreg.cpp)
set(builtin_html vmbifregx.cpp)
endif()

if (VM_TARGET_T3)
	set(target_src_no_link
		${tads3_dir}/tct3.cpp
		${tads3_dir}/tct3stm.cpp
		${tads3_dir}/tct3unas.cpp
		${tads3_dir}/tct3nl.cpp
		${tads3_dir}/tct3_d.cpp
	)
	set(target_src
		${tads3_dir}/tct3.cpp
		${tads3_dir}/tct3stm.cpp
		${tads3_dir}/tct3unas.cpp
		${tads3_dir}/tct3img.cpp
		${tads3_dir}/tct3prg.cpp
	)
elseif (VM_TARGET_JS)
	set(target_src
		${tads3_dir}/tcjs.cpp
	)
endif()

set(dyn_comp_src
	${tads3_dir}/vmrunsym.cpp
    ${tads3_dir}/tcprs.cpp
    ${tads3_dir}/tcprs_rt.cpp
    ${tads3_dir}/tcprsnf.cpp
    ${tads3_dir}/tcprsstm.cpp
    ${tads3_dir}/tcprsnl.cpp
    ${tads3_dir}/tcgen.cpp
    ${tads3_dir}/tcglob.cpp
    ${tads3_dir}/tcerr.cpp
    ${tads3_dir}/tcerrmsg.cpp
    ${tads3_dir}/tctok.cpp
    ${tads3_dir}/tcmain.cpp
    ${tads3_dir}/tcsrc.cpp
    ${tads3_dir}/tchostsi.cpp
    ${tads3_dir}/tclibprs.cpp
    ${tads3_dir}/tccmdutl.cpp
    ${target_src_no_link}
)

if (VM_WITH_NETWORK)
	set(net_src_base
		${tads3_dir}/vmhttpsrv.cpp
		${tads3_dir}/vmhttpreq.cpp
		${tads3_dir}/vmbifnet.cpp
		${tads3_dir}/vmnetui.cpp
		${tads3_dir}/vmnetcfg.cpp
		${tads3_dir}/vmnetfil.cpp
		${tads3_dir}/vmnetfillcl.cpp
		${tads3_dir}/osifcnet.cpp
		${tads3_dir}/vmrefcnt.cpp
	)
	if (WIN32)
		set(net_src_base
			${net_src_base}
			${tads3_dir}/win32/osnetdos.cpp
			${tads3_dir}/win32/osnetwin.cpp
			${tads3_dir}/win32/osnetcli.cpp
			${tads3_dir}/win32/osnet-connect.cpp
			${tads3_dir}/win32/osnet-fgwin.cpp
		)
	else()
		set(net_src_base
			${net_src_base}
			${tads3_dir}/unix/osnetunix.cpp
		)
	endif()
	set(net_src
		${net_src_base}
		${tads3_dir}/vmnet.cpp
	)
	set(nonet_src
		${tads3_dir}/vmhttpdum.cpp
	)
else()
	set(net_src
		${tads3_dir}/vmnetfillcl.cpp
	)
endif()

#t3make

add_executable(t3make
		${tads3_dir}/tcmakecl.cpp
        ${tads3_dir}/tccmdutl.cpp
        ${tads3_dir}/tcmake.cpp
        ${tads3_dir}/tclibprs.cpp
        ${tads3_dir}/std.cpp
        ${tads3_dir}/std_dbg.cpp
        ${tads3_dir}/tcglob.cpp
        ${tads3_dir}/vmerr.cpp
        ${tads3_dir}/vmerrmsg.cpp
        ${tads3_dir}/utf8.cpp
        ${tads3_dir}/charmap.cpp
        ${tads3_dir}/resload.cpp
        ${tads3_dir}/resldexe.cpp
        ${tads3_dir}/tcmain.cpp
        ${tads3_dir}/rcmain.cpp
        ${tads3_dir}/tcerr.cpp
        ${tads3_dir}/tcerrmsg.cpp
        ${tads3_dir}/tchostsi.cpp
        ${tads3_dir}/tcsrc.cpp
        ${tads3_dir}/tctok.cpp
        ${tads3_dir}/tcprs.cpp
        ${tads3_dir}/tcprsfil.cpp
        ${tads3_dir}/tcprsstm.cpp
        ${tads3_dir}/tcprsprg.cpp
        ${tads3_dir}/tcprsimg.cpp
        ${tads3_dir}/tcgen.cpp
        ${tads3_dir}/tcgenfil.cpp
        ${target_src}
        ${tads3_dir}/vmhash.cpp
        ${tads3_dir}/vmwrtimg.cpp
        ${tads3_dir}/vmtypedh.cpp
        ${tads3_dir}/vmfile.cpp
        ${tads3_dir}/vminit.cpp
        ${tads3_dir}/vmini_nd.cpp
        ${tads3_dir}/vminitim.cpp
        ${tads3_dir}/vmcfgmem.cpp
        ${tads3_dir}/vmobj.cpp
        ${tads3_dir}/vmundo.cpp
        ${tads3_dir}/vmtobj.cpp
        ${tads3_dir}/vmpat.cpp
        ${tads3_dir}/vmstrcmp.cpp
        ${tads3_dir}/vmdict.cpp
        ${tads3_dir}/vmgram.cpp
        ${tads3_dir}/vmstr.cpp
        ${tads3_dir}/vmcoll.cpp
        ${tads3_dir}/vmiter.cpp
        ${tads3_dir}/vmfref.cpp
        ${tads3_dir}/vmlst.cpp
        ${tads3_dir}/vmsort.cpp
        ${tads3_dir}/vmsortv.cpp
        ${tads3_dir}/vmbignum.cpp
        ${tads3_dir}/vmbignumlib.cpp
        ${tads3_dir}/vmvec.cpp
        ${tads3_dir}/vmintcls.cpp
        ${tads3_dir}/vmanonfn.cpp
        ${tads3_dir}/vmlookup.cpp
        ${tads3_dir}/vmstrbuf.cpp
        ${tads3_dir}/vmdynfunc.cpp
        ${tads3_dir}/vmbytarr.cpp
        ${tads3_dir}/vmdate.cpp
        ${tads3_dir}/vmtzobj.cpp
        ${tads3_dir}/vmtz.cpp
        ${tads3_dir}/vmcset.cpp
        ${tads3_dir}/vmfilobj.cpp
        ${tads3_dir}/vmfilnam.cpp
        ${tads3_dir}/vmtmpfil.cpp
        ${tads3_dir}/vmpack.cpp
        ${tads3_dir}/vmnetfillcl.cpp
        ${tads3_dir}/vmstack.cpp
        ${tads3_dir}/vmpool.cpp
        ${tads3_dir}/vmpoolim.cpp
        ${tads3_dir}/vmtype.cpp
        ${tads3_dir}/vmglob.cpp
        ${tads3_dir}/vmrun.cpp
        ${tads3_dir}/vmop.cpp
        ${tads3_dir}/vmfunc.cpp
        ${tads3_dir}/vmmeta.cpp
        ${tads3_dir}/vmpreini.cpp
        ${tads3_dir}/vmimgrb.cpp
        ${tads3_dir}/vmbif.cpp
        ${tads3_dir}/vmbifc.cpp
        ${tads3_dir}/vmimage.cpp
        ${tads3_dir}/vmrunsym.cpp
        ${tads3_dir}/vmimg_nd.cpp
        ${tads3_dir}/vmsrcf.cpp
        ${tads3_dir}/vmbiftad.cpp
        ${tads3_dir}/vmisaac.cpp
        ${tads3_dir}/vmbiftio.cpp
        ${tads3_dir}/askf_tx3.cpp
        ${tads3_dir}/indlg_tx3.cpp
        ${tads3_dir}/vmsave.cpp
        ${tads3_dir}/vmcrc.cpp
        ${tads3_dir}/vmbift3.cpp
        ${tads3_dir}/vmbt3_nd.cpp
        ${tads3_dir}/vmregex.cpp
        ${tads3_dir}/vmconsol.cpp
        ${tads3_dir}/vmconmor.cpp
        ${tads3_dir}/vmconhmp.cpp
        ${tads3_dir}/os_stdio.cpp
        ${tads3_dir}/vmhosttx.cpp
        ${tads3_dir}/vmhostsi.cpp
        ${tads3_dir}/vmlog.cpp
        ${tads3_dir}/sha2.cpp
        ${tads3_dir}/md5.cpp
		${tads3_dir}/vmmcreg.cpp
        ${tads3_dir}/vmbifreg.cpp
		${tads3_dir}/derived/vmuni_cs.cpp
        ${tads2_dir}/osifc.c
        ${tads2_dir}/osnoui.c
        ${tads2_dir}/osstzprs.c
        ${tads2_dir}/osrestad.c
        ${nonet_src}
)

target_include_directories(t3make PRIVATE
		${tads3_dir}/test
	)

if (WIN32)
	target_sources(t3make
		PRIVATE
		${tads2_dir}/msdos/osdosex.c
		${tads2_dir}/msdos/osdos.c
        ${tads2_dir}/msdos/osdosnui.c
        ${tads2_dir}/msdos/oswinmem.c
		${tads2_dir}/msdos/osnogen.c
		${tads2_dir}/msdos/ostzw32.c
	)
else()
	target_sources(t3make
		PRIVATE
		${tads2_dir}/unix/osunixt.c
		${tads2_dir}/ostzposix.c
		${tads3_dir}/unix/osunix.c
		${tads3_dir}/unix/osunix3.c
	)
	target_compile_definitions(t3make PRIVATE
		USE_STDIO
		BINDIR="bin"
		RESDIR="res"
		LIBDIR="lib"
		INCDIR="include"
	)
endif()

target_link_libraries(t3make PRIVATE tads_settings)


#t3res

add_executable(t3res
        ${tads3_dir}/rcmaincl.cpp
        ${tads3_dir}/rcmain.cpp
        ${tads3_dir}/std.cpp
        ${tads3_dir}/derived/vmuni_cs.cpp		
        ${tads2_dir}/osnoui.c
)

if (WIN32)
	target_sources(t3res
		PRIVATE
		${tads2_dir}/msdos/osdosex.c
		${tads2_dir}/msdos/osdos.c
        ${tads2_dir}/msdos/osdosnui.c
        ${tads2_dir}/msdos/oswinmem.c
		${tads2_dir}/msdos/osnogen.c
	)
else()
	target_sources(t3res
		PRIVATE
		${tads2_dir}/unix/osunixt.c
	)
	target_compile_definitions(t3res PRIVATE
		USE_STDIO
	)
endif()

target_link_libraries(t3res PRIVATE tads_settings)

add_executable(Tads::t3res ALIAS t3res)

#mkchrtab
add_executable(mkchrtab
        ${tads3_dir}/mkchrtab.cpp
        ${tads3_dir}/std.cpp
        ${tads3_dir}/std_dbg.cpp
        ${tads3_dir}/derived/vmuni_cs.cpp		
        ${tads2_dir}/osnoui.c
)

if (WIN32)
	target_sources(mkchrtab
		PRIVATE
        ${tads2_dir}/msdos/osdosnui.c
        ${tads2_dir}/msdos/oswinmem.c
	)
else()
	target_sources(mkchrtab
		PRIVATE
		${tads2_dir}/unix/osunixt.c
	)
	target_compile_definitions(mkchrtab PRIVATE
		USE_STDIO
	)
endif()

target_link_libraries(mkchrtab PRIVATE tads_settings)

#t3core

add_library(t3core STATIC
	${tads3_dir}/vmmain.cpp
	${tads3_dir}/std.cpp
	${tads3_dir}/std_dbg.cpp
	${tads3_dir}/charmap.cpp
	${tads3_dir}/resload.cpp
	${tads3_dir}/resldexe.cpp
	${tads3_dir}/vminit.cpp
	${tads3_dir}/vmini_nd.cpp
	${tads3_dir}/vminitim.cpp
	${tads3_dir}/vmcfgmem.cpp
	${tads3_dir}/vmobj.cpp
	${tads3_dir}/vmundo.cpp
	${tads3_dir}/vmtobj.cpp
	${tads3_dir}/vmpat.cpp
	${tads3_dir}/vmstrcmp.cpp
	${tads3_dir}/vmdict.cpp
	${tads3_dir}/vmgram.cpp
	${tads3_dir}/vmstr.cpp
	${tads3_dir}/vmcoll.cpp
	${tads3_dir}/vmiter.cpp
	${tads3_dir}/vmfref.cpp
	${tads3_dir}/vmlst.cpp
	${tads3_dir}/vmsort.cpp
	${tads3_dir}/vmsortv.cpp
	${tads3_dir}/vmbignum.cpp
	${tads3_dir}/vmbignumlib.cpp
	${tads3_dir}/vmvec.cpp
	${tads3_dir}/vmintcls.cpp
	${tads3_dir}/vmanonfn.cpp
	${tads3_dir}/vmlookup.cpp
	${tads3_dir}/vmstrbuf.cpp
	${tads3_dir}/vmdynfunc.cpp
	${tads3_dir}/vmbytarr.cpp
	${tads3_dir}/vmdate.cpp
	${tads3_dir}/vmtzobj.cpp
	${tads3_dir}/vmtz.cpp
	${tads3_dir}/vmcset.cpp
	${tads3_dir}/vmfilobj.cpp
	${tads3_dir}/vmfilnam.cpp
	${tads3_dir}/vmtmpfil.cpp
	${tads3_dir}/vmpack.cpp
	${tads3_dir}/sha2.cpp
	${tads3_dir}/md5.cpp
	${tads3_dir}/vmstack.cpp
	${tads3_dir}/vmerr.cpp
	${tads3_dir}/vmerrmsg.cpp
	${tads3_dir}/vmpool.cpp
	${tads3_dir}/vmpoolim.cpp
	${tads3_dir}/vmtype.cpp
	${tads3_dir}/vmtypedh.cpp
	${tads3_dir}/utf8.cpp
	${tads3_dir}/vmglob.cpp
	${tads3_dir}/vmrun.cpp
	${tads3_dir}/vmop.cpp
	${tads3_dir}/vmfunc.cpp
	${tads3_dir}/vmmeta.cpp
	${tads3_dir}/vmsa.cpp
	${tads3_dir}/vmbif.cpp
	${tads3_dir}/vmbifl.cpp
	${tads3_dir}/vmimage.cpp
	${tads3_dir}/vmimg_nd.cpp
	${tads3_dir}/vmsrcf.cpp
	${tads3_dir}/vmfile.cpp
	${tads3_dir}/vmsave.cpp
	${tads3_dir}/vmcrc.cpp
	${tads3_dir}/vmbiftad.cpp
	${tads3_dir}/vmisaac.cpp
	${tads3_dir}/vmbift3.cpp
	${tads3_dir}/vmbt3_nd.cpp
	${tads3_dir}/vmregex.cpp
	${tads3_dir}/vmhash.cpp
	${tads3_dir}/vmlog.cpp
	${tads3_dir}/derived/vmuni_cs.cpp
	${dyn_comp_src}
	${net_src}
)

target_link_libraries(t3core PRIVATE
	tads_settings
)

if (WITH_HTMLTADS)
	#t3htm
	set(html_base
		${tads3_dir}/vminit.cpp
		${tads3_dir}/vmmain.cpp
		${tads3_dir}/std.cpp
		${tads3_dir}/std_dbg.cpp
		${tads3_dir}/charmap.cpp
		${tads3_dir}/resload.cpp
		${tads3_dir}/resldexe.cpp
		${tads3_dir}/vmconsol.cpp
		${tads3_dir}/vmconhtm.cpp
		${tads3_dir}/vmconhmp.cpp
		${tads3_dir}/vminitim.cpp
		${tads3_dir}/vmcfgmem.cpp
		${tads3_dir}/vmobj.cpp
		${tads3_dir}/vmundo.cpp
		${tads3_dir}/vmtobj.cpp
		${tads3_dir}/vmpat.cpp
		${tads3_dir}/vmstrcmp.cpp
		${tads3_dir}/vmdict.cpp
		${tads3_dir}/vmgram.cpp
		${tads3_dir}/vmhash.cpp
		${tads3_dir}/vmstr.cpp
		${tads3_dir}/vmcoll.cpp
		${tads3_dir}/vmiter.cpp
		${tads3_dir}/vmfref.cpp
		${tads3_dir}/vmlst.cpp
		${tads3_dir}/vmsort.cpp
		${tads3_dir}/vmsortv.cpp
		${tads3_dir}/vmbignum.cpp
		${tads3_dir}/vmbignumlib.cpp
		${tads3_dir}/vmvec.cpp
		${tads3_dir}/vmintcls.cpp
		${tads3_dir}/vmanonfn.cpp
		${tads3_dir}/vmlookup.cpp
		${tads3_dir}/vmstrbuf.cpp
		${tads3_dir}/vmdynfunc.cpp
		${tads3_dir}/vmbytarr.cpp
		${tads3_dir}/vmdate.cpp
		${tads3_dir}/vmtzobj.cpp
		${tads3_dir}/vmtz.cpp
		${tads3_dir}/vmcset.cpp
		${tads3_dir}/vmfilobj.cpp
		${tads3_dir}/vmfilnam.cpp
		${tads3_dir}/vmtmpfil.cpp
		${tads3_dir}/vmpack.cpp
		${tads3_dir}/vmstack.cpp
		${tads3_dir}/vmfunc.cpp
		${tads3_dir}/vmerr.cpp
		${tads3_dir}/vmerrmsg.cpp
		${tads3_dir}/vmpool.cpp
		${tads3_dir}/vmpoolim.cpp
		${tads3_dir}/vmtype.cpp
		${tads3_dir}/vmtypedh.cpp
		${tads3_dir}/utf8.cpp
		${tads3_dir}/vmglob.cpp
		${tads3_dir}/vmmeta.cpp
		${tads3_dir}/vmsa.cpp
		${tads3_dir}/vmop.cpp
		${tads3_dir}/vmbif.cpp
		${tads3_dir}/vmbifl.cpp
		${tads3_dir}/vmimage.cpp
		${tads3_dir}/vmsrcf.cpp
		${tads3_dir}/vmfile.cpp
		${tads3_dir}/vmbiftad.cpp
		${tads3_dir}/vmisaac.cpp
		${tads3_dir}/vmbiftio.cpp
		${tads3_dir}/vmbiftix.cpp
		${tads3_dir}/askf_os3.cpp
		${tads3_dir}/indlg_os3.cpp
		${tads3_dir}/vmsave.cpp
		${tads3_dir}/vmcrc.cpp
		${tads3_dir}/vmbift3.cpp
		${tads3_dir}/vmregex.cpp
		${tads3_dir}/vmhosttx.cpp
		${tads3_dir}/vmhostsi.cpp
		${tads3_dir}/vmlog.cpp
		${tads3_dir}/vmmcreg.cpp
		${tads3_dir}/${builtin_html}
		${tads3_dir}/sha2.cpp
		${tads3_dir}/md5.cpp
		${tads3_dir}/derived/vmuni_cs.cpp
		${tads3_dir}/resfind.cpp
		${tads3_dir}/gameinfo.cpp
		${tads3_dir}/gameinfl.cpp
	)

	add_library(t3htm STATIC
		${html_base}
		${tads3_dir}/vmrun.cpp
		${tads3_dir}/vmimg_nd.cpp
		${tads3_dir}/vmbt3_nd.cpp
		${tads3_dir}/vmini_nd.cpp
		${dyn_comp_src}
		${net_src}
	)

	target_link_libraries(t3htm PUBLIC
		tads_settings
	)
	
	target_include_directories(t3htm PRIVATE
		${HTMLTADS_DIR}/htmltads
		${HTMLTADS_DIR}/htmltads/win32
	)

	target_compile_definitions(t3htm PUBLIC
		T3_COMPILING_FOR_HTML
	)

	add_library(Tads::t3htm ALIAS t3htm)
	
	#htmd
	
	add_library(t3htm_d STATIC
		${html_base}
		${tads3_dir}/vmrun.cpp
		${tads3_dir}/vmini_d.cpp
        ${tads3_dir}/vmdbg.cpp
		${tads3_dir}/vmimg_d.cpp
		${tads3_dir}/vmbt3_d.cpp	
		${dyn_comp_src}
		${net_src}
	)

	target_link_libraries(t3htm_d PUBLIC
		tads_settings
	)
	
	target_include_directories(t3htm_d PRIVATE
		${HTMLTADS_DIR}/htmltads
		${HTMLTADS_DIR}/htmltads/win32
	)

	target_compile_definitions(t3htm_d PUBLIC
		T3_COMPILING_FOR_HTML
		VM_DEBUGGER
		VM_PROFILER
	)

	add_library(Tads::t3htm_d ALIAS t3htm_d)
endif()

#t3run

set(tads3src
	${tads3_dir}/vmmaincl.cpp	
	${tads3_dir}/vmconsol.cpp
	${tads3_dir}/vmconmor.cpp
	${tads3_dir}/vmconhmp.cpp
	${tads3_dir}/vmbiftio.cpp
	${tads3_dir}/askf_os3.cpp
	${tads3_dir}/indlg_tx3.cpp
	${tads3_dir}/vmhosttx.cpp
	${tads3_dir}/vmhostsi.cpp
	${tads3_dir}/vmmcreg.cpp
	
	${tads3_dir}/${builtin_char}
		
	${tads2_dir}/osifc.c
	${tads2_dir}/osnoui.c
	${tads2_dir}/osstzprs.c
	${tads2_dir}/osrestad.c
	#${tads2_dir}/osgen3.c
)

if (USE_STDIO)
set(tads3src
	${tads3src}
	${tads3_dir}/os_stdio.cpp
)
else()
set(tads3src
	${tads3src}
	${tads2_dir}/osgen3.c
)
endif()

add_executable(t3run
	${tads3src}
)

target_link_libraries(t3run PRIVATE
	t3core
	tads_settings
)

if (WIN32)
	target_sources(t3run
		PRIVATE
		${tads2_dir}/msdos/osdos.c
		${tads2_dir}/msdos/osdosnui.c	
		${tads2_dir}/msdos/osscolor.c
		${tads2_dir}/msdos/ossdos32.c
		${tads2_dir}/msdos/osdoscon.c
		${tads2_dir}/msdos/osdosex.c
		${tads2_dir}/msdos/oswinmem.c
		${tads2_dir}/msdos/ostzw32.c
		
		${tads3_dir}/win32/t3run.rc
	)
	
	if (VM_WITH_NETWORK)
		target_link_libraries(t3run PRIVATE Ws2_32.lib Wininet.lib)
	endif()
	
	target_compile_definitions(t3run
		PRIVATE
		RUNTIME
	)
else()
	
	target_compile_definitions(t3run PRIVATE
		BINDIR="bin"
		RESDIR="res"
		LIBDIR="lib"
		INCDIR="include"
	)
	
	if (Curses_FOUND)
		target_compile_definitions(t3run PRIVATE
			HAVE_TPARM
		)
		target_link_libraries(t3run PRIVATE ${CURSES_LIBRARIES})
	endif()
	
	target_sources(t3run
		PRIVATE
		${tads2_dir}/unix/osunixt.c
		${tads2_dir}/ostzposix.c
		${tads2_dir}/unix/tputs.c
		${tads2_dir}/unix/tparm.c
		${tads3_dir}/unix/osunix.c
		${tads3_dir}/unix/osunix3.c
	)
	
	if (VM_WITH_NETWORK)
		if (NOT CURL_FOUND)
			message(FATAL_ERROR "Curl required for network support")
		endif()
		target_link_libraries(t3run PRIVATE CURL::libcurl)
	endif()
endif()

add_custom_command(
        TARGET t3run POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:t3run>/charmap
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/charmap/cmaplib.t3r $<TARGET_FILE_DIR:t3run>/charmap/)

#############
###Install###
#############

install(TARGETS 
	mkchrtab t3make t3res t3run
	EXPORT TadsTargets
	DESTINATION .)
	
install(FILES
	${tads3_dir}/tz/timezones.t3tz
	DESTINATION .
)
	
install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/charmap/cmaplib.t3r
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/charmap/README.txt
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/charmap/us-ascii.txt
	DESTINATION charmap
)

install(DIRECTORY
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/charmap/source
	DESTINATION charmap
)

install(DIRECTORY
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/doc
	DESTINATION .
)

install(DIRECTORY
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/samples
	DESTINATION .
)

install(FILES
	${tads3_dir}/samples/starta3.t
	${tads3_dir}/samples/starti3.t
	DESTINATION lib/adv3/samples
)

install(FILES
	${tads3_dir}/samples/startB3.t
	DESTINATION lib/samples
)

install(DIRECTORY
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/lib
	DESTINATION .
)

install(DIRECTORY
	${CMAKE_CURRENT_SOURCE_DIR}/${tads3_dir}/include
	DESTINATION .
)

install(FILES
	${tads3_dir}/test/data/anonobj.t
	${tads3_dir}/test/data/calc.t
	${tads3_dir}/test/data/gram.t
	${tads3_dir}/test/data/noun_ph.t
	${tads3_dir}/test/data/template.t
	DESTINATION samples
)