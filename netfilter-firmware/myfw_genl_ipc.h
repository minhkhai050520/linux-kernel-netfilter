#ifndef MYFW_GENL_IPC_H
#define MYFW_GENL_IPC_H

enum myfw_commands {
    MYFW_CMD_UNSPEC,
    MYFW_CMD_ADD_RULE,
    MYFW_CMD_DEL_RULE,
    MYFW_CMD_GET_RULES,
    MYFW_CMD_GET_STATS,
    MYFW_CMD_RESET_STATS,
    __MYFW_CMD_MAX,
};
#define MYFW_CMD_MAX (__MYFW_CMD_MAX - 1)

enum myfw_attrs {
    MYFW_ATTR_UNSPEC,
    MYFW_ATTR_SRC_IP,
    MYFW_ATTR_DST_IP,
    MYFW_ATTR_SRC_PORT,
    MYFW_ATTR_DST_PORT,
    MYFW_ATTR_PROTO,
    MYFW_ATTR_ACTION,
    MYFW_ATTR_RX_PACKETS,
    MYFW_ATTR_RX_BYTES,
    MYFW_ATTR_DROPPED_PACKETS,
    MYFW_ATTR_PAD,
    __MYFW_ATTR_MAX,
};
#define MYFW_ATTR_MAX (__MYFW_ATTR_MAX - 1)

int myfw_init_netlink(void);
void myfw_unregister_netlink(void);

#endif /* MYFW_GENL_IPC_H */