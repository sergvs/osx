
# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libX11], [1.4.3],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [libX11])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([src/config.h include/X11/XlibConf.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
# Set common system defines for POSIX extensions, such as _GNU_SOURCE
# Must be called before any macros that run the compiler (like AC_PROG_LIBTOOL)
# to avoid autoconf errors.
AC_USE_SYSTEM_EXTENSIONS

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Initialize libtool
AC_PROG_LIBTOOL

# Require xorg-macros minimum of 1.12 for DocBook external references
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.12 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.12)
XORG_DEFAULT_OPTIONS
XORG_ENABLE_SPECS
XORG_WITH_XMLTO(0.0.22)
XORG_WITH_FOP([no])
XORG_WITH_XSLTPROC
XORG_CHECK_SGML_DOCTOOLS(1.7)
XORG_PROG_RAWCPP

# Required when PKG_CHECK_MODULES called within an if statement
PKG_PROG_PKG_CONFIG

if test x"$CC_FOR_BUILD" = x; then
	if test x"$cross_compiling" = xyes; then
	       AC_CHECK_PROGS(CC_FOR_BUILD, gcc cc)
	else
	       CC_FOR_BUILD="$CC"
	fi
fi
AC_SUBST([CC_FOR_BUILD])

if test x"$CPPFLAGS_FOR_BUILD" = x; then
	if test ! x"$cross_compiling" = xyes; then
		CPPFLAGS_FOR_BUILD=${CPPFLAGS}
	fi
fi
AC_SUBST(CPPFLAGS_FOR_BUILD)

if test x"$CFLAGS_FOR_BUILD" = x; then
	if test ! x"$cross_compiling" = xyes; then
		CFLAGS_FOR_BUILD=${CFLAGS}
	fi
fi
AC_SUBST(CFLAGS_FOR_BUILD)

if test x"$LDFLAGS_FOR_BUILD" = x; then
	if test ! x"$cross_compiling" = xyes; then
		LDFLAGS_FOR_BUILD=${LDFLAGS}
	fi
fi
AC_SUBST(LDFLAGS_FOR_BUILD)

# Find perl for "make check" tests in nls/Makefile.am
AC_ARG_WITH(perl,
	AS_HELP_STRING([--with-perl=<path>],
			[path to perl interpreter for build-time tests]),
	[PERL=$withval ; AC_MSG_CHECKING([perl]) ;
	 AC_MSG_RESULT([(from --with-perl) $PERL])],
	AC_CHECK_PROGS([PERL], [perl], [no]))
AM_CONDITIONAL(HAVE_PERL, test x$PERL != xno)

# Checks for pkg-config packages

# Always required
X11_REQUIRES='xproto >= 7.0.13 xextproto xtrans xcb >= 1.1.92'
X11_EXTRA_DEPS="xcb >= 1.1.92"

PKG_PROG_PKG_CONFIG()

AC_SUBST(X11_EXTRA_DEPS)

# Issue an error if xtrans.m4 was not found and XTRANS_CONNECTION_FLAGS macro
# was not expanded, since libX11 with no transport types is rather useless.
#
# If you're seeing an error here, be sure you installed the lib/xtrans module
# first and if it's not in the default location, that you set the ACLOCAL
# environment variable to find it, such as:
#	ACLOCAL="aclocal -I ${PREFIX}/share/aclocal"
m4_pattern_forbid([^XTRANS_CONNECTION_FLAGS$])

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS

# Secure RPC detection macro from xtrans.m4
XTRANS_SECURE_RPC_FLAGS

