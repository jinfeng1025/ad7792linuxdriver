#ifndef __AD7792_DRV_H_
#define __AD7792_DRV_H_


#include <linux/ioctl.h>
#include <linux/types.h>







#define SCLK_L  AD7792_Io_Low(GPIO4_DR, 25)
#define SCLK_H  AD7792_Io_Hight(GPIO4_DR, 25)
#define CS_L    AD7792_Io_Low(GPIO4_DR, 26)
#define CS_H    AD7792_Io_Hight(GPIO4_DR,26)     
#define DIN_0   AD7792_Io_Low(GPIO4_DR, 27)
#define DIN_1   AD7792_Io_Hight(GPIO4_DR, 27)

#define  AD7792_CHANNEL_BASE   'E'
#define AD7792_CHANNEL1_CMD    _IOR(AD7792_CHANNEL_BASE, 0, unsigned char)
#define AD7792_CHANNEL2_CMD    _IOR(AD7792_CHANNEL_BASE, 1, unsigned char)


#endif