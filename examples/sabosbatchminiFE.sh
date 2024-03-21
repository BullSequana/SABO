# Copyright 2021-2024 Bull SAS

#!/bin/sh
#SBATCH -p partition
#SBATCH --ntasks-per-node=8
#SBATCH -N 2
#SBATCH --exclusive
#SBATCH -o slurm-sbatch-sabo-miniFE.%j.out
#SBATCH -e slurm-sbatch-sabo-miniFE.%j.err

MINIFE_ROOT=/path/to/sabo/miniFE/openmp/src/
SABO_ROOT=/path/to/sabo/sabo

source /path/to/compilervars.sh intel64
source /path/to/openmpi/mpivars.sh intel

export OMP_NUM_THREADS=16

mpirun ${SABO_ROOT}/scripts/sabolauncher.sh ${MINIFE_ROOT}/miniFE.x
