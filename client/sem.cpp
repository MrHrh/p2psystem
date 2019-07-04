/*************************************************************************
    > File Name: sem.c
    > Created Time: Sun 14 Oct 2018 04:41:01 PM CST
 ************************************************************************/

#include"sem.h"
int sem_init(key_t key) //获取信号量集id函数
{
	int id=semget(key,1,IPC_CREAT&IPC_EXCL);//获取下，看看key值所对应的信号量即是否存
	if(id==-1) //key值对应的信号量集不存在，要创建
	{
		id=semget(key,1,0664|IPC_CREAT); //2是元素个数
		semun un;
		un.val=0;  //信号量数值，这个很关键
		semctl(id,0,GETVAL,un); //根据un值设置1号下标的信号量中的各个属性
	}
	return id;
}

void sem_p(int id,int num) //num是要操作的元素下标
{
	struct sembuf buf;
	buf.sem_num=num;   
	buf.sem_op=-1;
	buf.sem_flg=SEM_UNDO;
	semop(id,&buf,1);   //1是要操作的元素数
}

void sem_v(int id,int num) //num是要操作的元素下标
{
	struct sembuf buf;
	buf.sem_num=num;
	buf.sem_op=1;
	buf.sem_flg=SEM_UNDO;
	semop(id,&buf,1);
}

void sem_del(int id)
{
	semctl(id,0,IPC_RMID);
}
