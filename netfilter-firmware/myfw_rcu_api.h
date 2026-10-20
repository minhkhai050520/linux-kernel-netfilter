#ifndef __MYFW_RCU_API_H
#define __MYFW_RCU_API_H

#include "myfw_hashtable.h"
#include "myfw_spinlock.h"

u8 lookup_rule_rcu(const struct fw_tuple *tuple);
int add_rule_rcu(const struct fw_rule_node *rule);
void clean_hashtable_rcu(void);

#endif /* __MYFW_RCU_API_H */