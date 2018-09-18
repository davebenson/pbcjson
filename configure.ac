AC_INIT([pbcrep], [0.0.0], [lahiker42@gmail.com], [pbcrep], [http://www.example.com])
AC_PREREQ([2.68])
AM_INIT_AUTOMAKE([subdir-objects foreign -Wall -Werror])
AM_SILENT_RULES([yes])
AC_PROG_RANLIB()
PKG_CHECK_MODULES(LPBC, libprotobuf-c)
AM_PROG_AR()
AC_CONFIG_FILES([Makefile])
AC_CONFIG_SRCDIR([src/pbcrep/parser.c])

AC_CHECK_HEADERS([execinfo.h])
AC_SEARCH_LIBS([backtrace],
               [execinfo],
               AC_DEFINE(HAS_BACKTRACE, 1),
               AC_DEFINE(HAS_BACKTRACE, 0))
AC_PROG_CC
AC_OUTPUT
