/*
 * Copyright (c) 2016-2017, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
 *
 * job_submit_require_cpu_gpu_ratio: Job Submit plugin to enfornce the
 *      CPU/GPU ratio to facilitate accounting.
 *
 * This plugin enforces the job to provide enough CPU number to match the GPU
 * number (e.g, 2:1) on a particular partition. If the provided number does not
 * match the job will be refused.
 *
 * Note you will need to change the definition of "mypart" and "ratio" in the
 * following code to meet your own requirement.
 *
 * gcc -shared -fPIC -pthread -I${SLURM_SRC_DIR}
 *     job_submit_require_cpu_gpu_ratio.c -o job_submit_require_cpu_gpu_ratio.so
 *
 */

#include <limits.h>
#include <regex.h>
#include <slurm/slurm_errno.h>
#include "src/slurmctld/slurmctld.h"

/* Required by Slurm job_submit plugin interface. */
const char plugin_name[] = "Require CPU/GPU ratio";
const char plugin_type[] = "job_submit/require_cpu_gpu_ratio";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/* Global variables. */
const char *myname = "job_submit_require_cpu_gpu_ratio";
/* GRES GPU regex. */
const char *gpu_regex="^gpu:[_[:alnum:]:]*([[:digit:]]+)$";

/* Number of partitions to be checked - need to modify. */
const int  npart = 2;
/* The Partition to be checked - need to modify. */
const char *mypart[2] = {"gpu", "gpu2"};
/* The CPU/GPU ratio that is checked against - need to modify. */
const int  ratio[2] = {2, 2};


/* Convert string to integer. */
int _str2int (char *str, uint32_t *p2int) {
    long int l;
    char *p;

    l = strtol(str, &p, 10);

    if ((*p != '\0') || (l > UINT32_MAX) || (l < 0)) {
        return -1;
    }

    *p2int = l;

    return 0;
}

/* Check GRES to make sure CPU/GPU ratio meeting requirement. */
int _check_ratio(char *part, char *gres, uint32_t ncpu) {
    if (part == NULL) {
        info("%s: missed partition info", myname);
        return SLURM_SUCCESS;
    }

    /* Loop through all partitions that need to be checked. */
    int i;
    for (i = 0; i < npart; i++) {
        if (strcmp(part, mypart[i]) == 0) {
            /* Require GRES on a GRES partition. */
            if (gres == NULL) {
                info("%s: missed GRES on partition %s", myname, mypart[i]);
                return ESLURM_INVALID_GRES;
            } else {
                regex_t re;
                regmatch_t rm[2];

                /* NUmber of GPUs. */
                uint32_t ngpu = 0;

                if (regcomp(&re, gpu_regex, REG_EXTENDED) != 0) {
                    info("%s: failed to compile regex '%s': %m", myname, gpu_regex);
                    return ESLURM_INTERNAL;
                }

                int rv = regexec(&re, gres, 2, rm, 0);

                regfree(&re);

                if (rv == 0) { /* match */
                    /* Convert the GPU # to integer. */
                    if (_str2int(gres + rm[1].rm_so, &ngpu) && ngpu < 1) {
                        info("%s: invalid GPU number %s", myname, gres + rm[1].rm_so);
                        return ESLURM_INVALID_GRES;
                    }

                    /* Sanity check of the CPU/GPU ratio. */
                    if (ncpu / ngpu < ratio[i]) {
                        info("%s: CPU=%zu, GPU=%zu, not qualify", myname, ncpu, ngpu);
                        return ESLURM_INVALID_GRES;
                    }
                } else if (rv == REG_NOMATCH) { /* no match */
                    info("%s: missed GPU on partition %s", myname, mypart[i]);
                    return ESLURM_INVALID_GRES;
                } else { /* error */
                    info("%s: failed to match regex '%s': %m", myname, gpu_regex);
                    return ESLURM_INTERNAL;
                }
            }
        }
    }

    return SLURM_SUCCESS;
}

extern int job_submit(struct job_descriptor *job_desc, uint32_t submit_uid,
        char **err_msg) {
    return _check_ratio(job_desc->partition,
                        job_desc->gres,
                        job_desc->min_cpus);
}

extern int job_modify(struct job_descriptor *job_desc,
        struct job_record *job_ptr, uint32_t submit_uid) {
    return _check_ratio(
        job_desc->partition == NULL ? job_ptr->partition : job_desc->partition,
        job_desc->gres == NULL ? job_ptr->gres : job_desc->gres,
        job_desc->min_cpus == (uint32_t) -2 ? job_ptr->total_cpus :
             job_desc->min_cpus);
}
