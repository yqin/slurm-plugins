all:	getjobscript.so spank_demo.so tmpshm.so

getjobscript.so: getjobscript.c
	gcc -shared -fPIC -o getjobscript.so getjobscript.c

spank_demo.so: spank_demo.c
	gcc -shared -fPIC -o spank_demo.so spank_demo.c

tmpshm.so: rmrf.c tmpshm.c
	gcc -shared -fPIC -o tmpshm.so rmrf.c tmpshm.c

clean:
	rm -rf getjobscript.so tmpshm.so
