dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.63)
AC_INIT(eblook, 1.6.1+media-20200414)
AC_CONFIG_SRCDIR([eblook.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE
AH_TEMPLATE([FALLBACK_ENCODING],[definition for Default Char Set])
AC_CONFIG_MACRO_DIRS([m4])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S

dnl Checks for libraries.

dnl Checks for header files.
AC_CHECK_HEADERS(langinfo.h locale.h iconv.h)
dnl AC_CHECK_HEADERS(langinfo.h locale.h iconv.h sys/types.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

dnl *
dnl * large file support.
dnl *
eb_SYS_LARGEFILE

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_CHECK_FUNCS(iconv nl_langinfo setlocale vsnprintf isatty)
AC_REPLACE_FUNCS(strcasecmp strdup)

dnl *
dnl * System services
dnl *
AC_EXEEXT
AC_OBJEXT

TARGET=${target}

dnl * Default Charset
case "$target" in
     *-*-hpux*) default_fallback="SHIFT-JIS" ;;
     *-*-cygwin*) default_fallback="SHIFT-JIS" ;;
     *-*-mingw32*) default_fallback="SHIFT-JIS" ;;
     *) default_fallback="EUC-JP" ;;
esac
AC_ARG_WITH(fallback-encoding,
	[  --with-fallback-encoding[=encoding] (default="EUC-JP" ("SHIFT-JIS" on HP-UX,
                                     Cygwin and MinGW systems))],
	[AC_DEFINE_UNQUOTED(FALLBACK_ENCODING,"${fallback_encoding}")],
	[AC_DEFINE_UNQUOTED(FALLBACK_ENCODING,"${default_fallback}")])

dnl *
dnl * --with-readline option.
dnl *
AC_ARG_WITH(readline,
[  --with-readline     compile with gnu readline library],[
  tcap=""
  for termlib in ncurses curses termcap terminfo termlib; do
    AC_CHECK_LIB($termlib, tputs, 
		[tcap="-l$termlib"]
		break
    )
    termlib="no"
  done
  if test ! "$termlib" = "no" ; then
    AC_CHECK_HEADER(readline/readline.h,[AC_DEFINE(USE_READLINE, 1, [with GNU readline library]) LIBS="$LIBS -lreadline -l$termlib"],)
  fi
], )

dnl *
dnl * --with-pager option.
dnl *
AC_ARG_WITH(pager,
[  --with-pager     compile with pager option],
AC_DEFINE(USE_PAGER, 1, [with pager option]), )

dnl EB_CONFIG
save_sysconfdir=$sysconfdir
dnl for linux
[ if test ! -f $sysconfdir/eb.conf; then
    if test -f /etc/eb.conf ; then
      sysconfdir=/etc
    fi
  fi ]
eb_LIB_EBU
if test $ENABLE_EBU = no; then
  eb_LIB_EB4
fi
sysconfdir=$sysconfdir
CPPFLAGS="$CPPFLAGS $EBCONF_PTHREAD_CPPFLAGS $EBCONF_EBINCS $EBCONF_ZLIBINCS $EBCONF_INTLINCS"
CFLAGS="$CFLAGS $EBCONF_PTHREAD_CFLAGS"
LDFLAGS="$LDFAGS $EBCONF_PTHREAD_LDFLAGS"
LIBS="$LIBS $EBCONF_EBLIBS $EBCONF_ZLIBLIBS $EBCONF_INTLLIBS"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
