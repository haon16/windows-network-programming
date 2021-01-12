//时间：2019年8月5日15:46:15
//事件选择模型-有序处理之事情组从最小索引值询问

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

struct fd_es_set esSet;

//回调函数
BOOL WINAPI fun(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
		for (int i = 0; i < esSet.count; i++)
		{
			closesocket(esSet.sockall[i]);
			WSACloseEvent(esSet.eventall[i]);
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
		WSACloseEvent(eventServer);
		closesocket(socketServer);
		WSACleanup();
	}

	//装进去
	esSet.eventall[esSet.count] = eventServer;
	esSet.sockall[esSet.count] = socketServer;
	esSet.count++;


	while (1)
	{
		//询问事件
		DWORD nRes = WSAWaitForMultipleEvents(esSet.count, esSet.eventall, FALSE, WSA_INFINITE, FALSE);

		if (WSA_WAIT_FAILED == nRes)
		{
			printf("询问事件失败，错误码为%d\n", WSAGetLastError());
			break;
		}
		DWORD nIndex = nRes - WSA_WAIT_EVENT_0;   //此时得到的是有信号事件中索引值最小的

		//有序处理方法2，此索引值前的事件都是无信号的，可以直接从此处开始循环接下来的所有事件，处理有信号的事件，优化2比优化1更好一点
		for (int i = nIndex; i < esSet.count; i++)
		{
			DWORD nRes = WSAWaitForMultipleEvents(1, &esSet.eventall[i], FALSE, 0, FALSE);
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
			if (SOCKET_ERROR == WSAEnumNetworkEvents(esSet.sockall[i], esSet.eventall[i], &NetworkEvents))
			{
				printf("得到下标对应的具体操作失败，错误码：%d\n", WSAGetLastError());
				break;
			}

			if (NetworkEvents.lNetworkEvents & FD_ACCEPT)  
			{
				if (0 == NetworkEvents.iErrorCode[FD_ACCEPT_BIT])
				{
					//正常处理
					SOCKET socketClient = accept(esSet.sockall[i], NULL, NULL);
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

					//装进结构体
					esSet.sockall[esSet.count] = socketClient;
					esSet.eventall[esSet.count] = wsaClientEvent;
					esSet.count++;

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
					if (SOCKET_ERROR == send(esSet.sockall[i], "connect success", strlen("connect success"), 0))  
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
					if (SOCKET_ERROR == recv(esSet.sockall[i], strRecv, 1499, 0)) 
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
				closesocket(esSet.sockall[i]);
				esSet.sockall[i] = esSet.sockall[esSet.count - 1];
				WSACloseEvent(esSet.eventall[i]);
				esSet.eventall[i] = esSet.eventall[esSet.count - 1];
				esSet.count--;
			}
		}
		
	}
	//释放事件句柄//释放socket
	for (int i = 0; i < esSet.count; i++)
	{
		closesocket(esSet.sockall[i]);
		WSACloseEvent(esSet.eventall[i]);
	}
	
	//清理网络库
	WSACleanup();
	system("pause");
	return 0;
}


/*
->事件机制  事件选择模型  ->重叠I/O模型
C/S -> select
->消息机制  异步选择模型  ->完成端口模型
*/
