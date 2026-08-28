#----------------------------------------------------------------------------------------------------------------------
# Injected via CMAKE_PROJECT_INCLUDE by Windows.MSVC.toolchain.cmake.
#
# It runs once, immediately after the top-level project() call, which is the
# earliest point at which enable_language() may be used (a toolchain file
# itself cannot call it).
#
# A normal Visual Studio generator build enables the RC language implicitly
# as part of setting up the MSVC toolset, so .rc resource scripts listed as
# target sources are compiled and linked automatically.  This toolchain
# bypasses that path - it provides the C/CXX compilers explicitly and, when
# the target architecture differs from the host, presents itself as a cross
# toolchain - so RC is never enabled and .rc files are silently demoted to
# non-building "None" entries.  Enable RC here to restore the expected
# behaviour.  enable_language() is idempotent, so this is a no-op if RC is
# already enabled.
#----------------------------------------------------------------------------------------------------------------------
if(WIN32)
    enable_language(RC)
endif()
