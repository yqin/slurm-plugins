all:	tmpshm.so

tmpshm.so: rmrf.c tmpshm.c
	gcc -shared -fPIC -o tmpshm.so rmrf.c tmpshm.c

clean:
	rm -rf tmpshm.so
