/*************************************************************************
    > File Name: main.c
    > Created Time: Mon 10 Dec 2018 09:42:40 AM CST
	> 最后修改于：2018.12.18
	> 修改者：何瑞虎
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<assert.h>

#include<mysql/mysql.h>

#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<json/json.h>
#include<event.h>
#include<pthread.h>
#include<map>
#include<iostream>
using namespace std;

/*三种服务类型*/
enum Type
{
	REGISTER,	//注册
	LOGIN,		//登录
	REQUEST,	//请求连接
};

map<int, event*>_eventmap;//map表
event_base* _base;//libevent
MYSQL* mpcon;
MYSQL_RES * mp_res;
MYSQL_ROW mp_row;

/*函数声明*/
bool requesttosql(string name, string &ip, int &port);
bool logintosql(string id, string password, string ip, int port);
bool registertosql(string name, string password, string ip, int port, int &id);
void clirequest(int fd, Json::Value val);
void clilogin(int fd, Json::Value val, struct sockaddr_in &peeraddr);
void cliregister(int fd, Json::Value val, struct sockaddr_in &peeraddr);
void cliCb(int fd, short event, void *arg);
void listenCb(int fd, short event, void *arg);
int CreateSockfd();


/*创建一个TCP服务器*/
int CreateSockfd()
{
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd != 0);

	struct sockaddr_in ser;
	ser.sin_family = AF_INET;
	ser.sin_port = htons(6500);
	ser.sin_addr.s_addr = inet_addr("127.0.0.1");

	int ret = bind(sockfd, (struct sockaddr*)&ser, sizeof(ser));
	if(ret == -1)
	{
		printf("server bind socket fail\n");
		return -1;
	}

	listen(sockfd, 5);
	printf("tcp server open...\n");

	return sockfd;
}

/*libevent的监听描述符的回调函数*/
void listenCb(int fd, short event, void *arg)
{
	printf("listenCb running...\n");
	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);
	int cliFd = accept(fd, (sockaddr*)&cli, &len);
	printf("accept a new client...\n");
	if(cliFd == -1)
	{
		printf("accept link fail\n");
		return;
	}

	struct event* clievent = event_new(_base, cliFd, EV_READ | EV_PERSIST, cliCb, NULL);
	if(NULL == clievent)
	{
		printf("add to libevent fail\n");
		return;
	}
	_eventmap.insert(make_pair(cliFd, clievent));
	event_add(clievent, NULL);
}

/*libevnet的连接文件描述符的回调函数*/
void cliCb(int fd, short event, void *arg)
{
	printf("cliCb running...\n");
	char buff[128] = {0};

	int ret = recv(fd, buff, 127, 0);
	if(ret <= 0)
	{
		printf("%d client is breaked\n", fd);
		event_free(_eventmap[fd]);
		_eventmap.erase(fd);
		close(fd);
		return;
	}
	
	struct sockaddr_in peeraddr;//用来获取该套接字的对端ip和port
	socklen_t len = sizeof(peeraddr);
	if(-1 == getpeername(fd, (struct sockaddr*)&peeraddr, &len))
	{
		printf("get cli %d ip&port fail\n", fd);
		return;
	}
	Json::Value val;
	Json::Reader read;
	if(!read.parse(buff, val))
	{
		printf("read recvbuff fail\n");
		return;
	}
	
	if(val["type"].asInt() == REGISTER)
	{
		cliregister(fd, val, peeraddr);//若是注册类型，则调用该函数进行注册，并且绑定注册时的ip和port
	}
	if(val["type"].asInt() == LOGIN)
	{
		clilogin(fd, val, peeraddr);//登录类型，调用该函数
	}
	if(val["type"].asInt() == REQUEST)
	{
		clirequest(fd, val);//请求其他客户端调用
	}

}

/*注册类型数据处理函数*/
void cliregister(int fd, Json::Value val, struct sockaddr_in &peeraddr)
{
	printf("cliregister running...\n");
	string name = val["name"].asString();
	string password = val["password"].asString();
	string ip = inet_ntoa(peeraddr.sin_addr);
	cout<<peeraddr.sin_port<<endl;
	int port = ntohs(peeraddr.sin_port);
	cout<<port<<endl;
	int id;
	bool ret = registertosql(name, password, ip, port, id);//对数据库进行操作的函数并且从中获取到服务器给客户端分配的id
	cout<<"id:"<<id<<endl;
	if(ret)//注册成功
	{
		Json::Value val1;
		val1["ret"] = 1;//注册成功标志
		val1["id"] = id;//只返回一个id
		string Ret = val1.toStyledString();
		send(fd, Ret.c_str(), strlen(Ret.c_str()), 0);//给客户端返回结果
	}
	else//注册失败
	{
		Json::Value val1;
		val1["ret"] = 0;//失败标志
		string Ret = val1.toStyledString();
		send(fd, Ret.c_str(), strlen(Ret.c_str()), 0);//给客户端返回结果
	}
}

/*登录类型数据处理函数*/
void clilogin(int fd, Json::Value val, struct sockaddr_in &peeraddr)
{
	printf("clilogin running...\n");
	string id = val["id"].asString();
	string password = val["password"].asString();
	string ip = inet_ntoa(peeraddr.sin_addr);
	int port = ntohs(peeraddr.sin_port);
	bool ret = logintosql(id, password, ip, port);//检测密码和用户名是否正确，并且更新ip和port
	if(ret)//登录成功
	{
		Json::Value val1;
		val1["ret"] = 1;
		string Ret = val1.toStyledString();
		send(fd, Ret.c_str(), strlen(Ret.c_str()), 0);//登录成功结果返回
	}
	else//登录失败
	{
		Json::Value val1;
		val1["ret"] = 0;
		string Ret = val1.toStyledString();
		send(fd, Ret.c_str(), strlen(Ret.c_str()), 0);//登录失败结果返回
	}
}

