#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define LED0_GPIO 17
#define LED1_GPIO 27
#define LED2_GPIO 22

static bool req0, req1, req2;

static ssize_t led_write(struct file *file,
                         const char __user *buf,
                         size_t count,
                         loff_t *ppos)
{
    char kbuf[4] = {0};
    int mask;

    if (count < 1) return -EINVAL;
    if (count > sizeof(kbuf)-1) count = sizeof(kbuf)-1;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;

    mask = kbuf[0] - '0';

    /* 첫 write 시점에 동적 gpio_request */
    if (!req0) {
        int r = gpio_request(LED0_GPIO, "LED0");
        if (r==0) { gpio_direction_output(LED0_GPIO,0); req0=true; }
    }
    if (!req1) {
        int r = gpio_request(LED1_GPIO, "LED1");
        if (r==0) { gpio_direction_output(LED1_GPIO,0); req1=true; }
    }
    if (!req2) {
        int r = gpio_request(LED2_GPIO, "LED2");
        if (r==0) { gpio_direction_output(LED2_GPIO,0); req2=true; }
    }

    if (req0) gpio_set_value(LED0_GPIO, (mask&0x1)?1:0);
    if (req1) gpio_set_value(LED1_GPIO, (mask&0x2)?1:0);
    if (req2) gpio_set_value(LED2_GPIO, (mask&0x4)?1:0);

    return count;
}

static const struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .write = led_write,
};

static struct miscdevice led_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "led_control",
    .fops  = &led_fops,
};

static int __init led_init(void)
{
    int ret = misc_register(&led_dev);
    if (ret) return ret;
    pr_info("led_control: registered\n");
    return 0;
}

static void __exit led_exit(void)
{
    misc_deregister(&led_dev);
    if (req0) gpio_free(LED0_GPIO);
    if (req1) gpio_free(LED1_GPIO);
    if (req2) gpio_free(LED2_GPIO);
    pr_info("led_control: unloaded\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Dynamic GPIO-request LED control");

