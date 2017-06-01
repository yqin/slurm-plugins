/*
 * Copyright (c) 2016-2017, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * spank_private_tmpshm.c : SPANK plugin for a per-job tmp (/tmp, /var/tmp) 
 * and shm (/dev/shm).
 *
 * Managing /tmp has been an issue for HPC systems.  Typically there are three
 * ways to deal with it:
 *
 * 1. Set the $TMPDIR environment variable on a per-job session and clean it
 *    after the job is done.  But it is not scalable and it does not prevent
 *    creative users to get out of it.
 *
 * 2. Let the job to use /tmp and in the epilog find all the files owned by the
 *    user if s/he does not have an active job running on the node anymore clean
 *    them.  But if the user still has a job running, even though temp files
 *    may belong to the last job, they are still retained in this job session.
 *
 * 3. Use a separate namespace which is used in this plugin.  The idea is to
 *    create a per-job basis tmpdir and shmdir in /tmp and /dev/shm and attach
 *    it to the job's mount namespace.  It should work for most cases but does
 *    not work if user spawns a remote process to another node via ssh, which
 *    will bypass the namespace created by this plugin and falls back to the
 *    default namespace, such as Hadoop and Spark jobs. (TODO)
 *
 *
 * gcc -shared -fPIC -o spank_private_tmpshm.so rmrf.c spank_private_tmpshm.c
 *
 * plugstack.conf:
 * required /etc/slurm/spank/spank_private_tmpshm.so
 *
 */

#define _GNU_SOURCE

#include <errno.h>
#include <ftw.h>
#include <linux/limits.h>
#include <pwd.h>
#include <sched.h>
#include <slurm/spank.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mount.h>
#include <unistd.h>


SPANK_PLUGIN (spank_private_tmpshm, 1);
const char *myname = "spank_private_tmpshm";

extern int rmrf(const char*);

const char *shm_base = "/dev/shm";
const char *tmp_base = "/tmp";
const char *var_base = "/var/tmp";


/* Unlink callback function to handle both file and directory. */
int _unlink_cb_f(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    return remove(fpath);
}

/* rm -rf */
int _rmrf(const char *path) {
    return nftw(path, _unlink_cb_f, 64, FTW_DEPTH | FTW_PHYS);
}

/* Build per-job tmpdir and shmdir directory names. */
int _get_tmpshm (spank_t sp, char *tmpdir, char *shmdir) {
    uint32_t jobid;
    int rv;

    if (spank_get_item(sp, S_JOB_ID, &jobid)) {
        slurm_error("%s: Unable to get JOBID", myname);
        return -1;
    }

    rv = snprintf(tmpdir, PATH_MAX, "%s/job%d", tmp_base, jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to construct tmpdir: %s/job%d", myname, tmp_base, jobid);
        return -1;
    }

    rv = snprintf(shmdir, PATH_MAX, "%s/job%d", shm_base, jobid);

    if (rv < 0 || rv > PATH_MAX - 1) {
        slurm_error("%s: Unable to construct shmdir: %s/job%d", myname, shm_base, jobid);
        return -1;
    }

    return 0;
}

