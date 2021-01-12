//时间：2019年8月8日17:18:52
//重叠I/O

/*
重叠IO介绍：
意义：重叠IO是windows提供的一种异步读写文件的机制
作用：	1.正常读写文件（socket本质就是文件操作）,如recv，是阻塞的,等协议缓冲区中的数据全部复制进buffer里，函数才结束并返回复制的个数，写也一样，同一时间只能读写一个，其他的全都靠边等
		2.重叠IO机制读写，将读的指令以及咱们的buffer投给操作系统，然后函数直接返回，操作系统独立开个线程，将数据复制进咱们的Buffer，数据复制期间，咱们就去做其他事儿，两不耽误，即读写过程变成了异步，可以同时投递多个读写操作
		3.即：将accept, recv, send优化成了异步过程，被AcceptEx WSARecv WSASend函数代替     所以说这个是对基本c/s模型的直接优化
本质：	1.结构体：WSAOVERLAPPED  struct _WSAOVERLAPPED
                              {										前四个成员系统使用，咱们不需要直接使用
							       DWORD Internal;                  成员1：保留，供服务提供商定义使用   （函数设计者）
								   DWORD InternalHigh;              成员2：保留，供服务提供商定义使用    接收或者发送的字节数
								   DWORD Offset;                    成员3：保留，供服务提供商定义使用
								   DWORD OffsetHigh;                成员4：保留，供服务提供商定义使用   一般是错误码
								   WSAEVENT hEvent;                 成员5：事件对象   我们唯一关注的    操作完成他就会被置成有信号
							  }
		2.定义一个该结构体的变量与socket绑定
使用：	1.异步选择模型把消息与socket绑一起，然后系统以消息机制处理反馈
		2.事件选择模型把事件与socket绑一起，然后系统以事件机制处理反馈
		3.重叠IO模型把重叠结构与socket绑一起，然后系统以重叠IO机制处理反馈
重叠IO反馈两种方式：1.事件通知
					2.完成例程   回调函数

重叠IO逻辑：1.事件通知：（1）调用AcceptEx WSARecv WSASend投递
                        （2）被完成的操作，事件信号置成有信号
						（3）调用WSAWaitForMultipleEvents获取事件信号
		    2.回调函数/完成例程 （1）调用AcceptEx WSARecv WSASend投递
			                    （2）完成后自动调用回调函数
性能：1.网上有测试结果的    20000个客户端同时请求链接以及发送信息   （1）系统CPU使用率上升40%左右  （2）完成端口只有2%左右  极品
	 2.其他模型也能处理这么多   但是同步（阻塞）的原因，导致延迟太高
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <mswsock.h>      //要放在winsock2.h后面，不然会报错
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

#define MAX_COUNT 1024
#define MAX_RECV_COUNT 1024

//第一步,创建事件数组，socket数组，重叠结构体数组 下标相同的绑定成一组
SOCKET g_allSock[MAX_COUNT];      //创建socket数组     （创建事件数组可省略，重叠结构体里包含了事件）
OVERLAPPED g_allOlp[MAX_COUNT];   //创建重叠结构体数组    下标相同的绑定成一组，调用函数时会自动绑定到一起
int g_count;

//接收缓冲区
char g_strRecv[MAX_RECV_COUNT] = { 0 };

int PostAccept();
int PostRecv(int index);
int PostSend(int index);
void Clear()
{
	for (int i = 0; i < g_count; i++)
	{
		closesocket(g_allSock[i]);
		WSACloseEvent(g_allOlp[i].hEvent);
	}
}

//回调函数  (点控制台退出)
BOOL WINAPI fun(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
		Clear();
		break;
	}

	return TRUE;
}

int main()
{
	//控制台退出 主函数投递一个监视
	SetConsoleCtrlHandler(fun, TRUE);

	//打开网络库
	WSADATA wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		printf("网络库打开失败\n");
		return 0;
	}

	//校验版本
	if (2 != HIBYTE(wsadata.wVersion) || 2 != LOBYTE(wsadata.wVersion))
	{
		printf("版本有误\n");
		WSACleanup();
		return 0;
	}

	//第二步，创建重叠IO模型使用的socket
/*
		作用：	1.创建一个用于异步操作的socket
				2.WSASocket windows专用   (1)WSA的函数都是windows专用的   (2)windows socket async 都是用于支持异步操作
		参数1：地址的类型  AF_INET
		参数2：套接字的类型   TCP是SOCK_STREAM
		参数3：协议的类型
		参数4：	1.设置套接字详细的属性   （1）指向WSAPROTOCOL_INFO结构的指针
										（2）比如：发送数据是否需要链接（accept  connect）；是否保证数据完整到达(如果数据丢失是否允许)；参数3填0，那么匹配哪个协议；设置传输接收字节数；设置套接字权限；还有很多保留字段，供以后拓展使用
				2.不使用 NULL

		参数5：	1.一组socket的组ID,大概是想一次操作多个socket
				2.保留的，暂时无用，填0
		参数6：	1.指定套接字属性
				2.填写WSA_FLAG_OVERLAPPED   创建一个供重叠IO模型使用的socket
				3.其他的（1）WSA_FLAG_MULTIPOINT_C_ROOT; WSA_FLAG_MULTIPOINT_C_LEAF; WSA_FLAG_MULTIPOINT_D_ROOT; WSA_FLAG_MULTIPOINT_D_LEAF   这几种用于多播协议
						（2）WSA_FLAG_ACCESS_SYSTEM_SECURITY	1）套接字操作权限/配合参数4使用
																2）意义：可以对套接字send设置权限，就会触发相关的权限设置，提醒
						（3）WSA_FLAG_NO_HANDLE_INHERIT	1）套接字不可被继承
														2）在多线程开发中，子线程会继承父线程的socket,即主线程创建了一个socket,那么子线程有两种使用方式 ： 一，直接用父类的，父子都使用这一个socket--共享   二，把父类的这个socket复制一份，自己用，这俩父子用两个socket，但是本质一样--继承
		返回值：1.成功返回可用的socket  不用了就一定要销毁套接字
				2.失败返回INVALID_SOCKET
*/
	SOCKET socketServer = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == socketServer)
	{
		printf("服务器socket创建失败,错误码：%d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	//绑定IP和端口
	struct sockaddr_in socketServerMsg;
	socketServerMsg.sin_family = AF_INET;
	socketServerMsg.sin_port = htons(12345);
	socketServerMsg.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	if (SOCKET_ERROR == bind(socketServer, (const struct sockaddr *)&socketServerMsg, sizeof(socketServerMsg)))
	{
		printf("绑定IP和端口失败，错误码：%d\n", WSAGetLastError());
		closesocket(socketServer);

		WSACleanup();
		return 0;
	}

	//监听
	if (SOCKET_ERROR == listen(socketServer, SOMAXCONN))
	{
		printf("监听失败，错误码：%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	g_allSock[g_count] = socketServer;
	g_allOlp[g_count].hEvent = WSACreateEvent();
	g_count++;


	//第三步，投递AcceptEx
	/*
		1.立即完成	（1）对客户端套接字投递WSARecv
							1）立即完成  处理信息  对客户端套接字投递WSARecv   重复上述
							2）延迟完成  那就去循环等消息
					（2) 根据需求对客户端套接字投递WSASend
							1）立即完成  处理消息       重复上述
							2）延迟完成  那就去循环等消息
					（3) 对服务器套接字继续投递AcceptEx 重复上述
		2.延迟完成  那就去循环等消息
	*/
	if (0 != PostAccept())
	{
		Clear();
		WSACleanup();
		return 0;
	}
	
	//循环等待事件
	/*
		1.等信号 WSAWaitForMultipleEvents
		2.有信号	（1) 获取重叠结构上的信息   WSAGetOverlappedResult
					（2）客户端推出       删除服务端的信息
					（3）接受链接   投递AcceptEx  上边这个逻辑写好了，封装成函数就非常清晰
					（4）接受信息   处理消息    对客户端套接字投递WSARecv
					（5）发送消息   根据需求对客户端套接字投递WSASend
	*/
	while (1)
	{
		for (int i = 0; i < g_count; i++)
		{
			DWORD nRes = WSAWaitForMultipleEvents(1, &g_allOlp[i].hEvent, FALSE, 0, FALSE);
			if (WSA_WAIT_FAILED == nRes || WSA_WAIT_TIMEOUT == nRes)
			{
				continue;
			}

			//有信号了
			/*
				功能：获取对应的socket上的具体情况 
				参数1：有信号的socket
				参数2：对应的重叠结构
				参数3：	1.由发送或者接收到的实际字节数
						2. 0 表示客户端下线
				参数4：	1.仅当重叠操作选择了基于事件的完成通知时，才能将fWait参数设置为TRUE
						2.选择事件通知，填TRUE
				参数5：	1.装WSARecv的参数5 lpflags
						2.不能是NULL
				返回值：1.函数执行成功返回TRUE
						2.失败返回FALSE
			*/
			DWORD dwState;
			DWORD dwFlag;
			BOOL bFlag = WSAGetOverlappedResult(g_allSock[i], &g_allOlp[i], &dwState, TRUE, &dwFlag);

			//信号置空
			WSAResetEvent(g_allOlp[i].hEvent);

			if (FALSE == bFlag)
			{
				int a = WSAGetLastError();
				if (10054 == a)    //跟Recv一样
				{
					//客户端强制退出
					printf("force close\n");
					//关闭
					closesocket(g_allSock[i]);
					WSACloseEvent(g_allOlp[i].hEvent);
					//从数组中删掉
					g_allSock[i] = g_allSock[g_count - 1];
					g_allOlp[i] = g_allOlp[g_count - 1];
					//循环控制变量减1
					i--;
					//个数减1
					g_count--;
				}
				continue;
			}
			//成功
			if (0 == i)
			{
				//接收链接完成了
				printf("accept\n");
				PostSend(g_count);
				//投递Recv
				PostRecv(g_count);
				//根据情况投递send
				//客户端适量++
				g_count++;
				//投递accept
				PostAccept();
				continue;
			}

			if (0 == dwState)      //accept时dwState依然是0，所以应该先判断accept再判断是否正常下线
			{
				//客户端正常下线
				printf("close\n");
				//关闭
				closesocket(g_allSock[i]);
				WSACloseEvent(g_allOlp[i].hEvent);
				//从数组中删掉
				g_allSock[i] = g_allSock[g_count - 1];
				g_allOlp[i] = g_allOlp[g_count - 1];
				//循环控制变量减1
				i--;
				//个数减1
				g_count--;
				continue;
			}

			if (0 != dwState)
			{
				//发送或者接收成功了
				if (g_strRecv[0] != 0)
				{
					//recv
					//打印信息
					printf("%s\n", g_strRecv);
					memset(g_strRecv, 0, MAX_RECV_COUNT);
					//根据情况投递send
					//对自己投递接收
					PostRecv(i);
				}
				else
				{
					//send
				}
			}
		}
	}


	Clear();
	//清理网络库
	WSACleanup();

	system("pause");
	return 0;
}

//投递AcceptEx
int PostAccept()
{
	//投递异步接收链接请求
/*
	功能：投递服务器socket,异步接收链接
	参数1：服务器socket
	参数2：链接服务器的客户端的socket   1.注意：我们要调用WSASocket亲自创建  
										2.AcceptEx将客户端的IP端口号绑在这个socket上
    参数3：	1.缓冲区的指针，接收在新连接上发送的第一个数据	（1）客户端第一次send,由这个函数接收
															（2）第二次以后就由WSARecv接收
			2.char str[1024]
			3.不能设置NULL

	参数4：	1.设置0  取消了参数3的功能
			2.设置成参数3的1024：	（1）那么就会接收第一次的数据
									（2）此时，客户端链接并发送了一条消息，服务器才产生信号  ： 所以会有阻塞状况，光链接不行，必须等客户端发来一条消息，他才完事有信号，所以设置0就行了

    参数5：	1.为本地地址信息保留的字节数。此值必须至少比使用的传输协议的最大地址长度长16个字节   (本地去保存客户端地址信息空间的大小)
			2.sizeof(struct sockaddr_in) + 16

    参数6：	1.为远程地址信息保留的字节数，此值必须至少比使用的传输协议的最大地址长度长16个字节，不能为0    （大概意思：在物理内存上申请多大空间去临时存储客户端的地址信息）    （文件地址，物理地址）
			2.sizeof(struct sockaddr_in) + 16

	参数7：	1.该函数可以接收第一次客户端发来的消息，如果这个消息刚好是调用时候接收到了，也即立即接收到了（客户端链接的同时发送了消息），这时候装着接收到的字节数（配合参数3，4使用）   
				（1）也就是这个接收是同步完成的时候，这个参数会被装上
				（2）没有立即处理，也即异步接收到消息，这时候这个参数没用
			2.大家可以填写：（1）填写NULL,不想获得这个字节数    （2）也可以填写DWORD变量的地址， 填这个就行
	参数8：我们的重叠结构(服务器socket对应的重叠重构)
	返回值：1.TRUE   立即返回    刚执行就已经有客户端到了
	       2.FALSE   出错    （1）int a = WSAGetLatError()
							（2）如果 a == ERROR_IO_PENDING  异步等待， 客户端还没来
							（3）其他，根据错误码解决
*/
	//创建客户端socket和相应的事件   （参数2需要）
	g_allSock[g_count] = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_allOlp[g_count].hEvent = WSACreateEvent();

	//创建字符数组   （参数3需要）
	char str[1024] = { 0 };

	DWORD dwRecvcount;   //参数7需要

	//投递AcceptEx
	BOOL bRes = AcceptEx(g_allSock[0], g_allSock[g_count], str, 0, sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16, &dwRecvcount, &g_allOlp[0]);
	if (TRUE == bRes)
	{
		//立即完成了
		//投递Recv
		printf("accept\n");
		PostSend(g_count);
		PostRecv(g_count);
		//根据情况投递send
		//客户端适量++
		g_count++;
		//投递accept
		PostAccept();

		return 0;
	}
	else
	{
		int a = WSAGetLastError();
		if (ERROR_IO_PENDING == a)
		{
			//延迟处理
			return 0;
		}
		else
		{
			return a;
		}
	}
}

int PostRecv(int index)
{
/*
    作用：投递异步接收信息
	参数1：客户端socket
	参数2：接收后的信息存储buffer    WSABUF    struct _WSABUF{ULONG len;  CHAR *buf}    成员1：字节数   成员2：指向字符数组的指针
	参数3：参数2是个WSABUF的个数  1个
	参数4：	1.接收成功的话，这里装着成功接收到的字节数
			2.参数6重叠结构不为NULL的时候，此参数可以设置为NULL

	参数5：	1.指向用于修改WSARecv函数调用行为的标志的指针
			2.MSG_PEEK   协议缓冲区信息复制出来，不删除，跟recv参数5一样
			3.MSG_OOB    带外数据
			4.MSG_PUSH_IMMEDIATE        通知传送尽快完成（1）比如传输10M数据，一次只能传1M，这个包要拆成10份发送，第一份发送中，后面9份要排队等待，指定了这个标记，那么指示了快点，别等了，那么没被发送的就被舍弃了，从而造成了数据发送缺失
		                                                （2）该参数不建议用于多数据的发送    聊天的那种没问题，发个文件什么的就不建议了
														（3）提示系统尽快处理，所以能减少一定的延迟
			5.MSG_WAITALL   呼叫者提供的缓冲区已满，连接已关闭，请求已取消或发生错误        才把数据发送出去
			6.MSG_PARTIIAL    传出的（send指定，recv收入不指定）       表示咱们此次接收到的数据是客户端发来的一部分，接下来接收下一部分

	参数6：重叠结构
	参数7：回调函数   完成例程形式使用    事件形式可以不使用  设置NULL
	返回值：1.立即发生  0   计算机比较闲，立刻就发送出去了
	       2.SOCKET_ERROR   int a = WSAGetLastError()    a == WSA_IO_PENDING表示你自己去干把
*/

	WSABUF wsabuf;
	wsabuf.buf = g_strRecv;
	wsabuf.len = MAX_RECV_COUNT;

	DWORD dwRecvCount;
	DWORD dwflag = 0;

	int nRes = WSARecv(g_allSock[index], &wsabuf, 1, &dwRecvCount, &dwflag, &g_allOlp[index], NULL);
	if (0 == nRes)
	{
		//立即完成
		//打印信息
		printf("%s\n", wsabuf.buf);
		memset(g_strRecv, 0, MAX_RECV_COUNT);
		//根据情况投递send
		//对自己投递接收
		PostRecv(index);
		return 0;
	}
	else
	{
		int a = WSAGetLastError();
		if (ERROR_IO_PENDING == a)
		{
			//延迟处理
			return 0;
		}
		else
		{
			return a;
		}
	}

	return 0;
}


int PostSend(int index)
{
	WSABUF wsabuf;
	wsabuf.buf = "你好";
	wsabuf.len = MAX_RECV_COUNT;

	DWORD dwSendCount;    //成功发送的字节数
	DWORD dwflag = 0;     //函数调用行为的标志

	int nRes = WSASend(g_allSock[index], &wsabuf, 1, &dwSendCount, dwflag, &g_allOlp[index], NULL);
	if (0 == nRes)
	{
		//立即完成
		//打印信息
		printf("send成功\n");
		return 0;
	}
	else
	{
		int a = WSAGetLastError();
		if (ERROR_IO_PENDING == a)
		{
			//延迟处理
			return 0;
		}
		else
		{
			return a;
		}
	}

	return 0;
}
