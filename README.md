
# Introduction #

The SABO library allows an hybrid *OpenMP/MPI* application to reevaluate periodically during the computation the number of OpenMP threads given the different times passed in the different OpenMP sections.

This library is composed of two parts libsabo.so and libsabortintel.so.

# Library usage #

## Library set ##

When you want to enable the SABO library two options are yours :

1. You can explicitely call the SABO entry point function (also known as *sabo_omp_balanced*) in your computational code.

- At compilation time do not forget to include the file `#include "sabo.h"` in your code.
- Since sabo_omp_balanced is declared as a weak symbol, you can either compile with `-lsabo` and `-L<my_SABO_location_path>` options 
or use the `LD_PRELOAD` environment variable pointing to the SABO dynamic libraries (it is then absolutely mandatory to compile the code with the `-fPIC` option).

2. If you do not want to modify your computational code, you can set the SABO_IMPLICIT_BALANCING environment variable to 1. 
Everytime the application quits an OpenMP parallel section, the sabo_omp_balanced function is called automatically and rebalanced the threads at the given rate.

CAUTION ! Whatever the option you choose, the sabo_omp_balanced has to be called the same number of times between all the processes of your application (if this is not the case, your application will freeze).

## Balancing rate set ##

To set up the balancing rate, please set the SABO_STEP_BALANCING value as environment variable.

## Balancing periodicity ##

The SABO_PERIODIC environment variable allows the user to choose if balancing will be done only once or periodically.


## OpenMP thread number settings ##

At the launching of his application, the user should use a pre-defined value of `OMP_NUM_THREADS`.
Nevertheless, due to restrictions of the Intel OpenMP implementation, the value 1 will not work with `SABO_IMPLICIT_BALANCING` enabled. A strictly bigger value must be preferred.


# License #

SABO is an open access software and it is licensed under a limited use MIT-based license. Full text of this license can be found in the LICENSE files.