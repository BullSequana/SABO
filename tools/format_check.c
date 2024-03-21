/*
 * Copyright 2021-2024 Bull SAS
 */

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>

static int check(const char *filename)
{
    size_t buffer_size;
    char *buffer, *ptr;
    FILE *fd;
    int rc;
    int line;
    int empty_line;
    int empty_line_count;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        fprintf(stderr, "error opening file '%s'\n", filename);
        return -1;
    }

    buffer_size = 65536;
    buffer = malloc(buffer_size);
    if (buffer == NULL) {
        perror("malloc");
        fclose(fd);
        return -1;
    }

    rc = 0;
    empty_line = 0;
    empty_line_count = 0;
    for (line = 1;; ++line) {
        int bol;
        int in_string;
        int space_count;
        int trailing_line;

        /* read the entire file, line by line */
        if (fgets(buffer, (int)buffer_size, fd) == NULL) {
            if (ferror(fd)) {
                perror("fgets");
                rc = -1;
            }
            break;
        }

        /* scan the line and check for unexpected elements */
        bol = 0;
        empty_line = 1;
        in_string = 0;
        space_count = 0;
        trailing_line = 0;
        for (ptr = buffer; *ptr; ++ptr) {
            if (*ptr == ' ') {
                if (!in_string)
                    ++space_count;
                if (!bol || bol == '\t') {
                    /* transition from tabs to spaces is */
                    /* allowed to align function */
                    /* parameters */
                    bol = ' ';
                }
                else if (bol != ' ' && bol != -1) {
                    printf("space and bol = %d\n", bol);
                    fprintf(stderr,
                        "%s:%d: mix of space and tab "
                        "at beginning of line\n",
                        filename, line);
                    rc = -1;
                    bol = -1;
                }
                ++trailing_line;
                continue;
            } else {
                if (space_count >= 8) {
                    fprintf(stderr,
                        "%s:%d: too much spaces\n",
                        filename, line);
                    rc = -1;
                }
                space_count = 0;
            }
            if (*ptr == '\t') {
                if (!bol)
                    bol = '\t';
                else if (bol != '\t' && bol != -1) {
                    fprintf(stderr,
                        "%s:%d: mix of space and tab "
                        "at beginning of line\n",
                        filename, line);
                    rc = -1;
                    bol = -1;
                }
                ++trailing_line;
                continue;
            }
            bol = -1;
            if (*ptr == '\n')
                continue;
            if (*ptr == '"') {
                in_string = !in_string;
            }
            empty_line = 0;
            trailing_line = 0;
        }

        /* report issues */
        if (empty_line) {
            ++empty_line_count;
        } else {
            if (empty_line_count > 1) {
                fprintf(stderr,
                    "%s:%d: multiple empty lines\n",
                    filename, line);
                rc = -1;
            }
            empty_line_count = 0;
        }

        if (trailing_line) {
            fprintf(stderr,
                "%s:%d: trailing space at end of line\n",
                filename, line);
            rc = -1;
        }
    }

    if (empty_line) {
        fprintf(stderr,
            "%s:%d: empty line at end of file\n",
            filename, line);
        rc = -1;
    }

    free(buffer);
    fclose(fd);

    return rc;
}

int main(int argc, char *argv[])
{
    int i, rc;

    rc = EXIT_SUCCESS;
    for (i = 1; i < argc; ++i) {
        if (check(argv[i]))
            rc = EXIT_FAILURE;
    }

    return rc;
}
