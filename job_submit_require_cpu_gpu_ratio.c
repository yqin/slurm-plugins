/*
 * Copyright (c) 2016, Yong Qin <yong.qin@lbl.gov>. All rights reserved.
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
#include <slurm/slurm_errno.h>
#include "src/slurmctld/slurmctld.h"

/* Required by Slurm job_submit plugin interface. */
const char plugin_name[] = "Require CPU/GPU ratio";
const char plugin_type[] = "job_submit/require_cpu_gpu_ratio";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/* Global variables. */
const char *myname = "job_submit_require_cpu_gpu_ratio";
/* The Partition that needs to be checked - need to modify. */
const char *mypart = "c_shared";
/* The CPU/GPU ratio that is checked against - need to modify. */
const int  ratio = 2;


/* Convert string to integer. */
int _str2int (char * str, uint32_t *p2int) {
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
    if (strcmp(part, mypart) == 0) {
        /* Require GRES on a GRES partition. */
        if (gres == NULL) {
            info("%s: missed GRES on partition %s", myname, mypart);
            return ESLURM_INVALID_GRES;

        /* Require GPU on a GRES partition. */
        } else if (strncmp(gres, "gpu:", 4) != 0) {
            info("%s: missed GPU on partition %s", myname, mypart); 
            return ESLURM_INVALID_GRES;

        /* Check CPU/GPU ratio. */
        } else {
            /* Number of GPUs. */
            uint32_t ngpu = 0;

            /* Requested GPU number from the job. */
            int sl = strlen(gres);

            /* Assume GPU number is the last digit of the GRES option. */
            if (_str2int(gres + sl - 1, &ngpu)) {
                info("%s: invalid GPU number %s", myname, gres + sl - 1);
                return ESLURM_INVALID_GRES;
            }

            /* Sanity check of the CPU/GPU ratio. */
            if (ncpu / ngpu < ratio) {
                info("%s: CPU=%ul, GPU=%ul, not qualify", myname, ncpu, ngpu);
                return ESLURM_INVALID_GRES;
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
