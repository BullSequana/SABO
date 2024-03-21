# Copyright 2021-2024 Bull SAS

#!/bin/bash

GDBSERVERPORT=$(expr 2222 + ${SLURM_LOCALID})
GDBSERVERHOST=$(echo $(hostname) | sed -e "s/\.bullx//g")

if [ ${SLURM_PROCID} -eq 0 ]
then
	echo "Client cmdline : gdb $@"
fi

echo "Launch new gdb server @ target remote ${GDBSERVERHOST}:${GDBSERVERPORT}"
gdbserver ${GDBSERVERHOST}:${GDBSERVERPORT} $@
