#############################################################################
# Copyright (c) 2016 Balabit
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

SUBDIRS			=
AM_MAKEFLAGS		= --no-print-directory
AM_YFLAGS		= -Wno-yacc -Wno-other

AM_TESTS_ENVIRONMENT	= top_srcdir="$(top_srcdir)"

ACLOCAL_AMFLAGS		= -I m4 --install

EXTRA_DIST		= ${BUILT_SOURCES} VERSION NEWS.md autogen.sh \
			  syslog-ng.spec syslog-ng.spec.in \
			  dist.conf dist.conf.in syslog-ng.pc.in \
			  CONTRIBUTING.md

pkgconfigdir		= $(libdir)/pkgconfig
pkgconfig_DATA		= syslog-ng.pc

AM_CPPFLAGS		= -I$(top_srcdir)/lib -I$(top_srcdir)/modules -I$(top_builddir)/lib -I$(top_builddir)/modules

AM_CFLAGS =
if ENABLE_EXTRA_WARNINGS
AM_CFLAGS += \
	-Wshadow

endif

TEST_CFLAGS		= -I$(top_srcdir)/libtest $(AM_CFLAGS)

TEST_LDADD		= $(top_builddir)/libtest/libsyslog-ng-test.a \
			  $(top_builddir)/lib/libsyslog-ng.la \
			  $(TOOL_DEPS_LIBS)

if ENABLE_CRITERION
TEST_CFLAGS		+= $(CRITERION_CFLAGS)
TEST_LDADD		+= $(CRITERION_LIBS)
endif

test_ldflags		= -no-install

PREOPEN_SYSLOGFORMAT	= -dlpreopen ${top_builddir}/modules/syslogformat/libsyslogformat.la
PREOPEN_BASICFUNCS	= -dlpreopen ${top_builddir}/modules/basicfuncs/libbasicfuncs.la
PREOPEN_CORE		= $(PREOPEN_SYSLOGFORMAT) $(PREOPEN_BASICFUNCS)

syslog_ng_tools		= ${top_srcdir}/lib

export top_srcdir
export MAKE

CLEAN_SUBDIRS		=

clean-local: ${CLEAN_HOOKS}
	${AM_v_at}for dir in ${CLEAN_SUBDIRS}; do \
		[ -f $${dir}/Makefile ] && ${MAKE} -C $${dir} clean || true; \
	done

distclean-local: distclean-ax-prefix-config-h
distclean-ax-prefix-config-h:
	rm -f _configs.sed syslog-ng-config.h

local-check: subdir_tests=$(foreach ts,${${check_subdir}_test_subdirs},${${ts}_tests_TESTS})
local-check: current_tests=$(foreach subdir,${check_subdir} ${check_subdir}_tests,${${subdir}_TESTS})
local-check:
	${AM_v_at}${MAKE} check check_PROGRAMS="${current_tests} ${subdir_tests}" \
				TESTS="${current_tests} ${subdir_tests}"

${check_PROGRAMS}: LDFLAGS+="${test_ldflags}"

noinst_LIBRARIES	=
noinst_DATA		=
noinst_LTLIBRARIES	=
lib_LTLIBRARIES		=
pkginclude_HEADERS	= syslog-ng-config.h
module_LTLIBRARIES	=
BUILT_SOURCES		=
CLEANFILES 		= $(BUILT_SOURCES)
check_PROGRAMS		=
check_SCRIPTS		=
TESTS			= $(check_PROGRAMS) $(check_SCRIPTS)
bin_SCRIPTS		=
bin_PROGRAMS		=
sbin_PROGRAMS		=
libexec_PROGRAMS	=
man_MANS		=

INSTALL_EXEC_HOOKS	=
UNINSTALL_HOOKS		=
CLEAN_HOOKS =

AUTOMAKE_OPTIONS	= subdir-objects parallel-tests

${top_srcdir}/ChangeLog:
	(cd $(top_srcdir); git log) > $@

install-exec-hook: ${INSTALL_EXEC_HOOKS}
uninstall-hook: ${UNINSTALL_HOOKS}

populate-makefiles:
	$(top_srcdir)/Mk/populate-makefiles.sh

style-check:
	$(top_srcdir)/scripts/style-checker.sh check $(top_srcdir)

style-format:
	$(top_srcdir)/scripts/style-checker.sh format $(top_srcdir)

help:
	@echo "Welcome to the syslog-ng build system!"
	@echo
	@echo "All of the standard targets are available:"
	@echo "  all, check, install, dist, distcheck, and clean"
	@echo
	@echo "Apart from these, the build system supports various other,"
	@echo "special targets:"
	@echo
	@echo " modules  : Builds all syslog-ng modules."
	@echo " syslog-ng: Build syslog-ng itself, only."
	@echo " populate-makefiles : populate build directory with stub Makefiles"
	@echo
	@echo "One can also build individual modules (and their dependencies),"
	@echo "using any of the following shortcuts:"
	@echo
	@echo "" ${SYSLOG_NG_MODULES} | sed -e 's#\(.\{,72\}\) #\1\n #g'

.PHONY: help populate-makefiles

include Mk/lex-rules.am
include Mk/Makefile.am
include libtest/Makefile.am
include lib/Makefile.am
include modules/Makefile.am
include syslog-ng/Makefile.am
include syslog-ng-ctl/Makefile.am
include scripts/Makefile.am
include tests/Makefile.am
include doc/Makefile.am
include contrib/Makefile.am
include scl/Makefile.am
include debian/Makefile.am
include tgz2build/Makefile.am

TEST_EXTENSIONS = .sh
@VALGRIND_CHECK_RULES@

VALGRIND_SUPPRESSIONS_FILES = $(top_srcdir)/syslog-ng.supp
EXTRA_DIST += syslog-ng.supp