# Preferred order to try transports for local connections
AC_MSG_CHECKING([what order to try transports in for local connections])
DEFAULT_LOCAL_TRANS=""
case $host_os in
	solaris*)
		# On Solaris 2.6 through 9, named pipes (LOCAL_TRANS) were
		# faster than Unix domain sockets, but on Solaris 10 & later,
		# Unix domain sockets are faster now.
		if test "$UNIXCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}UNIX_TRANS"
		fi
		if test "$LOCALCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}LOCAL_TRANS"
		fi
		if test "$TCPCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}TCP_TRANS"
		fi
		;;
	linux*)
		# LOCAL_TRANS is used for abstract sockets.
		if test "$UNIXCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}LOCAL_TRANS,UNIX_TRANS"
		fi
		if test "$TCPCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}TCP_TRANS"
		fi
		;;
	*)
		if test "$LOCALCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}LOCAL_TRANS"
		fi
		if test "$UNIXCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}UNIX_TRANS"
		fi
		if test "$TCPCONN" = "yes" ; then
			if test ! "x$DEFAULT_LOCAL_TRANS" = "x" ; then
				DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS},"
			fi
			DEFAULT_LOCAL_TRANS="${DEFAULT_LOCAL_TRANS}TCP_TRANS"
		fi
		;;
esac

AC_ARG_WITH(local-transport-order,
	AS_HELP_STRING([--with-local-transport-order=LIST], [preference sorted list of transport types to try for local connections]),
	[LOCAL_TRANSPORT_LIST=$withval],
	[LOCAL_TRANSPORT_LIST=$DEFAULT_LOCAL_TRANS])
AC_DEFINE_UNQUOTED([LOCAL_TRANSPORT_LIST], [$LOCAL_TRANSPORT_LIST],
   [preference sorted list of transport types to try for local connections])
AC_MSG_RESULT([$LOCAL_TRANSPORT_LIST])

# Check for dlopen
AC_MSG_CHECKING([if run-time linking is supported])
AC_SEARCH_LIBS(dlopen,[dl svld])
if test "x$ac_cv_search_dlopen" = xno; then
	AC_SEARCH_LIBS(shl_load,[dld])
	if test "x$ac_cv_search_shl_load" != xno; then
		AC_DEFINE(HAVE_SHL_LOAD,1,
			  [Use shl_load to load shared libraries])
		AC_CHECK_HEADERS([dl.h])
	fi
else
	AC_DEFINE(HAVE_DLOPEN,1,[Use dlopen to load shared libraries])
	AC_CHECK_HEADERS([dlfcn.h])
fi
if test x$ac_cv_header_dlcfn_h -o x$ac_cv_header_dl_h; then
	HAVE_LOADABLE_MODULES=yes
else
	HAVE_LOADABLE_MODULES=no
fi
AC_MSG_RESULT($HAVE_LOADABLE_MODULES)

AC_MSG_CHECKING([if loadable i18n module support should be enabled])
AC_ARG_ENABLE(loadable-i18n,
	      AS_HELP_STRING([--enable-loadable-i18n],
	       [Controls loadable i18n module support]),
	       [XLIB_LOADABLE_I18N=$enableval],
	       [XLIB_LOADABLE_I18N="no"])
if test x$XLIB_LOADABLE_I18N = xyes; then
	if test x$HAVE_LOADABLE_MODULES = xno; then
		AC_MSG_ERROR([Loadable module support is required to enable loadable i18n module support])
	fi
	AC_DEFINE(USE_DYNAMIC_LC,1,
		  [Split some i18n functions into loadable modules])
	AC_SUBST(I18N_MODULE_LIBS,'${top_builddir}/src/libX11.la')
fi
AC_MSG_RESULT($XLIB_LOADABLE_I18N)

AM_CONDITIONAL(XLIB_LOADABLE_I18N, test x$XLIB_LOADABLE_I18N = xyes)

AC_MSG_CHECKING([if loadable Xcursor library support should be enabled])
AC_ARG_ENABLE(loadable-xcursor,
	      AS_HELP_STRING([--disable-loadable-xcursor],
	       [Controls loadable xcursor library support]),
	       [XLIB_LOADABLE_XCURSOR=$enableval],
	       [XLIB_LOADABLE_XCURSOR=$HAVE_LOADABLE_MODULES])
if test x$XLIB_LOADABLE_XCURSOR = xyes; then
	AC_DEFINE(USE_DYNAMIC_XCURSOR,1,
		  [Use the X cursor library to load cursors])
