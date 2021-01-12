//时间：2019年8月10日13:32:39
//完成例程

/*
本质：	1.为我们的socket,重叠结构绑定一个函数。当异步操作完成时，系统异步自动调用这个函数，send绑一个，recv绑一个，完成就调用各自的函数，自动分类  （1）然后我们再函数内部做相应的操作 （2）注意：异步完成之后才有的通知
		2.事件通知  需要咱们调用WSAGetOverlappedResult得到结果，然后根据逻辑，自己分类，分类的逻辑太多，大家自己思考
		3.所以区别  分类方式上，完成例程性能更好，系统自动调用

代码逻辑：	1.创建事件数组,socket数组，重叠结构体数组    下标相同的绑定成一组
			2.创建重叠IO模型使用的socket    WSASocket
			3.投递AcceptEx   (1)立即完成：   1）对客户端套接字投递WSARecv   
													1.立即完成
													2.回调函数：  处理信息     对客户端套接字投递WSARecv
											2）根据需求对客户端套接字投递WSASend    
													1.立即完成
										            2.回调函数
											3）对服务器套接字继续投递AcceptEx   重复上述
							(2)延迟完成    那就去循环等信号
		4.循环等信号	（1）等信号   WSAWaitForMultipleEvents
						(2)有信号    接收链接   投递AcceptEx
						(3)根据需求对客户端套接字投递WSASend   回调函数
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

	//创建重叠IO模型使用的socket
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


	//投递AcceptEx
	if (0 != PostAccept())
	{
		Clear();
		WSACleanup();
		return 0;
	}

	//循环等待事件
	while (1)
	{
		DWORD nRes = WSAWaitForMultipleEvents(1, &g_allOlp[0].hEvent, FALSE, WSA_INFINITE, TRUE);    //参数5： WSA_INFINITE 咱就等一个，所以等有信号再走就行
																									//参数6：设置TRUE  意义：1.将等待事件函数与完成例程机制结合在一起  
																														//2.实现等待事件函数与完成例程函数的异步执行，执行完并给等待事件函数信号     等待事件函数返回WSA_WAIT_IO_COMPLETION(一个完成例程执行完了，就会返回这个值)
																														//3.即WSAWaitForMultipleEvents不仅能获取事件的信号通知，还能获取完成例程的执行通知
		if (WSA_WAIT_FAILED == nRes || WSA_WAIT_IO_COMPLETION == nRes)   
		{
			continue;
		}

		//有信号了
		//信号置空
		WSAResetEvent(g_allOlp[0].hEvent);

		//接收链接完成了
		//PostSend(g_count);
		printf("accept\n");
		//投递Recv
		PostRecv(g_count);
		//根据情况投递send
		//客户端适量++
		g_count++;
		//投递accept
		PostAccept();
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
	while (1)   //循环方法
	{
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
			PostRecv(g_count);
			//根据情况投递send
			//客户端适量++
			g_count++;
			//投递accept,递归方法, 递归层数多会爆栈
			//PostAccept();
			continue;
		}
		else
		{
			int a = WSAGetLastError();
			if (ERROR_IO_PENDING == a)
			{
				//延迟处理
				break;
			}
			else
			{
				break;
			}
		}
	}
	return 0;
}

//回调函数
/*
	返回值void
	调用约定 CALLBACK
	函数名字 随意
	参数1：错误码
	参数2：发送或接收到的字节数
	参数3：重叠结构
	参数4：函数执行的方式  对照WSARecv参数5
	处理流程 1.dwError == 10054  强行退出   （1）先判断   （2）删除客户端
			2.cbTransferred == 0   正常退出     
			3. else   处理数据
	对应WSAGetOverlapperResult函数
*/
void CALLBACK RecvCall(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	//方法一：这种获取下标的方法效率太低
	//int i = 0;
	//for (i; i < g_count; i++)
	//{
	//	if (lpOverlapped->hEvent == g_allOlp[i].hEvent)
	//	{
	//		break;
	//	}
	//}
	//方法二：
	int i = lpOverlapped - &g_allOlp[0];

	if (10054 == dwError || 0 == cbTransferred)
	{
		//删除客户端
		printf("close\n");
		//关闭
		closesocket(g_allSock[i]);
		WSACloseEvent(g_allOlp[i].hEvent);
		//从数组中删掉
		g_allSock[i] = g_allSock[g_count - 1];
		g_allOlp[i] = g_allOlp[g_count - 1];
		//个数减1
		g_count--;
	}
	else
	{
		printf("%s\n", g_strRecv);
		memset(g_strRecv, 0, MAX_RECV_COUNT);
		//根据情况投递send
		//对自己投递接收
		PostRecv(i);
	}
}

int PostRecv(int index)
{
	//投递异步接收信息

	WSABUF wsabuf;
	wsabuf.buf = g_strRecv;
	wsabuf.len = MAX_RECV_COUNT;

	DWORD dwRecvCount;
	DWORD dwflag = 0;

	int nRes = WSARecv(g_allSock[index], &wsabuf, 1, &dwRecvCount, &dwflag, &g_allOlp[index], RecvCall);
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

void CALLBACK SendCall(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	printf("send over\n");
}

int PostSend(int index)
{
	WSABUF wsabuf;
	wsabuf.buf = "你好";
	wsabuf.len = MAX_RECV_COUNT;

	DWORD dwSendCount;
	DWORD dwflag = 0;

	int nRes = WSASend(g_allSock[index], &wsabuf, 1, &dwSendCount, dwflag, &g_allOlp[index], SendCall);
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


/*
对比事件通知： 1.事件通知是咱们自己分配任务	（1）我们waitfor,所以顺序不能保证
											（2）循环次数多，下标越大的客户端延迟会越大
			 2.完成例程是咱们写好任务处理代码  系统根据具体事件自动调用咱们的代码，自动分类
			 3.所以完成例程比事件通知的性能稍好一些
问题：调试下断点时，在一次处理过程中，客户端产生多次send，服务器会产生多次接收消息，第一次接收消息会收完所有信息
*/