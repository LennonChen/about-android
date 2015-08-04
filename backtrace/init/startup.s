# The next step is to develop the startup code for the processor.
# When a processor is powered on, it jumps to a specified location,
# read code from that location, and executes it. Even in the case of
# a reset(like on a desktop machine), the processor jumps to a predefined
# location.

# _start is declared as global. The next line is the beginning of
# _start's code.
.global _start
_start:
        # We set the address of the stack to sp_top.
        # ldr will move the value of sp_top in the stack pointer sp.
        ldr sp,=sp_top
        # bl will jump to init
        bl init
        # then the processor will stop into an infinite loop with
        # the instruction "b ." which is like a while (1) or for (;;) loop
        # if we don't do this, our system will crash. The basics of
        # embedded systems programming is that our code should run into an
        # infinite loop.
        b .