fi
AC_MSG_RESULT($XLIB_LOADABLE_XCURSOR)

# Checks for header files.
AC_CHECK_HEADERS([sys/select.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([strtol])
# Used in lcFile.c (see also --enable-xlocaledir settings below)
XLOCALEDIR_IS_SAFE="no"
AC_CHECK_FUNC([issetugid], [XLOCALEDIR_IS_SAFE="yes"]
	AC_DEFINE(HASSETUGID,1,[Has issetugid() function]))
AC_CHECK_FUNC([getresuid], [XLOCALEDIR_IS_SAFE="yes"]
	AC_DEFINE(HASGETRESUID,1,[Has getresuid() & getresgid() functions]))
# Used in Font.c
AC_CHECK_FUNC([shmat], AC_DEFINE(HAS_SHM,1,[Has shm*() functions]))

# Checks for system services
# AC_PATH_XTRA

# arch specific things
WCHAR32="1"
case $host_os in
  os2*) os2="true" ; WCHAR32="0" ;;
  *) ;;
esac
AC_SUBST(WCHAR32)

AM_CONDITIONAL(OS2, test x$os2 = xtrue)

AC_ARG_WITH(launchd, AS_HELP_STRING([--with-launchd], [Build with support for Apple's launchd (default: auto)]), [LAUNCHD=$withval], [LAUNCHD=auto])
if test "x$LAUNCHD" = xauto; then
	unset LAUNCHD
	AC_CHECK_PROG(LAUNCHD, [launchd], [yes], [no], [$PATH$PATH_SEPARATOR/sbin])
fi

if test "x$LAUNCHD" = xyes ; then
	AC_DEFINE(HAVE_LAUNCHD, 1, [launchd support available])
	AC_DEFINE(TRANS_REOPEN, 1, [launchd support available])
fi

AC_ARG_ENABLE(xthreads,
              AS_HELP_STRING([--disable-xthreads],
                [Disable Xlib support for Multithreading]),
              [xthreads=$enableval],[xthreads=yes])

AC_CHECK_LIB(c, getpwuid_r, [mtsafeapi="yes"], [mtsafeapi="no"])

case x$xthreads in
xyes)
	AC_DEFINE(XTHREADS,1,[Whether libX11 is compiled with thread support])
	if test x$mtsafeapi = xyes
	then
	AC_DEFINE(XUSE_MTSAFE_API,1,[Whether libX11 needs to use MT safe API's])
	fi
	;;
*)
	;;
esac

AC_CHECK_LIB(c, pthread_self, [thrstubs="no"], [thrstubs="yes"])
AM_CONDITIONAL(THRSTUBS, test x$thrstubs = xyes)

# XXX incomplete, please fill this in
if test x$xthreads = xyes ; then
    case $host_os in
    linux*|openbsd*|gnu*|k*bsd*-gnu)
        XTHREADLIB=-lpthread ;;
    netbsd*)
	XTHREAD_CFLAGS="-D_POSIX_THREAD_SAFE_FUNCTIONS"
	XTHREADLIB="-lpthread" ;;
    freebsd*)
        XTHREAD_CFLAGS="-D_THREAD_SAFE"
        XTHREADLIB="-pthread" ;;
    dragonfly*)
        XTHREADLIB="-pthread" ;;
    solaris*)
	XTHREAD_CFLAGS="-D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS" ;;
    esac
fi
AC_SUBST(XTHREADLIB)
AC_SUBST(XTHREAD_CFLAGS)

AC_CHECK_FUNC(poll, [AC_DEFINE(USE_POLL, 1, [poll() function is available])], )

#
# Find keysymdef.h
#
AC_MSG_CHECKING([keysym definitions])
KEYSYMDEFDIR=`$PKG_CONFIG --variable=includedir xproto`/X11
FILES="keysymdef.h XF86keysym.h Sunkeysym.h DECkeysym.h HPkeysym.h"
for i in $FILES; do
    if test -f "$KEYSYMDEFDIR/$i"; then
            KEYSYMDEFS="$KEYSYMDEFS $KEYSYMDEFDIR/$i"
    elif test "x$i" = "xkeysymdef.h"; then
	    AC_MSG_ERROR([Cannot find keysymdef.h])
    fi
