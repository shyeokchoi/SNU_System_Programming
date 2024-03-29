#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/stat.h>
#include <linux/slab.h>

#define BUFSIZE 2048

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;
static char *buf;

struct process_item {
    pid_t pid;
    char process_name[16];
    struct list_head list;
};

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
    pid_t input_pid;
	int cursor = 0;
	int ret;

    sscanf(user_buffer, "%u", &input_pid);

	if (!(curr = pid_task(find_get_pid(input_pid), PIDTYPE_PID))) 
		return -ESRCH;

	LIST_HEAD(task_list);

    while(curr->pid) {
		struct process_item *item = kmalloc(sizeof(struct process_item), GFP_KERNEL);
		item->pid = curr->pid;
		strcpy(item->process_name, curr->comm);
		list_add(&item->list, &task_list);
		curr = curr->real_parent;
	}

	struct process_item *pos;
	struct process_item *temp;

	list_for_each_entry_safe(pos, temp, &task_list, list) {
		cursor += sprintf(buf + cursor, "%s (%d)\n", pos->process_name, pos->pid);
		list_del(&pos->list);
		kfree(pos);
	}

    return cursor;
}

static ssize_t read_ptree(struct file *fp, 
                                char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{   
    return simple_read_from_buffer(user_buffer, length, position, buf, strlen(buf));
}

static const struct file_operations dbfs_fops = {
    .write = write_pid_to_input,
    .read = read_ptree
};


static int __init dbfs_module_init(void)
{
    if (!(dir = debugfs_create_dir("ptree", NULL))) {
        printk("Failed to create ptree dir\n");
        return -1;
    }

    if (!(inputdir = debugfs_create_file("input", S_IRWXU|S_IRWXG|S_IRWXO, dir, NULL, &dbfs_fops))) {
        printk("Failed to create input file\n");
        return -1;
    }

    if (!(ptreedir = debugfs_create_file("ptree", S_IRWXU|S_IRWXG|S_IRWXO, dir, NULL, &dbfs_fops))) {
        printk("Failed to create ptree file\n");
        return -1;
    }

	printk("dbfs_ptree module initialize done\n");
    buf = kmalloc(BUFSIZE, GFP_KERNEL);
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);
    kfree(buf);
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
