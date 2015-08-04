/* This test case use study linker works way.
 *
 * Our first test program is a bare-metal program running directlyo
 * on the processor, without the help of a bootloader.
 */

/* Get the address of UART0 in qemu:
 *   cd qemu/hw/arm/ && cat versatilepb.c | grep UART0
 * UART0 is present at address 0x101f1000. we can write data directly to this
 * address, and check output on the terminal. we decalred a volatile variable
 * pointer and assigned the3 address of serial port UART0.
 */
volatile unsigned char* const UART0_ADDR = (unsigned char*)0x0101f1000;

void display(const char* str)
{
    while (*str != '\0') {
        *UART0_ADDR = *str;
        str++;
    }
}

/* init is the main routine, it merely calls the function display(),
 * which writes a string to the UART0.
 */
int init(void)
{
    display("hello world\n");
}
