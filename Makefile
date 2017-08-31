job_submit_plugins = job_submit_collect_script.so job_submit_require_cpu_gpu_ratio.so
spank_plugins = spank_demo.so spank_collect_script.so spank_private_tmpshm.so


job_submit_collect_script.so: job_submit_collect_script.c
	gcc -g -shared -fPIC -pthread job_submit_collect_script.c -o job_submit_collect_script.so

job_submit_require_cpu_gpu_ratio.so: job_submit_require_cpu_gpu_ratio.c
	gcc -g -shared -fPIC -pthread job_submit_require_cpu_gpu_ratio.c -o job_submit_require_cpu_gpu_ratio.so

spank_demo.so: spank_demo.c
	gcc -g -shared -fPIC -o spank_demo.so spank_demo.c

spank_collect_script.so: spank_collect_script.c
	gcc -g -shared -fPIC -o spank_collect_script.so spank_collect_script.c

spank_private_tmpshm.so: spank_private_tmpshm.c
	gcc -g -shared -fPIC -o spank_private_tmpshm.so spank_private_tmpshm.c


job_submit: $(job_submit_plugins)
spank:	$(spank_plugins)
all:	$(job_submit_plugins) $(spank_plugins)

clean:
	rm -rf $(job_submit_plugins) $(spank_plugins)
