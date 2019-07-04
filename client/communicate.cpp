/*************************************************************************
    > File Name: communicate.cpp
    > Created Time: Fri 21 Dec 2018 08:45:09 PM CST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<assert.h>
#include<fcntl.h>
#include<iostream>
#include"sem.h"
using namespace std;
int main()
{
	int semid = sem_init((key_t)2000);
	int fd=open("./FIFO",O_RDWR);
	assert(fd!=-1);
	while(1)
	{
		char buff[128]={0};
		if(0>=read(fd, buff, 127))
		{
			break;
		}
		sem_v(semid,0);
		cout<<"new message: "<<buff<<endl;
		cout<<"please reply: ";
		fflush(stdout);
		char buff1[128] = {0};
		cin>>buff1;
		if(strcmp(buff1,"end")==0)
		{
			break;
		}
		write(fd,buff1,strlen(buff1));
		sem_p(semid, 0);
	}
	sem_del(semid);
}
