# Copyright 2021-2024 Bull SAS

#!/bin/bash

SABO_ROOT=/path/to/sabo/sabo

export LD_PRELOAD=$SABO_ROOT/libsabo.so

#Run user command
$@
