#include "myfw_spinlock.h"
#include <linux/spinlock_types.h>

DEFINE_SPINLOCK(myfw_write_lock);