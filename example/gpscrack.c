/**
 * Copyright (C) 2017 Xevo Inc. All Rights Reserved.
 * Author: Martin Kelly <mkelly@xevo.com>
 *
 * A simple cracker for the gpsdsrc, dumping each buffer in human-readable form.
 * Reads binary struct gps_fix_t input from stdin.
 */

#include <gps.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DUMP(fix, field, desc) \
    printf("%s: %g\n", desc, fix->field)

void dump_timestamp(timestamp_t ts)
{
    time_t time;

    time = (time_t) ts;
    printf("time: %s", ctime(&time));
}

void dump_fix(struct gps_fix_t *fix)
{
    if (fix->mode == MODE_NOT_SEEN) {
        return;
    }

    /* See gps.h (shipped with gpsd) for details. */
    dump_timestamp(fix->time);
    DUMP(fix, ept, "expected time uncertainty");

    if (fix->mode >= MODE_2D) {
        DUMP(fix, latitude, "latitude (degrees)");
        DUMP(fix, epy, "latitude position uncertainty (meters)");
        DUMP(fix, longitude, "longitude (degrees)");
        DUMP(fix, epx, "longitude position uncertainty (meters)");
        DUMP(fix, track, "course made good (relative to true north)");
        DUMP(fix, epd, "track uncertainty (degrees)");
        DUMP(fix, speed, "speed over ground, (m/s)");
        DUMP(fix, eps, "speed uncertainty, (m/s)");
    }

    if (fix->mode >= MODE_3D) {
        DUMP(fix, altitude, "altitude (meters)");
        DUMP(fix, epv, "vertical position uncertainty (meters)");
        DUMP(fix, climb, "vertical speed, (m/s)");
        DUMP(fix, epc, "vertical speed uncertainty");
    }
}

int main(void)
{
    size_t bytes;
    struct gps_fix_t fix;
    int status;

    status = EXIT_SUCCESS;
    while (true) {
        bytes = fread(&fix, sizeof(fix), 1, stdin);
        if (bytes != 1) {
            /* Stream is closed or in error; we're done. */
            if (ferror(stdin)) {
                fprintf(stderr, "I/O error\n");
            }
            status = EXIT_FAILURE;
            break;
        }

        dump_fix(&fix);
        putchar('\n');
    }

    return status;
}
