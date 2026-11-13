#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
int main(void){ int fd=open("/dev/input/event0",O_RDONLY); struct input_absinfo a;
 int c[]={ABS_X,ABS_Y,ABS_MT_POSITION_X,ABS_MT_POSITION_Y}; const char*n[]={"ABS_X","ABS_Y","MT_X","MT_Y"};
 for(int i=0;i<4;i++){ if(ioctl(fd,EVIOCGABS(c[i]),&a)==0) printf("%s min %d max %d\n",n[i],a.minimum,a.maximum); else printf("%s -\n",n[i]); } return 0; }
