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

int add_rule_rcu(const struct fw_rule_node *rule)
{
    struct fw_rule_node *new_rule;
    u32 key = get_rule_hash(rule->src_ip);

    new_rule = kmalloc(sizeof(struct fw_rule_node), GFP_KERNEL);
    if (!new_rule)
        return -ENOMEM;

    new_rule->src_ip = rule->src_ip;
    new_rule->dst_ip = rule->dst_ip;
    new_rule->src_port = rule->src_port;
    new_rule->dst_port = rule->dst_port;
    new_rule->proto = rule->proto;
    new_rule->action = rule->action;

    spin_lock_bh(&myfw_write_lock);
    hash_add_rcu(fw_rule_htable, &new_rule->node, key);
    spin_unlock_bh(&myfw_write_lock);

    pr_info("[MYFW] Rule added: %pI4:%u -> %pI4:%u, proto: %u, action: %s\n",
            &new_rule->src_ip, ntohs(new_rule->src_port),
            &new_rule->dst_ip, ntohs(new_rule->dst_port),
            new_rule->proto,
            new_rule->action == FW_ACTION_DROP ? "DROP" : "ACCEPT");

    return 0;
}