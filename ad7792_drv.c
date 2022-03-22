#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "ad7792_drv.h"


#define AD7792_NAME    "ad7792"
#define AD7792_COUNT     1



/* 寄存器物理地址 */
#define CCM_CCGR3_BASE				(0X020C4074)     		 /* 时钟 */
#define SW_MUX_CSI_DATA04_BASE      (0X020E01F4)             /* SCLK  4-25 */
#define SW_MUX_CSI_DATA05_BASE      (0X020E01F8)     		 /*CS     4-26*/
#define SW_MUX_CSI_DATA06_BASE      (0X020E01FC)	 		 /* MOSI  4-27*/
#define SW_MUX_CSI_DATA07_BASE      (0X020E0200)           /*MISO   4-28*/
#define SW_PAD_CSI_DATA04_BASE 		(0X020E0480)
#define SW_PAD_CSI_DATA05_BASE		(0X020E0484)
#define SW_PAD_CSI_DATA06_BASE      (0X020E0488)
#define SW_PAD_CSI_DATA07_BASE      (0X020E048C)
#define GPIO4_DR_BASE				(0X020A8000)
#define GPIO4_GDIR_BASE				(0X020A8004) 
#define GPIO4_PSR_BASE              (0X020A8008)

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR3;
static void __iomem *SW_MUX_CSI_DATA04;
static void __iomem *SW_PAD_CSI_DATA04;
static void __iomem *SW_MUX_CSI_DATA05;
static void __iomem *SW_PAD_CSI_DATA05;
static void __iomem *SW_MUX_CSI_DATA06;
static void __iomem *SW_PAD_CSI_DATA06;
static void __iomem *SW_MUX_CSI_DATA07;
static void __iomem *SW_PAD_CSI_DATA07;


static void __iomem *GPIO4_DR;
static void __iomem *GPIO4_GDIR;
static void __iomem *GPIO4_PSR;


unsigned int IO_BUFF = 0;
unsigned char DataRead[2]  ;
//unsigned char ReadBuf[2];
/*
GPIO4_25->AD SCLK
GPIO4_26->AD CS
GPIO4_27->AD MOSI
GPIO4_28->AD MISO
*/

struct ad7792_dev {

    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major ;
    int minor ;

};

struct ad7792_dev  ad7792;



void AD7792_Io_Hight(void __iomem *ADGPIO_DR, unsigned char GPIO_id)
{
	IO_BUFF = readl(ADGPIO_DR);
	IO_BUFF |= (1 << GPIO_id);
	writel(IO_BUFF, ADGPIO_DR);

}
void AD7792_Io_Low(void __iomem *ADGPIO_DR, unsigned char GPIO_id)
{
	IO_BUFF = readl(ADGPIO_DR);
	IO_BUFF &= ~(1 << GPIO_id);
	writel(IO_BUFF, ADGPIO_DR);

}
void AD7792_Reset(void)
{
	int ResetTime;
	ResetTime = 32;
	SCLK_H; 
	CS_L;
	DIN_1;
	while(ResetTime --)
	{
		udelay(1);
		SCLK_L;
		udelay(1);
		SCLK_H;
	}

	CS_H;
	udelay(50);
}

void WriteToReg(unsigned char ByteData)
{
	unsigned char temp;
	int  i;
	temp = 0x80;
	CS_L;
	for ( i = 0; i < 8; i++)
	{
		if((ByteData &temp) == 0){
			DIN_0;
		}else{	
			DIN_1;
		}
	
		SCLK_L; 
		udelay(1);

		SCLK_H; 
		udelay(1);
 		temp = temp>>1;
	}
	CS_H;
	DIN_1; /*---------------*/  
}

void ReadFromReg(unsigned char nByte)
{
	int i,j;
   	unsigned char temp;

	DIN_1;
	CS_L;  
    temp = 0;
	

	for(i=0; i<nByte; i++)
	{
		for(j=0; j<8; j++)
	    {
	     
			SCLK_L; 
			temp = temp<<1;
	     	if((readl(GPIO4_PSR)>>28) & 0x001)
	     		temp = temp + 0x01;
			udelay(1);
	      
			SCLK_H; 
			udelay(1);
		  }
		  DataRead[i] = temp;
		  temp = 0;
	}
    CS_H; 
	DIN_1; /*---------------*/ 
}

void ad7792_read_id(void)
{
	WriteToReg(0x60);
	ReadFromReg(1);
	printk("AD7792 ID = %#X\r\n", DataRead[0]);	
}

