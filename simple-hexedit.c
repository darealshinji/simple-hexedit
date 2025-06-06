/**
 * MIT License
 *
 * Copyright (C) 2023-2025 Carsten Janssen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
# define _CRT_NONSTDC_NO_WARNINGS
# include <io.h>
# define strcasecmp _stricmp
#else
# include <unistd.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# define PATH_SEPARATOR '\\'
#else
# define PATH_SEPARATOR '/'
#endif


/* strtol() with error checks */
static long xstrtol(const char *str, int base)
{
    char *endptr = NULL;

    errno = 0;
    long l = strtol(str, &endptr, base);

    if (errno != 0) {
        perror("strtol()");
        exit(1);
    }

    if (*endptr != 0) {
        fprintf(stderr, "error: argument cannot be interpreted as number: %s\n", str);
        exit(1);
    }

    return l;
}


/* convert string to unsigned char */
static uint8_t strtouchar(const char *str, int base)
{
    long l = xstrtol(str, base);

    if (l < 0 || l > 255) {
        fprintf(stderr, "error: argument value outside of 8 bit range: %s (%ld)\n", str, l);
        exit(1);
    }

    return (uint8_t)l;
}


/* check if str begins with \x or 0x */
static bool hex_prefix(const char *str)
{
    return ((str[0] == '0' || str[0] == '\\') &&
            (str[1] == 'x' || str[1] == 'X') &&
             str[2] != 0);
}


/* convert string to long (xstrtol() wrapper) */
static long get_long(const char *str)
{
    if (hex_prefix(str)) {
        /* hexadecimal number */
        char *copy = strdup(str);
        copy[0] = '0'; /* turn "\x" prefix into "0x" */

        long l = xstrtol(copy, 16);
        free(copy);

        return l;
    } else if (str[0] == '0' && str[1] != 0) {
        /* octal number */
        return xstrtol(str, 8);
    }

    /* decimal number */
    return xstrtol(str, 10);
}


/* read data and print as hex digits on screen */
static void read_data(const char *arg_offset, const char *arg_length, const char *file)
{
    long offset, len;

    /* open file */
    FILE *fp = fopen(file, "rb");

    if (!fp) {
        perror("fopen()");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);

    /* check offset and filesize */
    if (strcasecmp(arg_offset, "append") == 0 ||
        (offset = get_long(arg_offset)) >= fsize)
    {
        fprintf(stderr, "error: offset equals or exceeds filesize\n");
        fclose(fp);
        exit(1);
    }

    /* seek to offset */
    if (fseek(fp, offset, SEEK_SET) == -1) {
        perror("fseek()");
        fclose(fp);
        exit(1);
    }

    /* get length to read */
    if (strcasecmp(arg_length, "all") == 0 ||
        (len = get_long(arg_length)) < 1)
    {
        fseek(fp, 0, SEEK_END);
        len = ftell(fp) - offset;
        fseek(fp, offset, SEEK_SET);
    }

    /* read and print bytes */
    for (long i = 0; i < len; i++) {
        int c = fgetc(fp);

        if (c == EOF) {
            putchar('\n');
            break;
        }

        int j = i + 1;

        if (i > 0 && j%4 == 0 && j%16 != 0) {
            /* print trailing space */
            printf(" %02X ", (uint8_t)c);
        } else {
            /* no trailing space */
            printf(" %02X", (uint8_t)c);
        }

        if (j == len || (i > 0 && j%16 == 0)) {
            putchar('\n');
        }
    }

    fflush(stdout);
    fclose(fp);
}


/* write data to file */
static void write_to_file(const char *file, const uint8_t *data, const uint8_t *byte, const char *arg_offset, long num_bytes)
{
    int sk;
    long written = 0;

    /* open or create file */
    int fd = open(file, O_RDWR | O_CREAT, 0664);

    if (fd == -1) {
        perror("open()");
        exit(1);
    }

    /* seek to offset */
    if (strcasecmp(arg_offset, "append") == 0) {
        sk = lseek(fd, 0, SEEK_END);
    } else {
        sk = lseek(fd, get_long(arg_offset), SEEK_SET);
    }

    if (sk == -1) {
        perror("lseek()");
        close(fd);
        exit(1);
    }

    /* write data */
    if (byte) {
        /* "memset" */
        for ( ; written < num_bytes; written++) {
            if (write(fd, byte, 1) != 1) {
                break;
            }
        }
    } else {
        written = write(fd, data, num_bytes);
    }

    if (written != num_bytes) {
        perror("write()");
        close(fd);
        exit(1);
    }

    printf("%ld bytes successfully written to `%s'\n", num_bytes, file);
    close(fd);
}


/* convert hex string to uchar array */
static uint8_t *hex_to_uchar(const char *arg_data, size_t *num_bytes)
{
    uint8_t *data = malloc((strlen(arg_data) / 2) + 2);

    if (!data) {
        perror("malloc()");
        exit(1);
    }

    uint8_t *pdata = data;
    char buf[5] = { '0','x', 0, 0, 0 };
    *num_bytes = 0;

    /* convert hex string to bytes */
    for (const char *p = arg_data; *p != 0; p++) {
        if (isspace(*p)) {
            /* ignore space */
            continue;
        }

        /* error if character is not a hex digit */
        if (!isxdigit(*p)) {
            if (isprint(*p)) {
                fprintf(stderr, "error: character `%c' is not a hexadecimal digit\n", *p);
            } else {
                fprintf(stderr, "error: character `0x%02X' is not a hexadecimal digit\n", (uint8_t)*p);
            }
            free(data);
            exit(1);
        }

        if (buf[2] == 0) {
            /* save char */
            buf[2] = *p;
            continue;
        }

        buf[3] = *p;
        *pdata = strtouchar(buf, 16);

        /* reset buffer */
        buf[2] = buf[3] = 0;

        *num_bytes += 1;
        pdata++;
    }

    /* trailing single-digit hex number */
    if (buf[2] != 0) {
        *pdata = strtouchar(buf, 16);
        *num_bytes += 1;
    }

    return data;
}


