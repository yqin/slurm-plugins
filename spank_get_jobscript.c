/*
 * Copyright (c) 2016, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * getjobscript.c: SPANK plugin to collect job script.
 *
 * This plugin creates a separate directory for each day on a shared stoarge
 * and copy the job script to that location. Another way to achieve this is to
 * instrument a slurmctld prolog and collect the job script from the hash dirs
 * within $StateSaveLocation.
 *
 * gcc -shared -fPIC -o getjobscript.so getjobscript.c
 *
 * plugstack.conf:
 * required /etc/slurm/spank/getjobscript.so source=/var/slurm/spool
 *          target=shared_dir [uid=new_uid] [gid=new_gid]
 *
 * Note: new_uid and new_gid have to be what SlurmdUser can switch to 
 *       (setuid/gid). They can also be ignore (optional arguments), or set to
 *       -1 to not to change from the current user/group.
 *
 */

#include <errno.h>
#include <limits.h>
#include <linux/limits.h>
#include <slurm/spank.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


SPANK_PLUGIN (getjobscript, 1);
const char *myname = "getjobscript";


/* Convert a string to UID/GID. */
int _str2id (char * str, int *p2int) {
    long int l;
    char *p;

    l = strtol(str, &p, 10);

    if ((*p != '\0') || (l > UINT_MAX) || (l < 0)) {
        return -1;
    }

    *p2int = l;

    return 0;
}

