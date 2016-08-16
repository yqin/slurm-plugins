/*
 * Copyright (c) 2016, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * job_submit_collect_script: Job Submit plugin to collect job script.
 *
 * This plugin collects the job script and workdir at submission time and store
 * them into a dayly directory within the pre-defined "target_base" location.
 *
 * Note you will need to change the definition of "target_base" to provide the
 * location where the job scripts should be stored into.
 *
 * gcc -shared -fPIC -pthread -I${SLURM_SRC_DIR}
 *     job_submit_collect_script.c -o job_submit_collect_script.so
 *
 */

#include <limits.h>
#include <slurm/slurm_errno.h>
#include "src/slurmctld/slurmctld.h"

/* Required by Slurm job_submit plugin interface. */
const char plugin_name[] = "Collect job script and workdir";
const char plugin_type[] = "job_submit/collect_script";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/* Global variables. */
const char *myname = "job_submit_collect_script";
const char *target_base = "/global/sched/slurm/jobscripts";


/* Get current date string in "%F" ("%Y-%m-%d") format. */
int _get_datestr (char *ds, int len) {
    time_t t;
    struct tm *lt;

    /* Obtain current time. */
    t = time(NULL);

    if (t == ((time_t) -1)) {
        info("%s: Unable to get current time", myname);
        return -1;
    }

    /* Convert to local time. */
    lt = localtime(&t);

    if (lt == NULL) {
        info("%s: Unable to convert to local time", myname);
        return -1;
    }

    /* Convert to string format. */
    if (strftime(ds, len, "%F", lt) == 0) {
        info("%s: Unable to convert to date string", myname);
        return -1;
    }

    return 0;
}

extern int job_submit(struct job_descriptor *job_desc, uint32_t submit_uid,
        char **err_msg) {
    /* TODO: job_desc->job_id is not available at submit time, so no way to
     *       identify the job by job id. What to do? */
    uint32_t jobid = job_desc->job_id;
    char ds[11];    /* Date string in "%F" ("%Y-%m-%d") format. */
    char target_dir[PATH_MAX];      /* Target directory. */
    char target_script[PATH_MAX];   /* Target job script filename. */
    char target_workdir[PATH_MAX];  /* Target workdir filename. */
    FILE *fd = NULL;    /* File handle for write. */
    int rv;

    /* If job script is not available no need to proceed. */
    if (job_desc->script == NULL) return SLURM_SUCCESS;

    /* Obtain current date string. */
    if (_get_datestr(ds, sizeof(ds))) {
        info("%s: Unable to get current date string", myname);
        return ESLURM_INTERNAL;
    }

    /* Construct target directory location to store daily job scripts. */
    rv = snprintf(target_dir, PATH_MAX, "%s/%s", target_base, ds);

    if (rv < 0 || rv > PATH_MAX - 1) {
        info("%s: Unable to contruct target_dir: %s/%s", myname, target_base,
            ds);
        return ESLURM_INTERNAL;
    }

    /* Construct target script location to save current job script. */
    rv = snprintf(target_script, PATH_MAX, "%s/job%lu.script", target_dir,
         jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        info("%s: Unable to construct target_script: %s/job%lu.script", myname,
            target_dir, jobid);
        return ESLURM_INTERNAL;
    }

    /* Construct target wordir location to save current job workdir. */
    rv = snprintf(target_workdir, PATH_MAX, "%s/job%lu.workdir", target_dir,
         jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        info("%s: Unable to construct target_workdir: %s/job%lu.workdir",
            myname, target_dir, jobid);
        return ESLURM_INTERNAL;
    }

    /* Ignore if target script exists. */
    if (access(target_script, F_OK) == 0) {
        info("%s: %s exists, ignore", myname, target_script);
        return SLURM_SUCCESS;
    } 

    /* Create target directory to store job scripts. */
    /* If it doesn't exist, create it, otherwise ignore. */
    if (mkdir(target_dir, 0750) && errno != EEXIST) {
        info("%s: Unable to mkdir(%s): %m", myname, target_dir);
        return ESLURM_INTERNAL;
    }

    /* Open the target file for write. */
    fd = fopen(target_script, "wb");

    if (fd == NULL) {
        info("%s: Unable to open %s: %m", myname, target_script);
        return ESLURM_INTERNAL;
    }

    /* Write job script to file. */
    fwrite(job_desc->script, strlen(job_desc->script), 1, fd);

    if (ferror(fd)) {
        info("%s: Error on writing %s: %m", myname, target_script);
        return ESLURM_WRITING_TO_FILE;
    }

    /* Close the target file. */
    fclose(fd);

    info("%s: Job script saved as %s", myname, target_script);

    /* Open the target file for write. */
    fd = fopen(target_workdir, "wb");

    if (fd == NULL) {
        info("%s: Unable to open %s: %m", myname, target_workdir);
        return ESLURM_INTERNAL;
    }

    /* Write job workdir to file. */
    fwrite(job_desc->work_dir, strlen(job_desc->work_dir), 1, fd);

    if (ferror(fd)) {
        info("%s: Error on writing %s: %m", myname, target_workdir);
        return ESLURM_WRITING_TO_FILE;
    }

    /* Close the target file. */
    fclose(fd);

    info("%s: Job workdir saved as %s", myname, target_workdir);

    return SLURM_SUCCESS;
}

extern int job_modify(struct job_descriptor *job_desc,
        struct job_record *job_ptr, uint32_t submit_uid) {
    return SLURM_SUCCESS;
}
