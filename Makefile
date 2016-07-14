all:	getjobscript.so tmpshm.so

getjobscript.so: getjobscript.c
	gcc -shared -fPIC -o getjobscript.so getjobscript.c

tmpshm.so: rmrf.c tmpshm.c
	gcc -shared -fPIC -o tmpshm.so rmrf.c tmpshm.c

clean:
	rm -rf getjobscript.so tmpshm.so
