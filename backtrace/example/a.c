#include <unistd.h>
#include <string.h>

void a(char* str)
{
    write(1, str, strlen(str));
}