/* get single character from argument (can be an escape sequence) */
static uint8_t get_uchar(const char *arg_char)
{
    const long len = strlen(arg_char);

    if (len == 1) {
        /* literal character */
        return (uint8_t)arg_char[0];
    }

    if (hex_prefix(arg_char)) {
        /* hex number (0x.. or \x..) */
        char *copy = strdup(arg_char);
        copy[0] = '0';  /* turn "\x" prefix into "0x" */
        uint8_t c = strtouchar(copy, 16);
        free(copy);
        return c;
    }

    if (arg_char[0] == '\\' && arg_char[1] != 0) {
        if (len == 2) {
            /* control character */
            switch(arg_char[1]) {
                case 'n': return '\n';
                case 't': return '\t';
                case 'r': return '\r';
                case 'a': return '\a';
                case 'b': return '\b';
                case 'f': return '\f';
                case 'v': return '\v';
                case 'e': return 0x1B;
                default:
                    break;
            }
        }

        /* escaped decimal number */
        return strtouchar(arg_char + 1, 10);
    }

    fprintf(stderr, "error: invalid argument: %s\n", arg_char);
    exit(1);
}


/* write arg_data at arg_offset to file */
static void write_data(const char *arg_offset, const char *arg_data, const char *file)
{
    if (*arg_data == 0) {
        fprintf(stderr, "error: empty argument\n");
        exit(1);
    }

    size_t num_bytes;
    uint8_t *data = hex_to_uchar(arg_data, &num_bytes);

    write_to_file(file, data, NULL, arg_offset, num_bytes);

    free(data);
}


/* create a data set of arg_length and write it to file */
static void memset_write_data(const char *arg_offset, const char *arg_length, const char *arg_char, const char *file)
{
    const long len = get_long(arg_length);

    if (len < 1) {
        fprintf(stderr, "error: length must be 1 or more: %s\n", arg_length);
        exit(1);
    }

    const uint8_t c = get_uchar(arg_char);

    write_to_file(file, NULL, &c, arg_offset, len);
}


static void print_usage(const char *self)
{
    const char *p = strrchr(self, PATH_SEPARATOR);

    if (p && *(p+1) != 0) {
        self = p + 1;
    }

    printf("usage:\n"
           "  %s --help\n"
           "  %s r[ead] [<offset> <length>] <file>\n"
           "  %s w[rite] <offset> <data> <file>\n"
           "  %s m[emset] <offset> <length> <char> <file>\n",
        self, self, self, self);
}


static void show_help(const char *self)
{
    print_usage(self);

    printf("\n\n"
           "  read, write, memset: <offset> and <length> may be hexadecimal prefixed with\n"
           "    `0x' or `\\x', an octal number prefixed with `0' or decimal\n"
           "\n"
           "  read: <length> set to 0 or `all' will print all bytes\n"
           "\n"
           "  write, memset: <offset> set to `append' will write data directly after the\n"
           "    end of the file\n"
           "\n"
           "  write: <data> must be hexadecimal without prefixes (whitespaces are ignored)\n"
           "\n"
           "  write: <char> can be a literal character, escaped control character,\n"
           "    hexadecimal value prefixed with `0x' or `\\x', an octal number prefixed\n"
           "    with `0' or a decimal number prefixed with `\\'\n"
           "\n");
}


static bool is_cmd(const char *arg, const char *cmd1, const char *cmd2)
{
    return (strcasecmp(arg, cmd1) == 0 || strcasecmp(arg, cmd2) == 0);
}


int main(int argc, char **argv)
{
    const char *a_cmd, *a_offset, *a_length, *a_data, *a_char, *a_file;

    if (argc > 1) {
        a_cmd = argv[1];
    }

    switch (argc)
    {
    case 2:
        if (strcmp(argv[1], "--help") == 0) {
            show_help(argv[0]);
            return 0;
        }
        break;

    case 3:
        if (is_cmd(a_cmd, "r", "read")) {
            a_offset = "0";
            a_length = "all";
            a_file = argv[2];
            read_data(a_offset, a_length, a_file);
            return 0;
        }
        break;

    case 5:
        if (is_cmd(a_cmd, "r", "read")) {
            a_offset = argv[2];
            a_length = argv[3];
            a_file = argv[4];
            read_data(a_offset, a_length, a_file);
            return 0;
        } else if (is_cmd(a_cmd, "w", "write")) {
            a_offset = argv[2];
            a_data = argv[3];
            a_file = argv[4];
            write_data(a_offset, a_data, a_file);
            return 0;
        }
        break;

    case 6:
        if (is_cmd(a_cmd, "m", "memset")) {
            a_offset = argv[2];
            a_length = argv[3];
            a_char = argv[4];
            a_file = argv[5];
            memset_write_data(a_offset, a_length, a_char, a_file);
            return 0;
        }
        break;

    default:
        break;
    }

    print_usage(argv[0]);

    return 1;
}
