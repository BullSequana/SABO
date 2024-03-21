# Copyright 2024 Bull SAS

MPICC			= mpicc

CC 			= ${MPICC}

DWARF_VER		= 4

CFLAGS_COMMON		= -g -grecord-gcc-switches -gdwarf-${DWARF_VER} -fPIC

OPENMP_FLAGS = -fopenmp
OPENMP_LDFLAGS =
MPI_FLAGS = -DSABO_USE_MPI

CFLAGS 		= ${CFLAGS_COMMON}
CFLAGS		+= ${OPENMP_FLAGS} ${OPENMP_LDFLAGS}
CFLAGS 		+= ${MPI_FLAGS}

INTEL_SPECIFIC_CFLAGS	= -DSABO_RTINTEL
INTEL_COVERAGE_CFLAGS 	= -prof-gen=srcpos

GNU_COVERAGE_CFLAGS 	= --coverage
GNU_SPECIFIC_FLAGS 	= \
			-fbounds-check \
			-Waggregate-return \
			-Wbad-function-cast \
			-Wcast-align \
			-Wlogical-op \
			-Wnested-externs \
			-Wpadded \
			-Wredundant-decls \
			-Wtrampolines

ifeq (${OMPI_CC}, icc)
CFLAGS += ${INTEL_SPECIFIC_CFLAGS}
COVERAGE_CFLAGS = ${INTEL_COVERAGE_CFLAGS}
else # ifeq(${OMPI_CC}, cc)
CFLAGS += ${GNU_SPECIFIC_CFLAGS}
COVERAGE_CFLAGS = ${GNU_COVERAGE_CFLAGS}
endif # ifeq(${OMPI_CC}, cc)

INCLUDE			= \
			-Itests \
			-Icommon \
			-Icore \
			-Iinclude \
			-Imodules \
			-Iintel \
			-Iompt

ifneq (${V},1)
V			= 0
MAKEFLAGS		+= --silent
endif

ifneq (${MODE},release)
CFLAGS			+= \
			-g \
			-O0 \
			-fstack-protector-strong \
			${COVERAGE_CFLAGS}
override MODE		= debug

# by default turn valgrind 'on'

else # ifneq (${MODE},release)
CFLAGS			+= \
			-O2 \
			-DNDEBUG

# by default valgrind is 'off'

endif # ifneq (${MODE},release)

CFLAGS			+= \
			-D_GNU_SOURCE

CFLAGS			+= \
			-pedantic \
			-Wall \
			-Wcast-qual \
			-Wconversion \
			-Wextra \
			-Wfloat-equal \
			-Wformat-security \
			-Wformat=2 \
			-Wimplicit \
			-Wmissing-declarations \
			-Wmissing-prototypes \
			-Wpointer-arith \
			-Wshadow \
			-Wstrict-prototypes \
			-Wundef \
			-Wuninitialized \
			-Wunused \
			-Wwrite-strings

CFLAGS			+= ${INTEL_FLAGS}

# log.h use variadic macros which only appear in C99. However, turning on
# C99 could allow C langages features that may impact performances.
# For now, only allow C89 + variadic macros.
CFLAGS			+= \
			-Wno-variadic-macros

ifeq (${WERROR}, yes)
CFLAGS			+= \
			-Werror
endif

CFLAGS			+= ${INCLUDE}

DFLAGS			= ${CFLAGS} -MM -MP

LDFLAGS			= -lhwloc

CPPCHECK		= cppcheck
CPPCHECKFLAGS		= --enable=all --force --inconclusive --inline-suppr \
			--std=c89 --suppress=missingIncludeSystem \
			--suppress=unusedFunction --library=gnu --quiet \
			$$(echo ${INCLUDE} | sed 's/-isystem/-I/g') \
			--check-config

BUILDTAG		= ${MODE}
$(info Target MODE=${MODE})

SABO_LIBNAME	= libsabo.${MODE}

######################################################## libcommon.a ##########
SABOCOMMON_LIB		= common/libsabocommon.${BUILDTAG}.a
LDFLAGS_SABOCOMMON	= -Lcommon -lsabocommon.${BUILDTAG}
SABOCOMMON_CFILES	= \
			common/binding.c \
			common/comm.c \
			common/decision_tree.c \
			common/env.c \
			common/hwloc_binding.c \
			common/topo.c \
			common/list.c \
			common/log.c \
			common/sys.c
