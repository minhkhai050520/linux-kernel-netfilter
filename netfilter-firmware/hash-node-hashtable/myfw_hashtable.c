#include "myfw_hashtable.h"

DEFINE_READ_MOSTLY_HASHTABLE(fw_rule_htable, FW_RULE_HTABLE_BITS);

u32 get_rule_hash(u32 src_ip)
{
    return jhash_1word(src_ip, 0x9e3779b9);
}