/*
 * Copyright (c) 2016, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * spank_demo.c : SPANK plugin to demonstrate when and where and by whom the
 *                function is called.
 *
 *
 * gcc -shared -fPIC -o spank_demo.so spank_demo.c
 *
 * plugstack.conf:
 * required /etc/slurm/spank/spank_demo.so
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <slurm/spank.h>


SPANK_PLUGIN (spank_demo, 1);
const char *myname = "spank_demo";

/* Display a message through Slurm SPANK system. */
int display_msg(spank_t sp, char const *caller, char const *msg) {
    uid_t uid = getuid();
    gid_t gid = getgid();
    char hostname[1024];

    int ctx = spank_context();
    char *ctx_str[] = {"ERROR", "LOCAL", "REMOTE", "ALLOCATOR", "SLURMD", "JOB_SCRIPT"};

    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    if (msg && msg[0]) {
        slurm_info("%s: %s, %s, %s (uid=%d, gid=%d): %s", myname, ctx_str[ctx], hostname, caller, uid, gid, msg);
    }
    else {
        slurm_info("%s: %s, %s, %s (uid=%d, gid=%d)", myname, ctx_str[ctx], hostname, caller, uid, gid);
    }

    return 0;
}

int slurm_spank_init (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_slurmd_init (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_job_prolog (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_init_post_opt (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_local_user_init (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_user_init (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_task_init_privileged (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_task_init (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_task_post_fork (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_task_exit (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_exit (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_job_epilog (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}

int slurm_spank_slurmd_exit (spank_t sp, int ac, char **av) {
    display_msg(sp, __FUNCTION__, NULL);
    return 0;
}
