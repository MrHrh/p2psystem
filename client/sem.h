/*************************************************************************
    > File Name: sem.h
    > Created Time: Sun 14 Oct 2018 05:05:36 PM CST
 ************************************************************************/

#ifndef __SEM_H
#define __SEM_H

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/sem.h>
#include<fcntl.h>
#include<assert.h>
#include<sys/ipc.h>
#include<iostream>
using namespace std;
typedef union semun
{
	int val;  //我们只用到了val，所以我们就重新定义一下这个结构
}semun;
	
int sem_init(key_t key); //初始化操作

void sem_p(int id,int num); //p操作

void sem_v(int id,int num); //v操作

void sem_del(int id);   //删除信号量

#endif
