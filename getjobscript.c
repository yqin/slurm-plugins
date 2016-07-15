/*
 * Copyright (c) 2016, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * getjobscript.c: SPANK plugin to collect job script.
 *
 * This plugin creates a separate directory for each day on a shared stoarge
 * and copy the job script to that location.
 *
 *
 * gcc -shared -fPIC -o getjobscript.so getjobscript.c
 *
 * plugstack.conf:
 * required /etc/slurm/spank/getjobscript.so source=foo target=bar
 *
 */

#include <errno.h>
#include <linux/limits.h>
#include <slurm/spank.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>


SPANK_PLUGIN (getjobscript, 1);
const char *myname = "getjobscript";

/* Get current date string in "%F" ("%Y-%m-%d") format. */
int get_datestring (spank_t sp, char *ds, int len) {
    time_t t;
    struct tm *lt;

    /* Obtain current time. */
    t = time(NULL);

    if (t == ((time_t) -1)) {
        slurm_error("%s: Unable to get current time", myname);
        return -1;
    }

    /* Covert to local time. */
    lt = localtime(&t);

    if (lt == NULL) {
        slurm_error("%s: Unable to convert to local time", myname);
        return -1;
    }

    /* Convert to string format. */
    if (strftime(ds, len, "%F", lt) == 0) {
        slurm_error("%s: Unable to convert to date string", myname);
        return -1;
    }

    return 0;
}

/* Copy source file to target. */
int copy_file (spank_t sp, const char* source_file, const char* target_file) {
    char ch;
    FILE *source, *target;
    int rv;

    source = fopen(source_file, "r");

    if (source == NULL) {
        slurm_error("%s: Unable to open %s for read: %m", myname, source_file);
        return -1;
    }

    target = fopen(target_file, "w");

    if (target == NULL) {
        slurm_error("%s: Unable to open %s for write: %m", myname, target_file);
        return -1;
    }

    while ((ch = fgetc(source)) != EOF) {
        if (fputc(ch, target) == EOF) {
            slurm_error("%s: Unable to write to %s: %m", myname, target_file);
            return -1;
        }
    }

    fclose(target);
    fclose(source);

    return 0;
}

/* Make a copy of the current job script. */
int slurm_spank_init(spank_t sp, int ac, char **av) {
    int i;
    int rv;
    uint32_t jobid;
    char ds[11];
    char *source_base = NULL;
    char *target_base = NULL;
    char source_file[PATH_MAX];
    char target_file[PATH_MAX];
    char target_dir[PATH_MAX];

    /* If not in a remote context no need to proceed. */
    if (spank_remote(sp) != 1) return 0;

    /* Processing command line arguments. */
    for (i = 0; i < ac; i++) {
        if (strncmp("source=", av[i], 7) == 0) {
            source_base = av[i] + 7;
        } else if (strncmp("target=", av[i], 7) == 0) {
            target_base = av[i] + 7;
        }
    }

    /* Sanity checking of the source and target's existence. */
    if (source_base == NULL) {
        slurm_error("%s: syntax: %s source=foo target=bar", myname, myname);
        slurm_error("%s: missing source location", myname);
        return -1;
    }

    if (access(source_base, F_OK) == -1) {
        slurm_error("%s: %s does not exist", myname, source_base);
        return -1;
    }

    if (target_base == NULL) {
        slurm_error("%s: syntax: %s source=foo target=bar", myname, myname);
        slurm_error("%s: missing target location", myname);
        return -1;
    }

    if (access(target_base, F_OK) == -1) {
        slurm_error("%s: %s does not exist", myname, target_base);
        return -1;
    }

    if (spank_get_item(sp, S_JOB_ID, &jobid)) {
        slurm_error("%s: Unable to get JOBID", myname);
        return -1;
    }

    /* Construct current job script location. */
    rv = snprintf(source_file, PATH_MAX, "%s/job%05d/slurm_script", source_base, jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to construct job script location: %s/job%05d/slurm_script", myname, source_base, jobid);
        return -1;
    }

    /* If job script does not exist no need to proceed. */
    if (access(source_file, F_OK) == -1) {
        slurm_info("%s: %s does not exist", myname, source_file);
        return 0;
    }

    /* Obtain current date string. */
    if (get_datestring(sp, ds, sizeof(ds))) {
        slurm_error("%s: Unable to get current date string", myname);
        return -1;
    }

    /* Construct target directory location to store daily job scripts. */
    rv = snprintf(target_dir, PATH_MAX, "%s/%s", target_base, ds);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to contruct target directory: %s/%s", myname, target_base, ds);
        return -1;
    }

    /* Construct target file location to save current job script. */
    rv = snprintf(target_file, PATH_MAX, "%s/%s/job%d", target_base, ds, jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to construct target_file: %s/job%d", myname, target_base, jobid);
        return -1;
    }

    /* Create target directory to store job scripts. If it doesn't exist, create it, otherwise ignore. */
    if (mkdir(target_dir, 0700) && errno != EEXIST) {
        slurm_error("%s: Unable to mkdir(%s, 0700): %m", myname, target_dir);
        return -1;
    }

    /* Copy the job script, ignore if file already exist. */
    if (access(target_file, F_OK) == 0) return 0;

    if (copy_file(sp, source_file, target_file)) {
        slurm_warn("%s: Unable to copy %s to %s", myname, source_file, target_file);
    }

    slurm_info("%s: %s copied", myname, source_file);

    return 0;
}
