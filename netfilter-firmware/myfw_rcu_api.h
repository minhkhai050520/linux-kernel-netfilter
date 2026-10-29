#ifndef __MYFW_RCU_API_H
#define __MYFW_RCU_API_H

u8 lookup_rule_rcu(const struct fw_tuple *tuple);
int add_rule_rcu(const struct fw_tuple *tuple);
int del_rule_rcu(const struct fw_tuple *tuple);
void clean_hashtable_rcu(void);

#endif /* __MYFW_RCU_API_H */