/*请求链接类型处理函数*/
void clirequest(int fd, Json::Value val)
{
	printf("clirequest running...\n");
	string ip;
	int port;
	string name = val["name"].asString();
	bool ret = requesttosql(name, ip, port);//查询并返回要链接的客户端ip和port
	if(ret)
	{
		Json::Value val1;
		val1["ret"] = 1;
		val1["ip"] = ip.c_str();
		val1["port"] = port;
		string Ret = val1.toStyledString();
		send(fd, Ret.c_str(), strlen(Ret.c_str()), 0);//找到要链接的客户端，将其ip&port返回
	}
	else
	{
		Json::Value val1;
		val1["ret"] = 0;
		string Ret = val1.toStyledString();
		send(fd, Ret.c_str(), strlen(Ret.c_str()), 0);//未找到返回0
	}
}

int InitMysql()
{
	mpcon = mysql_init((MYSQL *)0);
	if(!mysql_real_connect(mpcon,"127.0.0.1","root","111111",NULL,3306,NULL,0))
	{
		return 0;
	}
	if(mysql_select_db(mpcon,"p2pclient"))
	{
		return 0;
	}
	printf("mysql open success...\n");
	return 1;
}


/*注册类型数据的数据库操作函数*/
bool registertosql(string name, string password, string ip, int port, int &id)
{
	printf("registertosql running...\n");
	/*将命令拼接起来，发往mysql去执行*/
	char cmd[100] = {"insert into p2ptab values"};
	strcat(cmd, "('','");
	strcat(cmd, password.c_str());
	strcat(cmd, "'");
	
	strcat(cmd, ",'");
	strcat(cmd, name.c_str());
	strcat(cmd, "'");

	strcat(cmd, ",'");
	strcat(cmd, ip.c_str());
	strcat(cmd, "'");

	char portstring[10] = {0};
	sprintf(portstring, "%d", port);

	strcat(cmd, ",'");
	strcat(cmd, portstring);
	strcat(cmd, "');");
	
	if(mysql_real_query(mpcon, cmd, strlen(cmd)))
	{
		printf("cmd query fail\n");
		return false;
	}

	id = mysql_insert_id(mpcon);//获取自曾的uid
	return true;
}

/*登录类型数据的数据库操作函数*/
bool logintosql(string id, string password, string ip, int port)
{
	printf("logintosql running...\n");
	/*将命令拼接起来，发往mysql去执行*/
	char cmd[100] = {"select *from p2ptab where uid="};
	strcat(cmd, "'");
	strcat(cmd, id.c_str());
	strcat(cmd, "' and ");

	strcat(cmd, "password=");
	strcat(cmd, "'");
	strcat(cmd, password.c_str());
	strcat(cmd, "';");
	
	if(mysql_real_query(mpcon, cmd, strlen(cmd)))//从数据库中匹配用户名和密码
	{
		printf("cmd query fail\n");
		return false;
	}

	mp_res = mysql_store_result(mpcon);
	mp_row = mysql_fetch_row(mp_res);
	if(mp_row == NULL)//没有匹配到返回false
	{
		return false;
	}
	
	char cmd1[100] = {"update p2ptab set "};
	strcat(cmd1, "ip = '");
	strcat(cmd1, ip.c_str());
	strcat(cmd1, "',");
	strcat(cmd1, "port = '");
	
	char portstring[5] = {0};
	sprintf(portstring, "%d", port);

	strcat(cmd1, portstring);
	strcat(cmd1, "' where uid='");
	strcat(cmd1, id.c_str());
	strcat(cmd1, "';");
	
	if(mysql_real_query(mpcon, cmd1, strlen(cmd1)))//更新ip和port
	{
		printf("cmd1 query fail\n");
		return false;
	}
	return true;
}

/*请求链接类型数据的数据库操作函数*/
bool requesttosql(string name, string &ip, int &port)
{
	printf("requesttosql running...\n");
	char cmd[100] = {"select * from p2ptab where name='"};
	strcat(cmd, name.c_str());
	strcat(cmd, "';");

	if(mysql_real_query(mpcon, cmd, strlen(cmd)))
	{
		printf("cmd query fail\n");
		return false;
	}

	
	mp_res = mysql_store_result(mpcon);
	mp_row = mysql_fetch_row(mp_res);
	if(mp_row == NULL)//没有匹配到返回false
	{
		return false;
	}
	ip = mp_row[3];
	port = atoi(mp_row[4]);
	cout<<"request ip: "<<ip<<" port:"<<port<<endl;
	return true;
}

void *serverrun(void *arg)
{
	printf("libevent running...\n");
	event_base_dispatch(_base);
}

int main()
{
	int sockfd = CreateSockfd();
	_base = event_base_new();
	assert(_base != NULL);

	struct event* listenEvent = event_new(_base, sockfd, EV_READ | EV_PERSIST, listenCb, NULL);
	if(InitMysql()==0)
	{
		printf("mysql error!\n");
		return 0;
	}
	if(NULL == listenEvent)
	{
		printf("create listenEvent fail\n");
		return 0;
	}
	event_add(listenEvent, NULL);

	pthread_t tid;
	pthread_create(&tid, NULL, serverrun, NULL);
	pthread_exit(NULL);

	return 0;
}
