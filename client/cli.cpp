/*************************************************************************
    > File Name: cli.cpp
    > Created Time: Mon 10 Dec 2018 09:42:34 AM CST
 ************************************************************************/

#include<unistd.h>
#include<string.h>
#include<assert.h>
#include<sys/types.h>
#include<stdio.h>
#include<json/json.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<event.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<iostream>
#include"Enum.h"
#include"sem.h"
using namespace std;

int SendJson(int sockfd,Json::Value val)
{
	return send(sockfd,val.toStyledString().c_str(),strlen(val.toStyledString().c_str()),0);
}

int ReadJson(int sockfd,Json::Value &root)
{
	Json::Reader read;
	char buff[128]={0};
	int n=recv(sockfd,buff,127,0);
	if(n==-1)
		return n;
	return read.parse(buff,root);
}

bool Request(int sockfd,Json::Value &root)  
{
	Json::Value val;
	val["type"]=REQUEST;
	string name;
	cout<<"please input other name"<<endl;
	cin>>name;
	val["name"]=name.c_str();
	if(-1==SendJson(sockfd,val))
		return false;

	if(-1==ReadJson(sockfd,root))
		return false;
	if(root["ret"]!=0)
		return true;
	else
		return false;     //不在线
}
void Communicate(string& _ip,int& _port)  //主动发送方，客户端
{
	const char* mip=_ip.c_str();
	int port=_port;
	int sockfd=socket(AF_INET,SOCK_DGRAM,0);
	assert(sockfd!=-1);

	struct sockaddr_in ser,cli;   //ser即对端客户机
	ser.sin_family=AF_INET;
	ser.sin_port=htons(port);
	ser.sin_addr.s_addr=inet_addr(mip);
	while(1)
	{
		string buff;
		cout<<"please input your message: "<<endl;
		cin>>buff;
		if(strcmp(buff.c_str(),"end")==0)
		{
			close(sockfd);
			break;
		}
		sendto(sockfd,buff.c_str(),buff.size(),0,(struct sockaddr*)&ser,sizeof(ser));
		char buff1[128] = {0};
		recvfrom(sockfd,buff1,127,0,NULL,NULL);
		cout<<"from ip:"<<mip<<"and port:"<<port<<endl;
		cout<<buff1<<endl;
	}
}
bool GetLocalAddress(int sockfd,struct sockaddr_in* local)
{
	memset(local,0,sizeof(*local));
	socklen_t len=sizeof(*local);
	int ret=getsockname(sockfd,(struct sockaddr*)local,&len);
	if(ret!=0)
	{
		cout<<"getsockname fail"<<endl;
		return false;
	}
	return true;
}
void LinkCall(int _fd,short event,void* arg) //有对端发消息的回调函数
{
	int semid = sem_init((key_t)2000);
	cout<<"linkcall..."<<endl;
	struct sockaddr_in cli;
	socklen_t len=sizeof(cli);
	char buff[128]={0};   //用于接受对端发的消息
	unlink("FIFO");   //这里先这样写，fifo名字应该更改，后面就重复了
	mkfifo("FIFO",0777);
	int fd; 
	fd=open("./FIFO",O_RDWR);
	assert(fd!=-1);

	int n=recvfrom(_fd,buff,127,0,(struct sockaddr*)&cli,&len);
	system("gnome-terminal -e ./communicate"); //直接开启一个终端
	write(fd,buff,strlen(buff));
	while(1)
	{
		memset(buff,0,128);
		sem_p(semid,0);
		if(read(fd,buff,127)<=0)   
		{
			cout<<"talking end"<<endl;
			sendto(_fd,0,strlen(buff),0,(struct sockaddr*)&cli,len);
			sem_del(semid);
			return;
		}
		sendto(_fd,buff,strlen(buff),0,(struct sockaddr*)&cli,len);
		n=recvfrom(_fd,buff,127,0,(struct sockaddr*)&cli,&len);
		if(n<=0)
		{
			cout<<"that client down line"<<endl;
			sem_del(semid);
			return;
		}
		write(fd,buff,strlen(buff));
		sem_v(semid,0);
	}
	sem_del(semid);
}

