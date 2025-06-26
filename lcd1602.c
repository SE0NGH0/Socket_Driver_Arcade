// File: lcd1602.c
// I2C LCD1602 driver using alloc_chrdev_region + cdev_add for dynamic major/minor
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define LCD_ADDR       0x27
#define LCD_BACKLIGHT  (1<<3)
#define LCD_ENABLE     (1<<2)
#define LCD_RS         (1<<0)

static struct i2c_client *lcd_client;
static dev_t lcd_dev;
static struct cdev lcd_cdev;

// Enable pulse
static void pulse_enable(u8 data)
{
    u8 tmp = data | LCD_ENABLE;
    i2c_master_send(lcd_client, &tmp, 1);
    udelay(1);
    tmp = data & ~LCD_ENABLE;
    i2c_master_send(lcd_client, &tmp, 1);
    udelay(50);
}

// Write nibble
static void write4(u8 nibble, u8 ctrl)
{
    u8 data = (nibble & 0xF0) | ctrl | LCD_BACKLIGHT;
    i2c_master_send(lcd_client, &data, 1);
    pulse_enable(data);
}

// Send command/data
static void lcd_send(u8 val, u8 rs)
{
    write4(val, 0);
    write4(val << 4, rs);
}

static void lcd_cmd(u8 cmd)
{ lcd_send(cmd, 0); }

static void lcd_data(u8 d)
{ lcd_send(d, LCD_RS); }

// Init sequence
static void lcd_init_sequence(void)
{
    msleep(50);
    lcd_cmd(0x33); msleep(5);
    lcd_cmd(0x32); msleep(5);
    lcd_cmd(0x28); msleep(1);
    lcd_cmd(0x0C); msleep(1);
    lcd_cmd(0x06); msleep(1);
    lcd_cmd(0x01); msleep(2);
}

// Write file operation
static ssize_t lcd_write(struct file *filp, const char __user *buf,
                         size_t count, loff_t *f_pos)
{
    char kbuf[33];
    size_t len = min(count, (size_t)32);
    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    lcd_cmd(0x01);
    msleep(2);
    for (size_t i = 0; i < len && i < 16; i++)
        lcd_data(kbuf[i]);
    if (len > 16) {
        lcd_cmd(0xC0);
        for (size_t i = 16; i < len && i < 32; i++)
            lcd_data(kbuf[i]);
    }
    return len;
}

static const struct file_operations lcd_fops = {
    .owner = THIS_MODULE,
    .write = lcd_write,
};

static int __init lcd_init_module(void)
{
    struct i2c_adapter *adap;
    struct i2c_board_info info = {
        .type = "lcd1602",
        .addr = LCD_ADDR,
    };
    int ret;

    // Allocate char device region
    ret = alloc_chrdev_region(&lcd_dev, 0, 1, "lcd1602");
    if (ret) {
        pr_err("lcd1602: alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }
    cdev_init(&lcd_cdev, &lcd_fops);
    lcd_cdev.owner = THIS_MODULE;
    ret = cdev_add(&lcd_cdev, lcd_dev, 1);
    if (ret) {
        pr_err("lcd1602: cdev_add failed: %d\n", ret);
        unregister_chrdev_region(lcd_dev, 1);
        return ret;
    }
    pr_info("lcd1602: char device registered (major=%d, minor=%d)\n",
            MAJOR(lcd_dev), MINOR(lcd_dev));

    // Register I2C client
    adap = i2c_get_adapter(1);
    if (!adap) {
        pr_err("lcd1602: i2c_get_adapter failed\n");
        cdev_del(&lcd_cdev);
        unregister_chrdev_region(lcd_dev, 1);
        return -ENODEV;
    }
    lcd_client = i2c_new_client_device(adap, &info);
    i2c_put_adapter(adap);
    if (IS_ERR(lcd_client)) {
        pr_err("lcd1602: i2c_new_client_device failed\n");
        cdev_del(&lcd_cdev);
        unregister_chrdev_region(lcd_dev, 1);
        return PTR_ERR(lcd_client);
    }

    // Initialize LCD
    lcd_init_sequence();
    pr_info("lcd1602: LCD initialized\n");
    return 0;
}

static void __exit lcd_exit_module(void)
{
    i2c_unregister_device(lcd_client);
    cdev_del(&lcd_cdev);
    unregister_chrdev_region(lcd_dev, 1);
    pr_info("lcd1602: module exited\n");
}

module_init(lcd_init_module);
module_exit(lcd_exit_module);

MODULE_AUTHOR("hotari");
MODULE_DESCRIPTION("I2C LCD1602 driver with dynamic major via alloc_chrdev_region");
MODULE_LICENSE("GPL");
