# Copyright 2021-2024 Bull SAS

#!/bin/sh

USE_VALGRIND=${1}
BINARIES_TESTS_STANDALONE=${2}
BINARIES_TESTS_MPI=${3}

MYDIR=$(dirname $(readlink -f $0))

export OMP_NUM_THREADS=2
export SABO_PERIODIC=1
export SABO_NUM_STEPS_EXCHANGED=1
export SABO_NO_REBALANCE=1
export SABO_HWLOC_FILENAME=${MYDIR}/common/topomonosocket.xml 

export OMPI_MCA_oob_tcp_if_include=lo,eth0

ERROR=0
VALGRIND=""

BROWN='\033[0;33m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# set a signal handler to kill every child and return error
trap "pkill -P $$ ; exit 1" SIGINT SIGTERM

function test_standalone() {
    for TEST in ${BINARIES_TESTS_STANDALONE} ; do
	printf "${GREEN}start standalone test ${TEST}${NC}\n"
	#${VALGRIND} ${MYDIR}/../${TEST} 2>/dev/null
	${VALGRIND} ${MYDIR}/../${TEST} 
	if [ $? -ne 0 ] ; then
	    ERROR=1
	fi
    done
}

function test_mpi() {
    for TEST in ${BINARIES_TESTS_MPI} ; do
	printf "${GREEN}start standalone test ${TEST}${NC}\n"
	#${MYDIR}/../${TEST} 2>/dev/null
	${MYDIR}/../${TEST}
	if [ $? -ne 0 ] ; then
	    ERROR=1
	fi
    done
}

if [ "${USE_VALGRIND}" == "yes" ] ; then
    VALGRIND="valgrind --error-exitcode=1 --errors-for-leak-kinds=all --leak-check=full --quiet --show-leak-kinds=all --sim-hints=lax-ioctls --suppressions=${MYDIR}/tests.supp"
else
    VALGRIND=""
fi

printf "${BROWN}use valgrind     : %s${NC}\n" "${USE_VALGRIND}"
if [ ! -z "${VALGRIND}" ] ; then
    printf "${BROWN}valgrind command : %s${NC}\n" "${VALGRIND}"
fi

test_standalone
test_mpi

if [ ${ERROR} -ne 0 ] ; then
    exit 1
fi
exit 0
