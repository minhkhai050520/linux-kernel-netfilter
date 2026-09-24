#ifndef __MYFW_RCU_API_H
#define __MYFW_RCU_API_H

#include "myfw_hashtable.h"

u8 lookup_rule_rcu(const struct fw_tuple *tuple);
#endif