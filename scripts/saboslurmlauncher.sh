# Copyright 2021-2024 Bull SAS

#!/bin/bash

MYDIR=$(dirname $(readlink -f $0))

if [ -z ${SABO_SHARED_FILENAME} ] ; then
    export SABO_SHARED_FILENAME="/tmp/sabo_shared_node_sync"
fi

# World informations
export SABO_WORLD_TASK_ID=${SLURM_PROCID}
export SABO_WORLD_NUM_TASKS=${SLURM_NPROCS}

# Node informations
${MYDIR}/../tools/slurm_variables_parser
export SABO_NODE_NUM_TASKS=$?
export SABO_NODE_TASK_ID=${SLURM_LOCALID}

#Run user command
$@
