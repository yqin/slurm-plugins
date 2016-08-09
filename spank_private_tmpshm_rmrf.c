/*
 * Copyright (c) 2016, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * rmrf.c : implementation of "rm -rf" which is used in a SPANK epilog to
 *           remove a directory completely.
 *
 */

#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <slurm/spank.h>

int unlink_cb_f(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);
    if (rv) slurm_error("tmpshm: Unable to remove(%s): %m", fpath);
    return rv;
}

int rmrf(const char *path) {
    return nftw(path, unlink_cb_f, 64, FTW_DEPTH | FTW_PHYS);
}
