#ifndef _MYFW_PROCFS_H_
#define _MYFW_PROCFS_H_

#include "myfw_stats.h"

#define PROC_FILENAME "myfw_stats"

int myfw_init_procfs(void);
void myfw_cleanup_procfs(void);

#endif /* _MYFW_PROCFS_H_ */