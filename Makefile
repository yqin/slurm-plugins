SLURM_SRC_DIR = /global/home/users/yqin/applications/slurm

job_submit_plugins = job_submit_collect_script.so job_submit_require_cpu_gpu_ratio.so
spank_plugins = spank_demo.so spank_collect_script.so spank_private_tmpshm.so


job_submit_collect_script.so: job_submit_collect_script.c
	gcc -shared -fPIC -pthread -I$(SLURM_SRC_DIR) job_submit_collect_script.c -o job_submit_collect_script.so

job_submit_require_cpu_gpu_ratio.so: job_submit_require_cpu_gpu_ratio.c
	gcc -shared -fPIC -pthread -I$(SLURM_SRC_DIR) job_submit_require_cpu_gpu_ratio.c -o job_submit_require_cpu_gpu_ratio.so

spank_demo.so: spank_demo.c
	gcc -shared -fPIC -o spank_demo.so spank_demo.c

spank_collect_script.so: spank_collect_script.c
	gcc -shared -fPIC -o spank_collect_script.so spank_collect_script.c

spank_private_tmpshm.so: spank_private_tmpshm.c spank_private_tmpshm_rmrf.c
	gcc -shared -fPIC -o spank_private_tmpshm.so spank_private_tmpshm.c spank_private_tmpshm_rmrf.c


job_submit: $(job_submit_plugins)
spank:	$(spank_plugins)
all:	$(job_submit_plugins) $(spank_plugins)

clean:
	rm -rf $(job_submit_plugins) $(spank_plugins)
