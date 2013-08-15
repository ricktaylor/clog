AC_PREREQ(2.61)

AC_INIT(clog,0.0.1)
AC_CONFIG_SRCDIR([lib/tokenizer.ragel])
AC_CONFIG_HEADERS([autoheader_dummy.h])

AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([
	Makefile
])

AM_INIT_AUTOMAKE([foreign 1.10 no-define])

# Use the latest short output macro if available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# I like maintainer mode
AM_MAINTAINER_MODE

AC_CANONICAL_HOST

# Set the WIN32 conditional in the makefiles if the host is MinGW
# If the host is Cygwin, error
AS_CASE([$host_os],
	[*cygwin*],[AC_MSG_ERROR([Don't build for cygwin, use a native Win32 port such as MinGW])],
	[mingw*],[win32=true]
)
AM_CONDITIONAL([WIN32],[test "x$win32" = "xtrue"])

# Add the --enable-debug arg
AC_ARG_ENABLE([debug],AS_HELP_STRING([--enable-debug],[Turn on debugging]),[debug=true],[debug=false])
AM_CONDITIONAL([DEBUG], [test "x$debug" = "xtrue"])

OO_PROG_CC

# Check the multi-threading flags
AS_CASE([$host_os],
	[mingw*],[PTHREAD_CFLAGS=-mthreads AC_SUBST(PTHREAD_LIBS) AC_SUBST(PTHREAD_CFLAGS)],
	[AX_PTHREAD]
)

# Set sensible default CFLAGS if necessary
AS_IF([test "x$oo_test_CFLAGS" != "xset"],
[
  AS_IF([test "x$debug" = "xtrue"],[
    AX_APPEND_COMPILE_FLAGS([-ggdb -O0],[CFLAGS])
  ],[
    AX_APPEND_COMPILE_FLAGS([-O2],[CFLAGS])
  ])
  
  AX_APPEND_COMPILE_FLAGS([-ansi],[CFLAGS])

  AS_CASE([$host_os],
    [mingw*],[AX_APPEND_COMPILE_FLAGS([-march=i686 -mtune=native],[CFLAGS])]
  )
])

# Always try to hide symbols
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden -pipe],[CFLAGS])

# Check for functions we use
AC_CHECK_FUNCS([realpath])

# Always add full warnings
AX_CFLAGS_WARN_ALL
AX_CXXFLAGS_WARN_ALL

# Check for ragel and lemon
AC_PATH_PROG([RAGEL],[ragel])
AS_IF([test "x$RAGEL" = "x"],[AC_MSG_ERROR([Need ragel])])
AC_PATH_PROG([LEMON],[lemon])
AS_IF([test "x$LEMON" = "x"],[AC_MSG_ERROR([Need lemon])])

AC_PROG_MAKE_SET

AC_OUTPUT
