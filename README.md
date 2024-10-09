This is a set of [Slurm](http://slurm.schedmd.com/) plugins that are used on [LBNL HPCS](http://lrc.lbl.gov/) clusters.

To build Job Submit plugins requires [Slurm source code](https://github.com/SchedMD/slurm) and Makefile should be modified to point to the source code location.

1. job_submit_collect_script: A Job Submit plugin to collect job scripts on the fly and save them to a designated location. You will need to define your own location to save the job scripts.  (BUGGY DON'T USE YET)

2. job_submit_require_cpu_gpu_ratio: A Job Submit plugin to verify the CPU/GPU ratio on a particular partition. You will need to define your own partition and ratio in the source code.

3. spank_demo: A SPANK plugin to demonstrate various callback functions.

4. spank_collect_script: A SPANK plugin to collect job script on the fly and save it to a shared location.

5. spank_private_tmpshm: A SPANK plugin to create per-job private /tmp and /dev/shm directories and to clean them after the job completes.
