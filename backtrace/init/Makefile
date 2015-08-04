a.out : init.o startup.o
	arm-none-eabi-ld -T linker.ld init.o startup.o -o a.elf

init.o : init.c
	arm-none-eabi-gcc -c -mcpu=arm926ej-s init.c -o init.o

startup.o : startup.s
	arm-none-eabi-as -mcpu=arm926ej-s startup.s -o startup.o

.PHONY : all
all : a.out
	arm-none-eabi-objcopy -O binary a.elf a.bin

.PHONY : clean
clean:
	$(RM) *.o *.elf *.bin