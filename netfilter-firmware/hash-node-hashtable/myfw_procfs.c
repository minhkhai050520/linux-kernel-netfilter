#include "myfw_procfs.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static struct proc_dir_entry *myfw_proc_entry;

static int myfw_stats_show(struct seq_file *m, void *v)
{
    struct nft_counter_tot total;

    myfw_counter_do_dump(&myfw_priv, &total, false);

    seq_printf(m, "Rx Packets   : %lld\n", (long long)total.rx_packets);
    seq_printf(m, "Rx Bytes     : %lld\n", (long long)total.rx_bytes);
    seq_printf(m, "Dropped Pkts : %lld\n", (long long)total.dropped_packets);

    return 0;
}

static int myfw_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, myfw_stats_show, NULL);
}

static ssize_t myfw_stats_write(struct file *file, const char __user *buffer,
                                size_t count, loff_t *ppos)
{
    struct nft_counter_tot dummy;
    myfw_counter_do_dump(&myfw_priv, &dummy, true);
    return count;
}

static const struct proc_ops myfw_proc_ops = {
    .proc_open    = myfw_stats_open,
    .proc_read    = seq_read,
    .proc_write   = myfw_stats_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int myfw_init_procfs(void)
{
    myfw_proc_entry = proc_create(PROC_FILENAME, 0644, NULL, &myfw_proc_ops);
    if (!myfw_proc_entry)
        return -ENOMEM;
    return 0;
}

void myfw_cleanup_procfs(void)
{
    if (myfw_proc_entry)
        remove_proc_entry(PROC_FILENAME, NULL);
}