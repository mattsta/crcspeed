#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "crc64speed.h"
#include "crc64.h"

static long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec) * 1e6;
    ust += tv.tv_usec;
    return ust;
}

typedef uint64_t (*fns)(uint64_t, const unsigned char *, uint64_t);

/* GCC 4.8 on Linux is dumb */
#ifndef ftello
extern off_t ftello(FILE *stream);
#endif

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("e9c6d914c4b8d9ca == %016llx\n",
               (unsigned long long)crc64(0, (unsigned char *)"123456789", 9));
        return 0;
    }

    char *filename = argv[1];
    FILE *fp = fopen(filename, "r");

    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("Can't find file length");
        return 1;
    }
    off_t sz = ftello(fp);
    printf("File size: %0.2lf MB\n\n", (double)sz / 1024 / 1024);
    rewind(fp);
    char *contents = malloc(sz); /* potentially very big */

    if (fread(contents, sz, 1, fp) != 1) {
        free(contents);
        perror("Could not read entire file");
        return 1;
    }
    fclose(fp);

    crc64speed_init();

    fns compares[] = { crc64, crc64speed };
    char *names[] = { "regular", "crcspeed" };
    uint64_t results[sizeof(compares)/sizeof(*compares)] = { 0 };
    for (size_t i = 0; i < sizeof(compares) / sizeof(*compares); i++) {
        /* prime the code path with a dummy untimed call */
        compares[i](0, (unsigned char *)"howdy", 5);

        long long start = ustime();
        unsigned long long result =
            compares[i](0, (unsigned char *)contents, sz);
        long long end = ustime();

        results[i] = result;

        double total_time_seconds = (end - start) / 1e6;
        double size_mb = sz / 1024.0 / 1024.0;
        double speed = size_mb / total_time_seconds; /* MB per second */

        if (argc > 2) { /* easier parsing for comparisons */
            printf("%016llx:%lf\n", result, speed);
        } else { /* boring human readable results */
            printf("CRC-64 Result (%s): %016llx\n", names[i], result);
            printf("Evaluated %lf MB file in %lf seconds.\n", size_mb,
                   total_time_seconds);
            printf("Speed: %lf megabytes per second\n", speed);
        }
        printf("\n");
    }
    free(contents);

    uint64_t r = results[0];
    for (size_t i = 0; i < sizeof(results) / sizeof(*results); i++) {
        if (r != results[i])
            printf("ERROR! CRC results not match across functions at %ld!\n",
                   i);
        r = results[i];
    }

    return 0;
}
