AC_INIT([libbitcoin-watcher], [1.0.10], [amir@unsystem.net])
AC_USE_SYSTEM_EXTENSIONS
AC_LANG(C++)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])
AM_PROG_AR
LT_INIT
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_GNU_SOURCE
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

PKG_PROG_PKG_CONFIG

AM_CXXFLAGS="-ggdb -g3 -Wall -Wno-missing-braces -pedantic -Wextra -fstack-protector-all -DDEBUG -fvisibility=hidden -fvisibility-inlines-hidden"
AC_SUBST([AM_CXXFLAGS])

PKG_CHECK_MODULES([libbitcoin], [libbitcoin libbitcoin-client])

AC_ARG_WITH([pkgconfigdir], AS_HELP_STRING([--with-pkgconfigdir=PATH],
    [Path to the pkgconfig directory [[LIBDIR/pkgconfig]]]),
    [pkgconfigdir="$withval"], [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST([pkgconfigdir])

AC_CONFIG_FILES([Makefile include/bitcoin/Makefile src/Makefile libbitcoin-watcher.pc])
AC_OUTPUT