/* Get current date string in "%F" ("%Y-%m-%d") format. */
int _get_datestr (char *ds, int len) {
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

/* Restore UID, GID, and free buffer before exit. */
int _clean_exit (uid_t ruid, gid_t rgid, char *buffer) {
    if (rgid != -1)
        setegid(rgid);

    if (ruid != -1)
        seteuid(ruid);

    if (buffer != NULL)
        free(buffer);
}

/* Make a copy of the current job script in slurm_spank_init(). */
int slurm_spank_init (spank_t sp, int ac, char **av) {
    int i;
    int rv;

    /* Effective and real user/group. */
    uid_t euid = -1;
    gid_t egid = -1;
    uid_t ruid = -1;
    gid_t rgid = -1;

    uint32_t jobid;

    /* Date string in "%F" ("%Y-%m-%d") format. */
    char ds[11];

    /* Source and target base directory locations. */
    char *source_base = NULL;
    char *target_base = NULL;

    /* Target directory location. */
    char target_dir[PATH_MAX];

    /* Source and target filenames. */
    char source_file[PATH_MAX];
    char target_file[PATH_MAX];

    /* File handle for read and write. */
    FILE *fd = NULL;

    /* Buffer to store the source file. */
    char *buffer = NULL;

    /* Size of the buffer. */
    long fsize = 0;

    /* If not in a remote context no need to proceed. */
    if (spank_remote(sp) != 1) return 0;

    /* Processing command line arguments. */
    for (i = 0; i < ac; i++) {
        if (strncmp("source=", av[i], 7) == 0) {
            source_base = av[i] + 7;
        } else if (strncmp("target=", av[i], 7) == 0) {
            target_base = av[i] + 7;
        } else if (strncmp("uid=", av[i], 4) == 0) {
            if (_str2id(av[i] + 4, &euid)) {
                slurm_error("%s: Unable to conver string \"%s\" to UID", myname, av[i] + 4);
                return -1;
            }

            if (euid == getuid()) {
                euid = -1;
            } else {
                ruid = getuid();
            }
        } else if (strncmp("gid=", av[i], 4) == 0) {
            if (_str2id(av[i] + 4, &egid)) {
                slurm_error("%s: Unable to conver string \"%s\" to GID", myname, av[i] + 4);
                return -1;
            }

            if (egid == getgid()) {
                egid = -1;
            } else {
                rgid = getgid();
            }
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
        slurm_info("%s: Job script %s does not exist, ignore", myname, source_file);
        return 0;
    }

    /* Open the source job script. */
    fd = fopen(source_file, "rb");

    if (fd == NULL) {
        slurm_error("%s: Unable to open %s: %m", myname, source_file);
        return -1;
    }

    /* Get the size of the source job script. */
    if (fseek(fd, 0L, SEEK_END)) {
        slurm_error("%s: Unable to fseek to end of %s: %m", myname, source_file);
        return -1;
    }

    fsize = ftell(fd);

    /* If source job script is empty no need to proceed. */
    if (fsize == 0) {
        slurm_info("%s: %s is empty", myname, source_file);
        return 0;
    }

    if (fsize == -1) {
        slurm_error("%s: error getting size of %s: %m", myname, source_file);
        return -1;
    }

    if (fseek(fd, 0L, SEEK_SET)) {
        slurm_error("%s: Unable to fseek to start of %s: %m", myname, source_file);
        return -1;
    }

    /* Allocate buffer. */
    buffer = malloc((fsize + 1) * sizeof(char));

    if (buffer == NULL) {
        slurm_error("%s: Unable to allocate buffer: %m", myname);
        return -1;
    }

    /* Read the job script into buffer. */
    fread(buffer, fsize, 1, fd);
    buffer[fsize] = 0;

    if (ferror(fd)) {
        slurm_error("%s: Error on reading %s: %m", myname, source_file);
        free(buffer);
        return -1;
    }

    /* Close the source file. */
    fclose(fd);

    /* Obtain current date string. */
    if (_get_datestr(ds, sizeof(ds))) {
        slurm_error("%s: Unable to get current date string", myname);
        free(buffer);
        return -1;
    }

    /* Construct target directory location to store daily job scripts. */
    rv = snprintf(target_dir, PATH_MAX, "%s/%s", target_base, ds);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to contruct target directory: %s/%s", myname, target_base, ds);
        free(buffer);
        return -1;
    }

    /* Construct target file location to save current job script. */
    rv = snprintf(target_file, PATH_MAX, "%s/%s/job%d", target_base, ds, jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to construct target_file: %s/job%d", myname, target_base, jobid);
        free(buffer);
        return -1;
    }

    /* Ignore if target file exists - to prevent overwritten by multiple job steps. */
    if (access(target_file, F_OK) == 0) {
        slurm_info("%s: %s exists, ignore", myname, target_file);
        free(buffer);
        return 0;
    } 
    
    /* Switch user. */
    if (egid != -1 && setegid(egid)) {
        slurm_error("%s: Unable to setegid(%d): %m", myname, egid);
        _clean_exit(ruid, rgid, buffer);
        return -1;
    }

    if (euid != -1 && seteuid(euid)) {
        slurm_error("%s: Unable to seteuid(%d): %m", myname, euid);
        _clean_exit(ruid, rgid, buffer);
        return -1;
    }

    /* Create target directory to store job scripts. If it doesn't exist, create it, otherwise ignore. */
    if (mkdir(target_dir, 0750) && errno != EEXIST) {
        slurm_error("%s: Unable to mkdir(%s): %m", myname, target_dir);
        _clean_exit(ruid, rgid, buffer);
        return -1;
    }

    /* Open the target file for write. */
    fd = fopen(target_file, "wb");

    if (fd == NULL) {
        slurm_error("%s: Unable to open %s: %m", myname, target_file);
        _clean_exit(ruid, rgid, buffer);
        return -1;
    }

    /* Write buffer to the target job script file. */
    fwrite(buffer, fsize, 1, fd);

    if (ferror(fd)) {
        slurm_error("%s: Error on writing %s: %m", myname, target_file);
        _clean_exit(ruid, rgid, buffer);
        return -1;
    }

    /* Close the target file. */
    fclose(fd);

    slurm_info("%s: Job script saved as %s", myname, target_file);

    /* Clean exit. */
    _clean_exit(ruid, rgid, buffer);

    return 0;
}
