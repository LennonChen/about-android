#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define SIMULATOR_DEVICE_NAME "/dev/simulator_main"

int
main( int argc, char** argv )
{
    int fd  = -1;
    int val = 0;

    fd = open( SIMULATOR_DEVICE_NAME, 0666, O_RDWR );
    if ( fd == -1 ) {
        printf( "Failed to open device %s.\n", SIMULATOR_DEVICE_NAME );
        return -1;
    }

    printf( "Read original value:\n" );
    read( fd, &val, sizeof (val) );
    printf( "%d.\n\n", val );

    val = 5;
    printf( "Write value %d to %s.\n\n", val, SIMULATOR_DEVICE_NAME );
    write( fd, &val, sizeof (val) );

    printf( "Read the value again:\n" );
    read( fd, &val, sizeof (val) );
    printf( "%d.\n\n", val );

    close( fd );

    return 0;
}
