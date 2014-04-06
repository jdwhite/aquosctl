CC=gcc

aquosctl: aquosctl.c
	$(CC) -o aquosctl aquosctl.c

aquosctl-new: aquosctl.c
	$(CC) -DNEWER_PROTOCOL -o aquosctl aquosctl.c

clean:
	rm -f aquosctl

