#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "ad7792_drv.h"

#define REFERENCE_VOLTAGE   2.5


int main(int argc, char ** argv)
{
    char read_buf[3] ={0} ;
    int fd, ret ,i;
    unsigned int data;
    float sensor_data;
    fd = open("/dev/ad7792", O_RDWR);
    if(fd < 0 ){
        printf("can't open /dev/ad7792\n");
        return -1;
    }
    while(1)
    {
        //ioctl(fd, AD7792_CHANNEL1_CMD, read_buf);
        ret = read(fd, read_buf, sizeof(read_buf));
       // printf("ret = %d\n", ret);
        data =((unsigned int)read_buf[0] << 8) + read_buf[1]; 
        sensor_data = ((float)data)/65535 * REFERENCE_VOLTAGE;

        printf("get sensor_data is %f\n",sensor_data);
        
        usleep(100000); /*100ms */
    }

    close(fd);
    return 0;
    
}