/* Create tmpdir and shmdir in prolog. */
int slurm_spank_job_prolog (spank_t sp, int ac, char **av) {
    uid_t uid = -1;
    gid_t gid = -1;
    struct passwd *pwd = NULL;

    char tmpdir[PATH_MAX];
    char shmdir[PATH_MAX];

    /* In prolog we can get uid but not gid. */
    if (spank_get_item(sp, S_JOB_UID, &uid)) {
        slurm_error("%s: Unable to get uid: %m", myname);
        return -1;
    }

    /* Get gid of the user. */
    errno = 0;
    pwd = getpwuid(uid);
    if (errno) {
        slurm_error("%s, Unable to get gid: %m", myname);
    }

    gid = pwd->pw_gid;

    /* Get private tmp and shm locations. */
    if (_get_tmpshm(sp, tmpdir, shmdir)) {
        slurm_error("%s: Unable to construct tmpdir or shmdir", myname);
        return -1;
    }

    /* Create private tmp and shm directories. */
    if (mkdir(tmpdir, 0700) && errno != EEXIST) {
        slurm_error("%s: Unable to mkdir(%s, 0700): %m", myname, tmpdir);
        return -1;
    }

    if (mkdir(shmdir, 0700) && errno != EEXIST) {
        slurm_error("%s: Unable to mkdir(%s, 0700): %m", myname, shmdir);
        return -1;
    }

    /* Change the ownership to current job user. */
    if (chown(tmpdir, uid, gid)) {
        slurm_error("%s: Unable to chown(%s, %u, %u): %m", myname, tmpdir, uid, gid);
        return -1;
    }

    if (chown(shmdir, uid, gid)) {
        slurm_error("%s: Unable to chown(%s, %u, %u): %m", myname, shmdir, uid, gid);
        return -1;
    }

    return 0;
}

/* Clone mount namespace and bind mount tmpdir and shmdir in the current
 * namespace for each task before the priviledge is dropped. This callback
 * function is only executed in a remote context. */
int slurm_spank_task_init_privileged (spank_t sp, int ac, char **av) {
    char tmpdir[PATH_MAX];
    char shmdir[PATH_MAX];

    /* Get private tmp and shm locations. */
    if (_get_tmpshm(sp, tmpdir, shmdir)) {
        slurm_error("%s: Unable to construct tmpdir or shmdir", myname);
        return -1;
    }

    /* Make entire '/' mount tree shareable. */
    if (mount("", "/", "none", MS_REC|MS_SHARED, "")) {
        slurm_error("%s: Unable to share '/' mounts: %m", myname);
        return -1;
    }

    /* Create a new namespace. */
    if (unshare(CLONE_NEWNS)) {
        slurm_error("%s: Unable to unshare(CLONE_NEWNS): %m", myname);
        return -1;
    }

    /* Make entire '/' mount tree slave. */
    if (mount("", "/", "none", MS_REC|MS_SLAVE, "")) {
        slurm_error("%s: Unable to 'mount --make-rslave /'", myname);
        return -1;
    }

    /* Bind mount '/var/tmp'. */
    if (mount(tmpdir, var_base, "none", MS_BIND, "")) {
        slurm_error("%s: Unable to bind mount(%s, %s): %m", myname, tmpdir, var_base);
        return -1;
    }

    /* Bind mount '/tmp'. */
    if (mount(tmpdir, tmp_base, "none", MS_BIND, "")) {
        slurm_error("%s: Unable to bind mount(%s, %s): %m", myname, tmpdir, tmp_base);
        return -1;
    }

    /* Bind mount '/dev/shm'. */
    if (mount(shmdir, shm_base, "none", MS_BIND, "")) {
        slurm_error("%s: Unable to bind mount(%s, %s): %m", myname, shmdir, shm_base);
        return -1;
    }

    return 0;
}

/* Remove tmpdir and shmdir in epilog. */
int slurm_spank_job_epilog (spank_t sp, int ac, char **av) {
    char tmpdir[PATH_MAX];
    char shmdir[PATH_MAX];

    /* Get private tmp and shm locations. */
    if (_get_tmpshm(sp, tmpdir, shmdir)) {
        slurm_error("%s: Unable to construct tmpdir or shmdir", myname);
        return -1;
    }

    /* Remove tmp and shm. */
    if (_rmrf(tmpdir) && errno != ENOENT) {
        slurm_error("%s: Unable to rmrf(%s) (tmpdir): %m", myname, tmpdir);
        return -1;
    }

    if (_rmrf(shmdir) && errno != ENOENT) {
        slurm_error("%s: Unable to rmrf(%s) (shmdir): %m", myname, shmdir);
        return -1;
    }

    return 0;
}
