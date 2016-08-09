all:	spank_demo.so spank_get_jobscript.so spank_private_tmpshm.so

spank_demo.so: spank_demo.c
	gcc -shared -fPIC -o spank_demo.so spank_demo.c

spank_get_jobscript.so: spank_get_jobscript.c
	gcc -shared -fPIC -o spank_get_jobscript.so spank_get_jobscript.c

spank_private_tmpshm.so: spank_private_tmpshm.c spank_private_tmpshm_rmrf.c
	gcc -shared -fPIC -o spank_private_tmpshm.so spank_private_tmpshm.c spank_private_tmpshm_rmrf.c

clean:
	rm -rf spank_demo.so spank_get_jobscript.so spank_private_tmpshm.so
