a.out : m.o a.o
	arm-none-linux-gnueabi-gcc m.o a.o -o a.out

m.o : m.c
	arm-none-linux-gnueabi-gcc -g -c m.c -o m.o

a.o : a.c
	arm-none-linux-gnueabi-gcc -g -c a.c -o a.o

.PYHON: clean
clean:
	$(RM) *.o *.out
