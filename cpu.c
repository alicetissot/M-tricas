#include <sys/sysinfo.h>
#include <stdio.h>

int main() {
  int days, hours, mins;
  struct sysinfo sys_info;

  if(sysinfo(&sys_info) != 0)
    perror("sysinfo");

  // Uptime
  days = sys_info.uptime / 86400;
  hours = (sys_info.uptime / 3600) - (days * 24);
  mins = (sys_info.uptime / 60) - (days * 1440) - (hours * 60);

  // Load Averages for 1,5 and 15 minutes
  printf("Load Avgs: 1min(%ld) 5min(%ld) 15min(%ld)\n", sys_info.loads[0], sys_info.loads[1], sys_info.loads[2]);
  
  // Tempo que o processador tá funcionando desde que foi inicializado.
  printf("Tempo em execucao: %ddias, %dhoras, %dminutos, %ldsegundos\n", days, hours, mins, sys_info.uptime % 60);

  // Number of processes currently running.
  printf("Numero de processos: %d\n", sys_info.procs);

  return 0;
}
