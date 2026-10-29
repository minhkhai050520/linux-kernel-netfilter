#include "myfw_hashtable.h"
#include "myfw_spinlock.h"
#include "myfw_rcu_api.h"

u8 lookup_rule_rcu(const struct fw_tuple *tuple)
{
    struct fw_rule_node *rule;
    u32 key = get_rule_hash(tuple->src_ip);
    u8 action = FW_ACTION_ACCEPT;

    rcu_read_lock();

    hash_for_each_possible_rcu(fw_rule_htable, rule, node, key) {

        if ((rule->src_ip == tuple->src_ip) &&
            (rule->dst_ip == tuple->dst_ip || rule->dst_ip == 0) &&
            (rule->src_port == tuple->src_port || rule->src_port == 0) &&
            (rule->dst_port == tuple->dst_port || rule->dst_port == 0) &&
            (rule->proto == tuple->proto || rule->proto == 0)) {

                action = rule->action;
                break;
        }
    }

    rcu_read_unlock();

    return action;
}

int add_rule_rcu(const struct fw_tuple *tuple)
{
    struct fw_rule_node *new_rule;
    u32 key = get_rule_hash(tuple->src_ip);

    new_rule = kmalloc(sizeof(struct fw_rule_node), GFP_KERNEL);
    if (!new_rule)
        return -ENOMEM;

    new_rule->src_ip = tuple->src_ip;
    new_rule->dst_ip = tuple->dst_ip;
    new_rule->src_port = tuple->src_port;
    new_rule->dst_port = tuple->dst_port;
    new_rule->proto = tuple->proto;
    new_rule->action = tuple->action;

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

int del_rule_rcu(const struct fw_tuple *tuple)
{
    struct fw_rule_node *del_rule;
    struct hlist_node *tmp;
    u32 key = get_rule_hash(tuple->src_ip);

    spin_lock_bh(&myfw_write_lock);
    hash_for_each_possible_safe(fw_rule_htable, del_rule, tmp, node, key) {
        if ((del_rule->src_ip == tuple->src_ip) &&
            (del_rule->dst_ip == tuple->dst_ip || del_rule->dst_ip == 0) &&
            (del_rule->src_port == tuple->src_port || del_rule->src_port == 0) &&
            (del_rule->dst_port == tuple->dst_port || del_rule->dst_port == 0) &&
            (del_rule->proto == tuple->proto || del_rule->proto == 0)) {

                hash_del_rcu(&del_rule->node);
                kfree_rcu(del_rule, rcu);

                pr_info("[MYFW] Rule deleted: %pI4:%u -> %pI4:%u, proto: %u\n",
                        &tuple->src_ip, ntohs(tuple->src_port),
                        &tuple->dst_ip, ntohs(tuple->dst_port),
                        tuple->proto);

                return 0;
        }
    }
    spin_unlock_bh(&myfw_write_lock);

    return -ENOENT;
}

void clean_hashtable_rcu(void)
{
    struct fw_rule_node *rule;
    struct hlist_node *tmp;
    int bkt;

    spin_lock_bh(&myfw_write_lock);
    hash_for_each_safe(fw_rule_htable, bkt, tmp, rule, node) {
        hash_del_rcu(&rule->node);
        kfree_rcu(rule, rcu);
    }
    spin_unlock_bh(&myfw_write_lock);

    pr_info("[MYFW] Hashtable cleaned up.\n");
}