SABOCOMMON_DFILES	= ${SABOCOMMON_CFILES:%.c=%.${BUILDTAG}.d}
SABOCOMMON_OFILES	= ${SABOCOMMON_CFILES:%.c=%.${BUILDTAG}.o}

SABOBASIC_CFILES       = \
			common/list.c \
			common/log.c \
			common/sys.c
SABOBASIC_OFILES       = ${SABOBASIC_CFILES:%.c=%.${BUILDTAG}.o}

################################################# libsabomodulempi.so ##########
SABOMODULEMPI_LIB	= modules/libsabomodulempi.${BUILDTAG}.so
SABOMODULEMPI_CFILES	= modules/module_mpi.c
SABOMODULEMPI_DFILES	= ${SABOMODULEMPI_CFILES:%.c=%.${BUILDTAG}.d}
SABOMODULEMPI_OFILES	= ${SABOMODULEMPI_CFILES:%.c=%.${BUILDTAG}.o}

################################################# libsabomodulempi.so ##########
SABOMODULESHM_LIB	= modules/libsabomoduleshm.${BUILDTAG}.so
#SABOMODULESHM_CFILES	= modules/module_shm.c
SABOMODULESHM_DFILES	= ${SABOMODULESHM_CFILES:%.c=%.${BUILDTAG}.d}
SABOMODULESHM_OFILES	= ${SABOMODULESHM_CFILES:%.c=%.${BUILDTAG}.o}

######################################################### libsabo.so ##########
LDFLAGS_SABO		= -L. -lsabo
TESTS_LDFLAGS_SABO	= ${LDFLAGS_SABO} -Wl,-rpath=.
SABO_CFILES		= \
			core/sabo.c \
			core/sabo_omp.c

#Use dlopen with libsabomodule{shm,mpi}.so
SABO_CFILES		+= \
			modules/module_mpi.c \
			modules/module_shm.c

SABO_DFILES		= ${SABO_CFILES:%.c=%.${BUILDTAG}.d}
SABO_OFILES		= ${SABO_CFILES:%.c=%.${BUILDTAG}.o}

############################################################## intel ##########
SABORTINTEL_LDFLAGS	= -L. -lsabortintel -Wl,-rpath=.
SABORTINTEL_LIBNAME	= libsabortintel.${BUILDTAG}
SABORTINTEL_CFILES	= \
			ompt/sabo_ompt.c \
			intel/intel_binding.c \
			intel/sabo_intel_omp.c

SABORTINTEL_DFILES	= ${SABORTINTEL_CFILES:%.c=%.${BUILDTAG}.d}
SABORTINTEL_OFILES	= ${SABORTINTEL_CFILES:%.c=%.${BUILDTAG}.o}

####################################################### format_check ##########
FORMAT_CHECK_BIN	= tools/format_check.${BUILDTAG}
FORMAT_CHECK_CFILES	= tools/format_check.c
FORMAT_CHECK_DFILES	= ${FORMAT_CHECK_CFILES:%.c=%.${BUILDTAG}.d}
FORMAT_CHECK_OFILES	= ${FORMAT_CHECK_CFILES:%.c=%.${BUILDTAG}.o}

####################################################### slurm_parser ##########
SLURM_PARSER_BIN	= tools/slurm_variables_parser.${BUILDTAG}
SLURM_PARSER_CFILES     = tools/slurm_variables_parser.c
SLURM_PARSER_DFILES	= ${SLURM_PARSER_CFILES:%.c=%.${BUILDTAG}.d}
SLURM_PARSER_OFILES	= ${SLURM_PARSER_CFILES:%.c=%.${BUILDTAG}.o}

################################################# test_decision_tree ##########
TEST_DECISION_TREE_BIN		= tests/common/test_decision_tree.${BUILDTAG}
TEST_DECISION_TREE_CFILES	= tests/common/test_decision_tree.c
TEST_DECISION_TREE_DFILES	= ${TEST_DECISION_TREE_CFILES:%.c=%.${BUILDTAG}.d}
TEST_DECISION_TREE_OFILES	= ${TEST_DECISION_TREE_CFILES:%.c=%.${BUILDTAG}.o}

########################################################## test_comm ##########
TEST_COMM_BIN			= tests/common/test_comm.${BUILDTAG}
TEST_COMM_CFILES		= tests/common/test_comm.c
TEST_COMM_DFILES		= ${TEST_COMM_CFILES:%.c=%.${BUILDTAG}.d}
TEST_COMM_OFILES		= ${TEST_COMM_CFILES:%.c=%.${BUILDTAG}.o}

