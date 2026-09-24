#include "myfw_rcu_api.h"

u8 lookup_rule_rcu(const struct fw_tuple *tuple)
{
    struct fw_rule_node *rule;
    u32 key = get_rule_hash(tuple->src_ip);
    u8 action = FW_ACTION_ACCEPT;

    rcu_read_lock();

    hash_for_each_possible_rcu(fw_rule_htable, rule, node, key) {
        if (rule->src_ip == tuple->src_ip &&
            rule->dst_ip == tuple->dst_ip &&
            rule->src_port == tuple->src_port &&
            rule->dst_port == tuple->dst_port &&
            rule->proto == tuple->proto) {

                action = rule->action;
                break;
        }
    }

    rcu_read_unlock();

    return action;
}