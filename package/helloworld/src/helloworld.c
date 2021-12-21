#include <linux/module.h>
#include <linux/kernel.h>

//insmod会执行该函数
static int __init helloworld_init(void)
{
    int ret;
    printk("helloworld driver init !\n");
    return ret;
}
//rmmod会执行该函数
static void __exit helloworld_exit(void)
{
    printk("helloworld driver exit !\n");
}

//声明出入口函数
module_init(helloworld_init);
module_exit(helloworld_exit);
MODULE_AUTHOR("YANG");
MODULE_LICENSE("GPL");


