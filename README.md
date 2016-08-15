This is a set of [Slurm] (http://slurm.schedmd.com/ Slurm) plugins that are used on [LBNL HPCS] (http://lrc.lbl.gov/ LRC) clusters.

1. spank_demo: A SPANK plugin to demonstrate various callback functions.

2. spank_get_jobscript: A SPANK plugin to collect job script on the fly and store it to a shared location.

3. spank_private_tmpshm: A SPANK plugin to create per-job private /tmp and /dev/shm directories and to clean them after the job completes.

4. job_submit_require_cpu_gpu_ratio: A Job Submit plugin to verify the CPU/GPU ratio on a particular partition. You will need to define your own partition and ratio in the source code. Building this plugin requires [Slurm source code] (https://github.com/SchedMD/slurm Slurm source code) and Makefile should be modified to point to the source code location.