void* WaitOtherLink(void* tsockfd)
{
	socklen_t clifd=socket(AF_INET,SOCK_DGRAM,0); //创建用于udp的套接子
	assert(clifd!=-1);

	socklen_t sockfd=(int)tsockfd;//用与发现服务器链接的sockfd获取本机ip端口
	struct sockaddr_in local;
	if(!GetLocalAddress(sockfd,&local))   //获取本地ip和端口
		return NULL;
	struct sockaddr_in ser;
	ser.sin_family=AF_INET;
	ser.sin_port=local.sin_port;
	ser.sin_addr.s_addr=local.sin_addr.s_addr;
	string ip = inet_ntoa(ser.sin_addr);
	cout<<ip<<endl;
	int res=bind(clifd,(struct sockaddr*)&ser,sizeof(ser));
	if(res == 0)
	{
	}
	struct event_base* base;
	base=event_base_new();
	struct event* link_event=event_new(base,clifd,EV_READ|EV_PERSIST,LinkCall,(void *)clifd);  //libevent将链接事件加入
	if(NULL==link_event)
	{
		cout<<"new libevent event fail"<<endl;
		return NULL;
	}
	event_add(link_event,NULL);
	event_base_dispatch(base);   //启动循环监听
}

void Login(int tsockfd)
{
	Json::Value val;
	val["type"]=LOGIN;
	string id;
	cout<<"please input your id"<<endl;
	cin>>id;
	val["id"]=id.c_str();
	cout<<"please input your password"<<endl;
	string password;
	cin>>password;
	val["password"]=password.c_str();
	if(-1==SendJson(tsockfd,val))
	{
		cout<<"send login error"<<endl;
		return;
	}
	Json::Value root;
	if(-1==ReadJson(tsockfd,root))
	{
		cout<<"read login error"<<endl;
		return;
	}
	if(root["ret"]!=0)    //登录成功
	{
		cout<<"Login succefful"<<endl;
		pthread_t id;    //这个线程用来处理libevent对方客户发消息
		pthread_create(&id,NULL,WaitOtherLink,(void*)tsockfd);
		while(1)
		{
			cout<<"1:Request"<<endl;
			cout<<"2:Exit"<<endl;
			cout<<"please input your choice"<<endl;
			int n;
			cin>>n;
			switch(n)
			{
				case 1:
				{
					Json::Value reg;  //存储对端的ip和port
					if(Request(tsockfd,reg))    //对端在线，可以进行交互
					{
						string ip=reg["ip"].asString();  //对端ip
						int port=reg["port"].asInt(); //对端端口
						Communicate(ip,port);	 //目前只实现阻塞在这里与一个客户端进行交互
					}
					else   //对端不在线，重新选择对端
					{
						cout<<"that client not online"<<endl;
					}
					break;
				}
				case 2:
					break;
				default:
				{
					cout<<"choice error;change again"<<endl;
					break;
				}
			}
			if(n==2)
			{
				break;
			}
		}
	}
	else
	{
		cout<<"login error"<<endl;
		return;
	}
}
void LinkSer(int tsockfd)    
{
	struct sockaddr_in findser;
	memset(&findser,0,sizeof(findser));
	findser.sin_family=AF_INET;
	findser.sin_port=htons(6500);
	findser.sin_addr.s_addr=inet_addr("127.0.0.1");	
	int res=connect(tsockfd,(struct sockaddr*)&findser,sizeof(findser));
	//assert(res!=-1);
}
void BreakLink(int tsockfd)
{
	close(tsockfd);
}

void Register(int sockfd)
{	
	while(1)
	{
		Json::Value val;
		val["type"]=REGISTER;
		string name;
		cout<<"please input your username"<<endl;
		cin>>name;
		val["name"]=name.c_str();
		cout<<"please input your password"<<endl;
		string password;
		cin>>password;
		val["password"]=password.c_str();
		if(-1==SendJson(sockfd,val))
		{
			cout<<"Register send error"<<endl;
			return;
		}

		Json::Value root;
		if(-1==ReadJson(sockfd,root))
		{
			cout<<"Register recv error"<<endl;
			return;
		}
		if(root["ret"]!=0)    //注册成功
		{
			cout<<"register success"<<endl;
			cout<<"your id is:"<<root["id"].asInt()<<"CONGRATULATION"<<endl;
			break;
		}
		else    //注册失败   名字相同，重新输入
		{
			cout<<"register fail;your username have in sql"<<endl;
		}
	}
}

int main()
{
	int tsockfd=socket(AF_INET,SOCK_STREAM,0);
	assert(tsockfd!=-1);
	LinkSer(tsockfd);
	while(1)
	{
		cout<<"1:Register"<<endl;
		cout<<"2:Login"<<endl;
		cout<<"3:Exit"<<endl;
		int n;
		cout<<"please choice"<<endl;
		cin>>n;
		switch(n)
		{
			case 1:
			{
				Register(tsockfd);
				break;
			}
			case 2:
			{
				Login(tsockfd);
				break;
			}
			case 3:
			{
				BreakLink(tsockfd);
				break;
			}
			default:
			{
				cout<<"input error"<<endl;
				break;
			}
		}
		if(n==3)
			break;
	}
	
}
