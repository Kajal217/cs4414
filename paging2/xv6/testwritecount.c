#include "types.h"
#include "stat.h"
#include "user.h"

// test the writecount system call and print the result
int
main(int argc, char *argv[])
{
  int wc = writecount();
  printf(1, "write count: %d\n", wc);
  exit();
}
