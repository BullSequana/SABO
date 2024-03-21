# Copyright 2021-2024 Bull SAS

#!/bin/bash

PERF_BINARY=

if [ ${SLURM_PROCID} -eq 0 ]
then
	PERF_BINARY="perf record -o perf.data"
fi

${PERF_BINARY} $@