########################################################## test_comm ##########
TEST_ENV_BIN			= tests/common/test_env.${BUILDTAG}
TEST_ENV_CFILES			= tests/common/test_env.c
TEST_ENV_DFILES			= ${TEST_ENV_CFILES:%.c=%.${BUILDTAG}.d}
TEST_ENV_OFILES			= ${TEST_ENV_CFILES:%.c=%.${BUILDTAG}.o}

################################################ test_ompt_callbacks ##########
TEST_OMPT_CALLBACKS_BIN		= tests/ompt/test_ompt_callbacks.${BUILDTAG}
TEST_OMPT_CALLBACKS_CFILES	= tests/ompt/test_ompt_callbacks.c
TEST_OMPT_CALLBACKS_DFILES	= ${TEST_OMPT_CALLBACKS_CFILES:%.c=%.${BUILDTAG}.d}
TEST_OMPT_CALLBACKS_OFILES	= ${TEST_OMPT_CALLBACKS_CFILES:%.c=%.${BUILDTAG}.o}

########################################################## test_sabo ##########
TEST_SABO_BIN			= tests/core/test_sabo.${BUILDTAG}
TEST_SABO_CFILES		= tests/core/test_sabo.c
TEST_SABO_DFILES		= ${TEST_SABO_CFILES:%.c=%.${BUILDTAG}.d}
TEST_SABO_OFILES		= ${TEST_SABO_CFILES:%.c=%.${BUILDTAG}.o}

########################################################## test_comm ##########
TEST_TOPO_BIN			= tests/common/test_topo.${BUILDTAG}
TEST_TOPO_CFILES		= tests/common/test_topo.c
TEST_TOPO_DFILES		= ${TEST_TOPO_CFILES:%.c=%.${BUILDTAG}.d}
TEST_TOPO_OFILES		= ${TEST_TOPO_CFILES:%.c=%.${BUILDTAG}.o}

########################################################## test_sabo ##########
TEST_VALIDATION_BIN			= tests/validation/test_sabo_omp_balanced.${BUILDTAG}
TEST_VALIDATION_CFILES		= tests/validation/test_sabo_omp_balanced.c
TEST_VALIDATION_DFILES		= ${TEST_VALIDATION_CFILES:%.c=%.${BUILDTAG}.d}
TEST_VALIDATION_OFILES		= ${TEST_VALIDATION_CFILES:%.c=%.${BUILDTAG}.o}

BINARIES_TESTS	= \
		${TEST_ENV_BIN} \
		${TEST_DECISION_TREE_BIN} \
		${TEST_OMPT_CALLBACKS_BIN} \
		${TEST_TOPO_BIN}

BINARIES_MPI_TESTS = \
		${TEST_COMM_BIN} \
		${TEST_VALIDATION_BIN} \
		${TEST_SABO_BIN}

BINARIES	= \
		${SLURM_PARSER_BIN} \
		${BINARIES_TESTS} \
		${BINARIES_MPI_TESTS}

LIBRARIES	= \
		${SABOMODULEMPI_LIB} \
		${SABOMODULESHM_LIB}

CFILES		= \
		${SABO_CFILES} \
		${SABOCOMMON_CFILES} \
		${SABOMODULEMPI_CFILES} \
		${SABOMODULESHM_CFILES} \
		${SLURM_PARSER_CFILES} \
		${TEST_COMM_CFILES} \
		${TEST_ENV_CFILES} \
		${TEST_DECISION_TREE_CFILES} \
		${TEST_OMPT_CALLBACKS_CFILES} \
		${TEST_SABO_CFILES} \
		${TEST_VALIDATION_CFILES} \
		${TEST_TOPO_CFILES}

DFILES		= \
		${SABO_DFILES} \
		${SABOCOMMON_DFILES} \
		${SABOMODULEMPI_DFILES} \
		${SABOMODULESHM_DFILES} \
		${SABORTINTEL_DFILES} \
		${SLURM_PARSER_DFILES} \
		${TEST_COMM_DFILES} \
		${TEST_ENV_DFILES} \
		${TEST_DECISION_TREE_DFILES} \
		${TEST_OMPT_CALLBACKS_DFILES} \
		${TEST_SABO_DFILES} \
		${TEST_VALIDATION_DFILES} \
		${TEST_TOPO_DFILES}

