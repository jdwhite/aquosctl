CC=gcc

aquosctl: aquosctl.c
	$(CC) -o aquosctl aquosctl.c

clean:
	rm -f aquosctl