void AD7792_Convertion(unsigned char ADC_channel)
{
	WriteToReg(0x10);  //write to Communication register.The next step is writing to Configuration register
	WriteToReg(0x10);  //set the Configuration bipolar mode.Gain=1.select the ADC input range as 0~2.5
	WriteToReg(ADC_channel); //Configuration EXternal reference selected.

	WriteToReg(0x08);//write to Communication register.The next step is writing to Mode register.
    WriteToReg(0x20);//set the mode register as single conversion mode.
	WriteToReg(0x0A);//inter 64 kHZ clock.internal clock is not available at the clk pin.

	WriteToReg(0x40);  //write to Communication register.The next step is to read from Status register.
	ReadFromReg(1);
	while((DataRead[0] & 0x80)  == 0x80){  //wait for the end of convertion by polling the status register RDY bit
			WriteToReg(0x40); 
			ReadFromReg(1);	
	}
	WriteToReg(0x58);//write to Communication register.The next step is to read from Data register.
	ReadFromReg(2);	

}

static int ad7792_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &ad7792;
    
   return 0; 
}
static ssize_t ad7792_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int  ret ;
	unsigned char ReadBuf[2] ;
	
	AD7792_Convertion(0);
	memcpy(ReadBuf, DataRead,sizeof(DataRead));
	ret = copy_to_user(buf, ReadBuf, sizeof(ReadBuf));
	return ret;
}
static ssize_t ad7792_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	
    return 0;
}


long ad7792_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	/*
	int ret = 0;
	switch(cmd){		
		case AD7792_CHANNEL1_CMD:
			AD7792_Convertion(0);
			break;
		case AD7792_CHANNEL2_CMD:
		   AD7792_Convertion(1);
		default:
        ret = -1;
        break;
	}
	if (copy_to_user((void *)arg, DataRead, sizeof(DataRead)))
        return -1;

	return ret;
  */
 	return 0;
}


static int ad7792_release(struct inode *inode, struct file *filp)
{
    struct ad7792_dev *dev = filp->private_data;
	return 0;
}