OFILES		= \
		${SABO_OFILES} \
		${SABOCOMMON_OFILES} \
		${SABOMODULEMPI_OFILES} \
		${SABOMODULESHM_OFILES} \
		${SABORTINTEL_OFILES} \
		${SLURM_PARSER_OFILES} \
		${TEST_COMM_OFILES} \
		${TEST_ENV_OFILES} \
		${TEST_DECISION_TREE_OFILES} \
		${TEST_OMPT_CALLBACKS_DFILES}	\
		${TEST_SABO_OFILES} \
		${TEST_VALIDATION_OFILES} \
		${TEST_TOPO_OFILES}

.PRECIOUS		: ${DFILES}

.PHONY			: all
all			: ${SABO_LIBNAME:.${BUILDTAG}=} ${LIBRARIES} ${BINARIES:.${BUILDTAG}=}

.PHONY			: build_check
build_check		:
	if [ ${V} -ne 1 ] ; then echo Checking build $@ ; fi
	TARGET=(${BUILDTARGET}) ; \
	TARGET_COUNT=$${#TARGET[@]} ; \
	function expand_target { \
	    local BASE="$$1" ; \
	    local INDEX="$$2" ; \
	    if [ "$${INDEX}" -lt "$${TARGET_COUNT}" ] ; then \
		expand_target "$${BASE} $${TARGET[$${INDEX}]}=no" \
			"$$(($${INDEX}+1))" ; \
		expand_target "$${BASE} $${TARGET[$${INDEX}]}=yes" \
			"$$(($${INDEX}+1))" ; \
	    else \
		echo "$${BASE}" ; \
	    fi \
	} ; \
	for BUILDTAG in debug release ; do \
	    expand_target "" 0 | while read VAR ; do \
		${MAKE} BUILDTAG=$${BUILDTAG} $${VAR} || exit 1; \
	    done || exit 1; \
	done

.PHONY			: clean
clean			:
	if [ ${V} -ne 1 ] ; then echo Cleaning ; fi
	${RM}	${BINARIES:.${BUILDTAG}=} \
		${BINARIES:.${BUILDTAG}=.debug*} \
		${BINARIES:.${BUILDTAG}=.release*} \
		${SABO_LIBNAME:.${BUILDTAG}=.so} \
		${SABO_LIBNAME:.${BUILDTAG}=.debug.so} \
		${SABO_LIBNAME:.${BUILDTAG}=.release.so} \
		${SABORTINTEL_LIBNAME:.${BUILDTAG}=.so} \
		${SABORTINTEL_LIBNAME:.${BUILDTAG}=.debug.so} \
		${SABORTINTEL_LIBNAME:.${BUILDTAG}=.debug.so} \
		${LIBRARIES:.${BUILDTAG}.a=.a} \
		${LIBRARIES:.${BUILDTAG}.a=.debug*.a} \
		${LIBRARIES:.${BUILDTAG}.a=.release*.a} \
		${LIBRARIES:.${BUILDTAG}.so=.so} \
		${LIBRARIES:.${BUILDTAG}.so=.debug*.so} \
		${LIBRARIES:.${BUILDTAG}.so=.release*.so} \
		${OFILES:.${BUILDTAG}.o=.o} \
		${OFILES:.${BUILDTAG}.o=.debug*.o} \
		${OFILES:.${BUILDTAG}.o=.debug.gcda} \
		${OFILES:.${BUILDTAG}.o=.debug.gcno} \
		${OFILES:.${BUILDTAG}.o=.debug*.optrpt} \
		${OFILES:.${BUILDTAG}.o=.release*.o} \
		${OFILES:.${BUILDTAG}.o=.release.gcda} \
		${OFILES:.${BUILDTAG}.o=.release.gcno} \
		${OFILES:.${BUILDTAG}.o=.release*.optrpt} \
		pgopti.dpi \
		pgopti.dpi.lock \
		pgopti.spi \
		pgopti.spl \
		*.dyn \
		codecov.xml

.PHONY			: cleanall
cleanall		: clean
	if [ ${V} -ne 1 ] ; then echo Wide cleaning ; fi
	${RM}	${DFILES:.${BUILDTAG}.d=.d} \
		${DFILES:.${BUILDTAG}.d=.debug*.d} \
		${DFILES:.${BUILDTAG}.d=.release*.d}

.PHONY			: check
check			:
	if [ ${V} -ne 1 ] ; then echo Checking code $@ ; fi
	${CPPCHECK} ${CPPCHECKFLAGS} ${CFILES} ${FORMAT_CHECK_CFILES}

.PHONY			: dep
dep			: ${DFILES}

.PHONY			: ${SABO_LIBNAME:.${BUILDTAG}=}
${SABO_LIBNAME:.${BUILDTAG}=}	: ${SABO_LIBNAME}
	(cd $$(dirname $@) && ln -sf $$(basename $<.so) $$(basename $@.so))

${SABO_LIBNAME}	: ${SABOCOMMON_OFILES} ${SABO_OFILES} ${SABORTINTEL_LIBNAME:.${BUILDTAG}=}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -shared -o $@.so ${LDFLAGS} ${SABOCOMMON_OFILES}\
		${SABO_OFILES} ${SABORTINTEL_LDFLAGS}

.PHONY			: ${SABORTINTEL_LIBNAME:.${BUILDTAG}=}
${SABORTINTEL_LIBNAME:.${BUILDTAG}=}	: ${SABORTINTEL_LIBNAME}
	(cd $$(dirname $@) && ln -sf $$(basename $<.so) $$(basename $@.so))

${SABORTINTEL_LIBNAME}	: ${SABORTINTEL_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -shared -o $@.so ${LDFLAGS} ${SABORTINTEL_OFILES}

${SABOMODULEMPI_LIB}	:  ${SABOMODULEMPI_OFILES}
	if [ ${V} -ne 1 ] ; then echo Build $@ ; fi
	${CC} ${CFLAGS} -shared -o $@ ${LDFLAGS} ${SABOMODULEMPI_OFILES}

${SABOMODULESHM_LIB}	:  ${SABOMODULESHM_OFILES}
	if [ ${V} -ne 1 ] ; then echo Build $@ ; fi
	${CC} ${CFLAGS} -shared -o $@ ${LDFLAGS} ${SABOMODULESHM_OFILES}

.PHONY			: format_check

.PHONY			: format_check
format_check		: ${FORMAT_CHECK_BIN:.${BUILDTAG}=}
	if [ ${V} -ne 1 ] ; then echo Format check ; fi
	tools/format_check ${CFILES} */*.h Makefile

.PHONY			: ${FORMAT_CHECK_BIN:.${BUILDTAG}=}
${FORMAT_CHECK_BIN:.${BUILDTAG}=}	: ${FORMAT_CHECK_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${FORMAT_CHECK_BIN}	: ${FORMAT_CHECK_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${FORMAT_CHECK_OFILES}

.PHONY			: header_check
header_check		:
	error=0 ; \
	for HEADER in $$(git ls-files common/*.h core/*.h \
		include/*.h teste/*.h)  ; do \
		echo Checking $${HEADER} ; \
		( echo "#include \"$${HEADER}\"" ; \
		  echo "#include <stdlib.h>" ; \
		  echo "int main(void) { return EXIT_SUCCESS; }" ) | \
			${CC} ${CFLAGS} -o /dev/null -x c - || error=1; \
	done ; \
	if [ $$error -ne 0 ] ; then exit 1 ; fi

.PHONY			: headers_check
headers_check		:
	TARGET=(${BUILDTARGET}) ; \
	TARGET_COUNT=$${#TARGET[@]} ; \
	function expand_target { \
	    local BASE="$$1" ; \
	    local INDEX="$$2" ; \
	    if [ "$${INDEX}" -lt "$${TARGET_COUNT}" ] ; then \
		expand_target "$${BASE} $${TARGET[$${INDEX}]}=no" \
			"$$(($${INDEX}+1))" ; \
		expand_target "$${BASE} $${TARGET[$${INDEX}]}=yes" \
			"$$(($${INDEX}+1))" ; \
	    else \
		echo "$${BASE}" ; \
	    fi \
	} ; \
	for MODE in debug release ; do \
	    expand_target "" 0 | while read VAR ; do \
		${MAKE} MODE=$${MODE} $${VAR} header_check || exit 1; \
	    done || exit 1; \
	done

.PHONY			: tests
tests			: ${BINARIES:.${MODE}=}
	tests/tests.sh "no" "${BINARIES_TESTS} ${BINARIES_MPI_TESTS}"

.PHONY			: testsv
testsv			: ${BINARIES:.${MODE}=}
	tests/tests.sh "yes" "${BINARIES_TESTS} ${BINARIES_MPI_TESTS}"

.PHONY			: tests_check
tests_check		:
	TARGET=(${BUILDTARGET}) ; \
	TARGET_COUNT=$${#TARGET[@]} ; \
	function expand_target { \
	    local BASE="$$1" ; \
	    local INDEX="$$2" ; \
	    if [ "$${INDEX}" -lt "$${TARGET_COUNT}" ] ; then \
		expand_target "$${BASE} $${TARGET[$${INDEX}]}=no" \
			"$$(($${INDEX}+1))" ; \
		expand_target "$${BASE} $${TARGET[$${INDEX}]}=yes" \
			"$$(($${INDEX}+1))" ; \
	    else \
		echo "$${BASE}" ; \
	    fi \
	} ; \
	for MODE in debug release ; do \
	    expand_target "" 0 | while read VAR ; do \
		${MAKE} MODE=$${MODE} $${VAR} tests || exit 1; \
	    done || exit 1; \
	done

.PHONY			: ${SLURM_PARSER_BIN:.${BUILDTAG}=}
${SLURM_PARSER_BIN:.${BUILDTAG}=}	: ${SLURM_PARSER_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${SLURM_PARSER_BIN}	: ${SABOBASIC_OFILES} ${SLURM_PARSER_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${SLURM_PARSER_OFILES} ${SABOBASIC_OFILES}

.PHONY			: ${TEST_COMM_BIN:.${BUILDTAG}=}
${TEST_COMM_BIN:.${BUILDTAG}=}	: ${TEST_COMM_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_COMM_BIN}	: ${SABO_LIBNAME:.${BUILDTAG}=} ${TEST_COMM_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_COMM_OFILES} ${TESTS_LDFLAGS_SABO}

.PHONY			: ${TEST_ENV_BIN:.${BUILDTAG}=}
${TEST_ENV_BIN:.${BUILDTAG}=}	: ${TEST_ENV_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_ENV_BIN}		: ${SABO_LIBNAME:.${BUILDTAG}=} ${TEST_ENV_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_ENV_OFILES} ${TESTS_LDFLAGS_SABO}

.PHONY			: ${TEST_DECISION_TREE_BIN:.${BUILDTAG}=}
${TEST_DECISION_TREE_BIN:.${BUILDTAG}=}	: ${TEST_DECISION_TREE_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_DECISION_TREE_BIN}	: ${SABO_LIBNAME:.${BUILDTAG}=} ${TEST_DECISION_TREE_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_DECISION_TREE_OFILES} ${TESTS_LDFLAGS_SABO}

.PHONY			: ${TEST_OMPT_CALLBACKS_BIN:.${BUILDTAG}=}
${TEST_OMPT_CALLBACKS_BIN:.${BUILDTAG}=}	: ${TEST_OMPT_CALLBACKS_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_OMPT_CALLBACKS_BIN}	: ${SABO_LIBNAME:.${MODE}=} ${TEST_OMPT_CALLBACKS_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_OMPT_CALLBACKS_OFILES} ${TESTS_LDFLAGS_SABO}

.PHONY			: ${TEST_TOPO_BIN:.${BUILDTAG}=}
${TEST_TOPO_BIN:.${BUILDTAG}=}	: ${TEST_TOPO_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_TOPO_BIN}	: ${SABO_LIBNAME:.${BUILDTAG}=} ${TEST_TOPO_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_TOPO_OFILES} ${TESTS_LDFLAGS_SABO}

.PHONY			: ${TEST_SABO_BIN:.${BUILDTAG}=}
${TEST_SABO_BIN:.${BUILDTAG}=}	: ${TEST_SABO_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_SABO_BIN}	: ${SABO_LIBNAME:.${BUILDTAG}=} ${TEST_SABO_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_SABO_OFILES} ${TESTS_LDFLAGS_SABO}

.PHONY			: ${TEST_VALIDATION_BIN:.${BUILDTAG}=}
${TEST_VALIDATION_BIN:.${BUILDTAG}=}	: ${TEST_VALIDATION_BIN}
	(cd $$(dirname $@) && ln -sf $$(basename $<) $$(basename $@))

${TEST_VALIDATION_BIN}	: ${SABO_LIBNAME:.${BUILDTAG}=} ${TEST_VALIDATION_OFILES}
	if [ ${V} -ne 1 ] ; then echo Link $@ ; fi
	${CC} ${CFLAGS} -o $@ ${TEST_VALIDATION_OFILES}

%.${BUILDTAG}.d		: %.c Makefile
	if [ ${V} -ne 1 ] ; then echo Gen $@ ; fi
	${CC} ${DFLAGS} $< -MF $@ -MT ${@:.d=.o}

%.${BUILDTAG}.o		: %.c %.${BUILDTAG}.d Makefile
	if [ ${V} -ne 1 ] ; then echo Build $@ ; fi
	${CC} ${CFLAGS} -c -o $@ $<

sinclude $(wildcard ${DFILES})