done
AC_MSG_RESULT([$KEYSYMDEFS])
AC_SUBST(KEYSYMDEFS)

AM_CONDITIONAL(UDC, test xfalse = xtrue)

AC_ARG_ENABLE(xcms,
              AS_HELP_STRING([--disable-xcms],
                [Disable Xlib support for CMS *EXPERIMENTAL*]),
              [XCMS=$enableval],[XCMS=yes])
AM_CONDITIONAL(XCMS, [test x$XCMS = xyes ])
if test x"$XCMS" = "xyes"; then
	AC_DEFINE(XCMS,1,[Include support for XCMS])
fi

AC_ARG_ENABLE(xlocale,
              AS_HELP_STRING([--disable-xlocale],
                [Disable Xlib locale implementation *EXPERIMENTAL*]),
              [XLOCALE=$enableval],[XLOCALE=yes])

AM_CONDITIONAL(XLOCALE, [ test x$XLOCALE = xyes ])
if test x"$XLOCALE" = "xyes"; then
	AC_DEFINE(XLOCALE,1,[support for X Locales])
fi

# This disables XLOCALEDIR.  Set it if you're using BuildLoadableXlibI18n,
# don't have either issetugid() or getresuid(), and you need to protect
# clients that are setgid or setuid to an id other than 0.
AC_MSG_CHECKING([if XLOCALEDIR support should be enabled])
AC_ARG_ENABLE(xlocaledir,
	      AS_HELP_STRING([--enable-xlocaledir],
		[Enable XLOCALEDIR environment variable support]),
	      [ENABLE_XLOCALEDIR=$enableval],[ENABLE_XLOCALEDIR=$XLOCALEDIR_IS_SAFE])
if test "x$ENABLE_XLOCALEDIR" = "xno"; then
	AC_DEFINE(NO_XLOCALEDIR,1,[Disable XLOCALEDIR environment variable])
fi
AC_MSG_RESULT($ENABLE_XLOCALEDIR)

AC_ARG_ENABLE(xf86bigfont,
	      AS_HELP_STRING([--disable-xf86bigfont],
		[Disable XF86BigFont extension support]),
	      [XF86BIGFONT=$enableval],[XF86BIGFONT="yes"])
if test "x$XF86BIGFONT" = "xyes"; then
    PKG_CHECK_MODULES(BIGFONT, [xf86bigfontproto >= 1.2.0],
         AC_DEFINE(XF86BIGFONT,1,[Enable XF86BIGFONT extension]),XF86BIGFONT="no")
fi

AC_ARG_ENABLE(xkb,
              AS_HELP_STRING([--disable-xkb],
                [Disable XKB support *EXPERIMENTAL*]),
              [XKB=$enableval],[XKB=yes])

AM_CONDITIONAL(XKB, [ test x$XKB = xyes ])
if test x"$XKB" = "xyes"; then
   XKBPROTO_REQUIRES="kbproto"
   X11_REQUIRES="${X11_REQUIRES} kbproto inputproto"
   AC_DEFINE(XKB,1,[Use XKB])
else
   XKBPROTO_REQUIRES=""
fi
AC_SUBST(XKBPROTO_REQUIRES)

AC_FUNC_MMAP()
composecache_default=$ac_cv_func_mmap_fixed_mapped
AC_CHECK_FUNC(nl_langinfo, , [composecache_default=no])
AC_ARG_ENABLE(composecache,
              AS_HELP_STRING([--disable-composecache],
                [Disable compose table cache support]),
              [COMPOSECACHE=$enableval],[COMPOSECACHE=$composecache_default])
if test x"$COMPOSECACHE" = "xyes"; then
	AC_DEFINE(COMPOSECACHE,1,[Include compose table cache support])
fi

# Allow checking code with lint, sparse, etc.
XORG_WITH_LINT
XORG_LINT_LIBRARY([X11])

