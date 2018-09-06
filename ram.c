#include <sys/sysinfo.h>
#include <stdio.h>

int main() {
  
  struct sysinfo sys_info;

  if(sysinfo(&sys_info) != 0)
    perror("sysinfo");

  // Total and free ram.
  printf("Total RAM: %ldk\n", sys_info.totalram / 1024);
  printf("Free RAM: %ldk\n", sys_info.freeram / 1024);
  
  // Shared and buffered ram.
  printf("Shared RAM: %ldk\n", sys_info.sharedram / 1024);
  printf("Buffered RAM: %ldk\n", sys_info.bufferram / 1024);

  return 0;
}
