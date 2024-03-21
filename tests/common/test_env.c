/*
 * Copyright 2021-2024 Bull SAS
 */

#include <stdio.h>
#include <stdlib.h>

#include "env.h"
#include "sys.h"

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    env_variables_init();
    env_variables_fini();

    printf("all done\n");
    return EXIT_SUCCESS;
}
