//时间：2019年8月5日23:33:27
//事件选择模型

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

//定义事件集合以及socket集合的结构体
struct fd_es_set
{
	unsigned short count;
	SOCKET sockall[WSA_MAXIMUM_WAIT_EVENTS];
	WSAEVENT eventall[WSA_MAXIMUM_WAIT_EVENTS];
};

//一组一组询问,定义成结构体数组
struct fd_es_set esSet[20];    //结构体数组

//回调函数
BOOL WINAPI fun(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
		for (int j = 0; j < 20; j++)
		{
			for (int i = 0; i < esSet[j].count; i++)
			{
				closesocket(esSet[j].sockall[i]);
				WSACloseEvent(esSet[j].eventall[i]);
			}
		}
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
	int nRes = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (0 != nRes)
	{
		printf("创建失败，错误码为%d\n", WSAGetLastError());
		return 0;
	}

	//校验版本
	if (2 != HIBYTE(wsadata.wVersion) || 2 != LOBYTE(wsadata.wVersion))
	{
		printf("版本错误\n");
		WSACleanup();
		return 0;
	}

	//创建服务器Socket
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == socketServer)
	{
		printf("创建服务器Socket失败\n");
		WSACleanup();
		return 0;
	}

	//绑定IP地址和端口
	struct sockaddr_in socketMsg;
	socketMsg.sin_family = AF_INET;
	socketMsg.sin_port = htons(12345);
	socketMsg.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	if (SOCKET_ERROR == bind(socketServer, (const struct sockaddr*)&socketMsg, sizeof(socketMsg)))
	{
		printf("绑定IP地址和端口出现错误，错误码为%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	//监听
	if (SOCKET_ERROR == listen(socketServer, SOMAXCONN))
	{
		printf("监听失败，错误码为%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	//一组一组询问,定义成结构体数组
	//struct fd_es_set esSet[20];    //结构体数组
	//memset(esSet, 0, sizeof(struct fd_es_set)*20);    //memset：将esSet当前位置后面的sizeof(struct fd_es_set)*20个字节用0替换，并返回esSet

	//创建事件                                                      
	WSAEVENT eventServer = WSACreateEvent();   

	if (WSA_INVALID_EVENT == eventServer)  
	{
		printf("创建事件失败，错误码为%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}
	
	//绑定并投递 WSAEventSelect  
	if (SOCKET_ERROR == WSAEventSelect(socketServer, eventServer, FD_ACCEPT))
	{
		printf("绑定事件失败，错误码为%d\n", WSAGetLastError());
		//释放事件句柄
		WSACloseEvent(eventServer);
		//释放socket
		closesocket(socketServer);
		//清理网络库
		WSACleanup();
	}

	//装进去
	esSet[0].eventall[esSet[0].count] = eventServer;
	esSet[0].sockall[esSet[0].count] = socketServer;
	esSet[0].count++;

	//一组一组处理：（1）单线程，一组一组顺序处理  （2）创建多个线程，每个线程处理一个事件表，最大64，服务器socket只有一个，accept放在第一个线程，其他线程处理read,write,close就行

	while (1)
	{
		//询问事件
		for (int j = 0; j < 20; j++)     //单线程，一组一组顺序处理
		{
			if (0 == esSet[j].count)   //该组有效事件如果为0则跳到下一次循环
			{
				continue;
			}
			DWORD nRes = WSAWaitForMultipleEvents(esSet[j].count, esSet[j].eventall, FALSE, 0, FALSE);

			if (WSA_WAIT_FAILED == nRes)
			{
				printf("询问事件失败，错误码为%d\n", WSAGetLastError());
				break;
			}
			if (WSA_WAIT_TIMEOUT == nRes)
			{
				continue;
			}
			DWORD nIndex = nRes - WSA_WAIT_EVENT_0;   //此时得到的是有信号事件中索引值最小的

			//有序处理方法2，//此索引值前的事件都是无信号的，可以直接从此处开始循环接下来的所有事件，处理有信号的事件，优化2比优化1更好一点
			for (int i = nIndex; i < esSet[j].count; i++)
			{
				DWORD nRes = WSAWaitForMultipleEvents(1, &esSet[j].eventall[i], FALSE, 0, FALSE);
				if (WSA_WAIT_FAILED == nRes)
				{
					printf("询问事件失败，错误码为%d\n", WSAGetLastError());
					continue;
				}
				//如果参数4写具体超时间隔，此时事件要加上超时判断
				if (WSA_WAIT_TIMEOUT == nRes)
				{
					continue;
				}

				//列举事件,得到下标对应的具体操作
				WSANETWORKEVENTS  NetworkEvents;
				if (SOCKET_ERROR == WSAEnumNetworkEvents(esSet[j].sockall[i], esSet[j].eventall[i], &NetworkEvents))
				{
					printf("得到下标对应的具体操作失败，错误码：%d\n", WSAGetLastError());
					break;
				}

				if (NetworkEvents.lNetworkEvents & FD_ACCEPT) 
				{
					if (0 == NetworkEvents.iErrorCode[FD_ACCEPT_BIT])
					{
						//正常处理
						SOCKET socketClient = accept(esSet[j].sockall[i], NULL, NULL);
						if (INVALID_SOCKET == socketClient)
						{
							continue;
						}

						//创建事件对象
						WSAEVENT wsaClientEvent = WSACreateEvent();
						if (WSA_INVALID_EVENT == wsaClientEvent)
						{
							closesocket(socketClient);
							continue;
						}

						//投递给系统
						if (SOCKET_ERROR == WSAEventSelect(socketClient, wsaClientEvent, FD_READ | FD_CLOSE | FD_WRITE))
						{
							closesocket(socketClient);
							WSACloseEvent(wsaClientEvent);
							continue;
						}

						//每组只能装64个，大于64个的话要装到下一组
						for (int m = 0; m < 20; m++)
						{
							if (esSet[m].count < 64)
							{
								//装进结构体
								esSet[m].sockall[esSet[m].count] = socketClient;
								esSet[m].eventall[esSet[m].count] = wsaClientEvent;
								esSet[m].count++;
								break;
							}
						}
						printf("accept event\n");
					}
					else
					{
						continue;
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_WRITE)    
				{
					if (0 == NetworkEvents.iErrorCode[FD_WRITE_BIT])
					{
						if (SOCKET_ERROR == send(esSet[j].sockall[i], "connect success", strlen("connect success"), 0)) 
						{
							printf("send失败，错误码为：%d\n", WSAGetLastError());  
							continue;
						}
						printf("write event\n");
					}
					else
					{
						printf("socket error套接字错误，错误码为：%d\n", NetworkEvents.iErrorCode[FD_WRITE_BIT]);
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_READ)
				{
					if (0 == NetworkEvents.iErrorCode[FD_READ_BIT])
					{
						char strRecv[1500] = { 0 };
						if (SOCKET_ERROR == recv(esSet[j].sockall[i], strRecv, 1499, 0))
						{
							printf("recv失败，错误码为：%d\n", WSAGetLastError());
							continue;
						}
						printf("接收的数据：%s\n", strRecv);
					}
					else
					{
						printf("socket error套接字错误，错误码为：%d\n", NetworkEvents.iErrorCode[FD_READ_BIT]);
						continue;
					}
				}

				if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				{
					//打印
					printf("client close\n");
					printf("client force out客户端强制退出，错误码：%d\n", NetworkEvents.iErrorCode[FD_CLOSE_BIT]);
					//清理下线的客户端 套接字 事件
					closesocket(esSet[j].sockall[i]);
					esSet[j].sockall[i] = esSet[j].sockall[esSet[j].count - 1];
					WSACloseEvent(esSet[j].eventall[i]);
					esSet[j].eventall[i] = esSet[j].eventall[esSet[j].count - 1];
					esSet[j].count--;
				}
			}
		}
	}


	//释放socket,释放事件句柄
	for (int j = 0; j < 20; j++)
	{
		for (int i = 0; i < esSet[j].count; i++)
		{
			closesocket(esSet[j].sockall[i]);
			WSACloseEvent(esSet[j].eventall[i]);
		}
	}
	//清理网络库
	WSACleanup();
	system("pause");
	return 0;
}
*/

/*
->事件机制  事件选择模型  ->重叠I/O模型
C/S -> select
->消息机制  异步选择模型  ->完成端口模型
*/
