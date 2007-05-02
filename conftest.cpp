#include <unistd.h>
int main (int argc, char *argv[])
{
   char *test = crypt("abcdefg","aB");
   return 1;
}