static const struct file_operations ad7792_fops = {
	.owner = THIS_MODULE,
	.open = ad7792_open,
	.read = ad7792_read,
	.write = ad7792_write,
	.release = ad7792_release,
	.unlocked_ioctl = ad7792_ioctl,
};
static int __init ad7792_init(void)
{   
    int ret = 0;
    u32 val = 0;
    printk("ad7792 init \r\n");
	/* 初始化*/
	/* 1、寄存器地址映射 */

  	IMX6U_CCM_CCGR3 = ioremap(CCM_CCGR3_BASE, 4);
	SW_MUX_CSI_DATA04 = ioremap(SW_MUX_CSI_DATA04_BASE, 4);
	SW_PAD_CSI_DATA04 = ioremap(SW_PAD_CSI_DATA04_BASE, 4);
    SW_MUX_CSI_DATA05 = ioremap(SW_MUX_CSI_DATA05_BASE, 4);
	SW_PAD_CSI_DATA05 = ioremap(SW_PAD_CSI_DATA05_BASE, 4);
	SW_MUX_CSI_DATA06 = ioremap(SW_MUX_CSI_DATA06_BASE, 4);
	SW_PAD_CSI_DATA06 = ioremap(SW_PAD_CSI_DATA06_BASE, 4);
	SW_MUX_CSI_DATA07 = ioremap(SW_MUX_CSI_DATA07_BASE, 4);
	SW_PAD_CSI_DATA07 = ioremap(SW_PAD_CSI_DATA07_BASE, 4);


  
	GPIO4_DR = ioremap(GPIO4_DR_BASE, 4);
	GPIO4_GDIR = ioremap(GPIO4_GDIR_BASE, 4);
    GPIO4_PSR = ioremap(GPIO4_PSR_BASE, 4);

	/* 2、使能GPIO4时钟 */
	val = readl(IMX6U_CCM_CCGR3);
	val &= ~(3 << 12);	/* 清楚以前的设置 */
	val |= (3 << 12);	/* 设置cv 新值 */
	writel(val, IMX6U_CCM_CCGR3);

	/* 3、设置SW_MUX_CSI_DATA04-07、SW_PAD_CSI_DATA04-07、的复用功能，
	 *     将其复用为
	 *    GPIO，最后设置IO属性。
	 */
	writel(5, SW_MUX_CSI_DATA04);
	writel(5, SW_MUX_CSI_DATA05);
	writel(5, SW_MUX_CSI_DATA06);
	writel(5, SW_MUX_CSI_DATA07);

	/*寄存器设置IO属性
	 *bit 16:0 HYS关闭
	 *bit [15:14]: 00 默认下拉
     *bit [13]: 0 kepper功能
     *bit [12]: 1 pull/keeper使能
     *bit [11]: 0 关闭开路输出
     *bit [7:6]: 10 速度100Mhz
     *bit [5:3]: 110 R0/6驱动能力
     *bit [0]: 0 低转换率
	 */
	writel(0x10B0, SW_PAD_CSI_DATA04);
    writel(0x10B0, SW_PAD_CSI_DATA05);
	writel(0x10B0, SW_PAD_CSI_DATA06);
	writel(0x10B0, SW_PAD_CSI_DATA07);

	/* 4、设置GPIO4-25 sclk为输出功能 */
	val = readl(GPIO4_GDIR);
	val &= ~(1 << 25);	/* 清除以前的设置 */
	val |= (1 << 25);	/* 设置为输出 */
	writel(val, GPIO4_GDIR);


	/* 5、设置GPIO4-26 cs为输出功能 */
	val = readl(GPIO4_GDIR);
	val &= ~(1 << 26);	/* 清除以前的设置 */
	val |= (1 << 26);	/* 设置为输出 */															
	writel(val, GPIO4_GDIR);

	/* 6、设置GPIO4-27 mosi为输出功能 */
	val = readl(GPIO4_GDIR);
	val &= ~(1 << 27);	/* 清除以前的设置 */
	val |= (1 << 27);	/* 设置为输出 */
	writel(val, GPIO4_GDIR);

	/* 6、设置GPIO4-28 miso为输入功能 */
	val = readl(GPIO4_GDIR);
	val &= ~(1 << 28);	/* 设置为输入*/
	writel(val, GPIO4_GDIR);


	

    /* 注册设备号 */
    ad7792.major = 0;
    if(ad7792.major){   /*给定主设备号 */
        ad7792.devid = MKDEV(ad7792.major, 0);
        ret = register_chrdev_region(ad7792.devid, AD7792_COUNT, AD7792_NAME);
    }else{                /*没有给定主 设备号*/
        ret = alloc_chrdev_region(&ad7792.devid, 0, AD7792_COUNT, AD7792_NAME);
        ad7792.major = MAJOR(ad7792.devid);
        ad7792.minor = MINOR(ad7792.devid);
    }
    if(ret < 0){
         printk("chrdev region err !! \r\n ");
         goto fail_devid;
        }

    printk("ad7792 major = %d minor = %d \r\n", ad7792.major,ad7792.minor);
    /* 注册字符设备 */
    ad7792.cdev.owner = THIS_MODULE;
    cdev_init(&ad7792.cdev, &ad7792_fops);
    ret = cdev_add(&ad7792.cdev, ad7792.devid, AD7792_COUNT);
    if(ret < 0)
        goto fail_cdev;
    ad7792.class = class_create(THIS_MODULE, AD7792_NAME);
	if (IS_ERR(ad7792.class)){
        ret = PTR_ERR(ad7792.class);
        goto fail_class;
    }	
    ad7792.device = device_create(ad7792.class, NULL, ad7792.devid, NULL, AD7792_NAME);
	if (IS_ERR(ad7792.device)){
        ret = PTR_ERR(ad7792.device);
        goto fail_device;
    }
	AD7792_Reset();	 
	ad7792_read_id();
    return 0;
    
fail_device:
       class_destroy(ad7792.class); 
fail_class:
       cdev_del(&ad7792.cdev); 
fail_cdev:
       unregister_chrdev_region(ad7792.devid, AD7792_COUNT); 
fail_devid:
        return ret;

        
}
static void __exit ad7792_exit(void)
{   

    printk("ad7792 exit !! \r\n");
    /* 取消映射 */
	iounmap(IMX6U_CCM_CCGR3);
	iounmap(SW_MUX_CSI_DATA04);
	iounmap(SW_PAD_CSI_DATA04);
	iounmap(SW_MUX_CSI_DATA05);
	iounmap(SW_PAD_CSI_DATA05);
	iounmap(SW_MUX_CSI_DATA06);
	iounmap(SW_PAD_CSI_DATA06);
	iounmap(SW_MUX_CSI_DATA07);
	iounmap(SW_PAD_CSI_DATA07);	
	iounmap(GPIO4_DR);
	iounmap(GPIO4_GDIR);
	iounmap(GPIO4_PSR);

    /* 删除字符设备 */
    cdev_del(&ad7792.cdev);
    /* 注销设备号 */
    unregister_chrdev_region(ad7792.devid, AD7792_COUNT);

    /* 摧毁设备 */
    device_destroy(ad7792.class, ad7792.devid);
    /* 摧毁类 */
    class_destroy(ad7792.class);
}

/* 驱动入口 出口*/


module_init(ad7792_init);
module_exit(ad7792_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinfeng");