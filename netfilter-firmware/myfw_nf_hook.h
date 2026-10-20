#ifndef __MYFW_NF_HOOK_H
#define __MYFW_NF_HOOK_H

#include "myfw_hashtable.h"
#include "myfw_rcu_api.h"

unsigned int ipv4_nf_hook_fn(void *priv,
			      struct sk_buff *skb,
			      const struct nf_hook_state *state);

#endif /* __MYFW_NF_HOOK_H */