X11_DATADIR="${datadir}/X11"
AX_DEFINE_DIR(X11_DATADIR, X11_DATADIR, [Location of libX11 data])
AC_SUBST(X11_DATADIR)

X11_LIBDIR="${libdir}/X11"
AX_DEFINE_DIR(X11_LIBDIR, X11_LIBDIR, [Location of libX11 library data])
AC_SUBST(X11_LIBDIR)

PKG_CHECK_MODULES(X11, [$X11_REQUIRES])
X11_CFLAGS="$X11_CFLAGS $XTHREAD_CFLAGS"

#
# Yes, it would be nice to put the locale data in
# /usr/share, but the locale stuff includes loadable
# libraries which must be located in the same directory
# as the other locale data, so for now, everything lives
# in ${libdir}
#

X11_LOCALEDATADIR="${X11_DATADIR}/locale"
AX_DEFINE_DIR(XLOCALEDATADIR, X11_LOCALEDATADIR, [Location of libX11 locale data])
AC_SUBST(X11_LOCALEDATADIR)

AC_ARG_WITH(locale-lib-dir,	AS_HELP_STRING([--with-locale-lib-dir=DIR],
 [Directory where locale libraries files are installed (default: $libdir/X11/locale)]),
				[ X11_LOCALELIBDIR="$withval" ],
				[ X11_LOCALELIBDIR="${X11_LIBDIR}/locale" ])
AX_DEFINE_DIR(XLOCALELIBDIR, X11_LOCALELIBDIR, [Location of libX11 locale libraries])
AC_SUBST(X11_LOCALELIBDIR)

X11_LOCALEDIR="${X11_LOCALEDATADIR}"
AX_DEFINE_DIR(XLOCALEDIR, X11_LOCALEDIR, [Location of libX11 locale data])
AC_SUBST(X11_LOCALEDIR)

XKEYSYMDB="${X11_DATADIR}/XKeysymDB"
AX_DEFINE_DIR(XKEYSYMDB, XKEYSYMDB, [Location of keysym database])

XERRORDB="${X11_DATADIR}/XErrorDB"
AX_DEFINE_DIR(XERRORDB, XERRORDB, [Location of error message database])

XORG_CHECK_MALLOC_ZERO

AC_CONFIG_FILES([Makefile
		include/Makefile
		man/Makefile
		man/xkb/Makefile
		src/Makefile
		src/util/Makefile
		src/xcms/Makefile
		src/xlibi18n/Makefile
		modules/Makefile
		modules/im/Makefile
		modules/im/ximcp/Makefile
		modules/lc/Makefile
		modules/lc/def/Makefile
		modules/lc/gen/Makefile
		modules/lc/Utf8/Makefile
		modules/lc/xlocale/Makefile
		modules/om/Makefile
		modules/om/generic/Makefile
		src/xkb/Makefile
		nls/Makefile
		specs/Makefile
		specs/i18n/Makefile
		specs/i18n/framework/Makefile
		specs/i18n/localedb/Makefile
		specs/i18n/trans/Makefile
		specs/libX11/Makefile
		specs/XIM/Makefile
		specs/XKB/Makefile
		x11.pc
		x11-xcb.pc])
AC_OUTPUT

echo ""
echo "X11 will be built with the following settings:"
echo " Loadable i18n module support:            "$XLIB_LOADABLE_I18N
echo " Loadable xcursor library support:        "$XLIB_LOADABLE_XCURSOR
echo " Threading support:                       "$xthreads
echo " Use Threads safe API:                    "$mtsafeapi
echo " Threads stubs in libX11:                 "$thrstubs
echo " XCMS:                                    "$XCMS
echo " Internationalization support:            "$XLOCALE
echo " XF86BigFont support:                     "$XF86BIGFONT
echo " XKB support:                             "$XKB
echo " XLOCALEDIR environment variable support: "$ENABLE_XLOCALEDIR
echo " Compose table cache enabled:             "$COMPOSECACHE
echo " Functional specs building enabled:       "$build_specs
echo ""
