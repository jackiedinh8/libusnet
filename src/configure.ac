AC_PREREQ([2.69])
AC_INIT([libusnet], [0.1.0], [jackiedinh8@gmail.com],
        [libusnet], [https://github.com/jackiedinh8/libusnet])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror])
LT_PREREQ([2.2])
LT_INIT([dlopen])

AM_PROG_AR
#AC_PROG_RANLIB



CFLAGS="-g"

#AC_CONFIG_SRCDIR([config.h.in])
#AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_CC_C_O
#AC_LANG(C)
#AC_PROG_AWK
#AC_PROG_CXX
#AC_PROG_CPP
#AC_PROG_LN_S
#AC_PROG_MAKE_SET
#AC_PROG_RANLIB

# Checks for libraries.
# FIXME: Replace `main' with a function in `-levent':
#AC_CHECK_LIB([event], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero dup2 gethostbyname getpagesize gettimeofday inet_ntoa memset mkfifo select socket sqrt strerror])

AC_CONFIG_FILES([Makefile])


AC_OUTPUT
