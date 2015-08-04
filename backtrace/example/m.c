extern void a(char* str);

int main(int argc, char**argv)
{
    static char str[] = "hello world\n";
    a(str);
}
