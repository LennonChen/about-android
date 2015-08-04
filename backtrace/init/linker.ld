/* Tell the linker that the entry point is _start(defined in startup.s) */
ENTRY(_start)
SECTIONS
{
/* The qemu emulator, when executed with the -kernel option,
** starts execution from the address 0x10000, so we must place
** our code at this address
*/
. = 0x10000;
/* In this, startup.o forms the text(code) part, then comes the subsequent
** data and the bss part.
*/
startup.o : {startup.o(.text)}
.data : {*(.data)}
.bss : {*(.bss)}
. = . + 0x500;
/* The final step is to define the address of the stack pointer.
** The stack usually grows downward, so it's better to give it a
** safe address. We have a very small code snippet, and can place
** the stack at 0x500 ahead of the current position. The variable sp_top
** will store the address for the stack.
*/
sp_top = .;
}