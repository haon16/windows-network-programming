//时间：2019年7月30日16:32:25
/*
select模型
1.解决基本c / s模型中，accept recv傻等的问题   1）解决傻等阻塞
											   2）不解决执行阻塞 send recv accept在执行过程中都是阻塞的，所以注意，select模型是解决傻等的问题的，不解决这几个函数本身的阻塞问题
2.实现多个客户端链接，与多个客户端分别通信
3.用于服务器，客户端就不用这个了，因为只有一个socket   客户端recv等待的的时候，也不能send ：只需要单独创建一根线程，recv放线程里

select模型逻辑
1.每个客户端都有socket, 服务器也有自己的socket, 将所有的socket装进一个数据结构里，即数组
2.通过select函数，遍历1中的socket数组，当某个socket有响应，select就会通过其参数 / 返回值反馈出来
3.做相应处理   1）如果检测到的是服务器socket, 那就是有客户端链接，调用accept    
			   2）如果检测到的是客户端socket, 那就是客户端请求通信，调用send或recv
*/


//#define FD_SETSIZE 128     //FD_SETSIZE默认是64，可以在winsock2.h前声明这个宏，就可以让select模型处理更多的socket,不要过大，select原理就是遍历检测，越多肯定效率越低，延迟越大，select模型应用就是小用户量访问量
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>    //头文件
#pragma comment(lib, "ws2_32.lib")   //库文件

fd_set allSockets;   
//回调函数：系统调用我们的函数
BOOL WINAPI fun(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
		//释放所有socket
		for (u_int i = 0; i < allSockets.fd_count; i++)   //这边要调用allSockets所以定义成全局的
		{
			closesocket(allSockets.fd_array[i]);
		}
		//清理网络库
		WSACleanup();
	}
	return TRUE;
}

int main()
{
	//控制台窗口点X退出，让操作系统帮我们释放数据   主函数投递一个监视
	SetConsoleCtrlHandler(fun, TRUE);   //(fun true):操作系统执行默认的函数处理默认的东西，然后调用fun函数，把我们的东西处理掉进行释放    如果fun改为NULL,系统会做一些默认的事
	                                    //（fun false）:操作系统执行默认的函数处理默认的东西,不调用fun函数
	//打开网络库
	WSADATA wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		printf("网络库打开失败!\n");
		return 0;
	}

	//校验版本号
	if (2 != HIBYTE(wsadata.wVersion) || 2 != LOBYTE(wsadata.wVersion))
	{
		printf("版本号有误!\n");
		if (SOCKET_ERROR == WSACleanup())
			printf("错误码：%d\n", WSAGetLastError());
		return 0;
	}

	//创建服务器socket
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   //地址类型，套接字类型，协议类型
	if (INVALID_SOCKET == socketServer)
	{
		printf("服务器socket创建失败,错误码：%d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	//绑定IP地址和端口
	struct sockaddr_in socketMsg;
	socketMsg.sin_family = AF_INET;			//协议
	socketMsg.sin_port = htons(12345);		//端口
	socketMsg.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  //地址
	if (SOCKET_ERROR == bind(socketServer, (const struct sockaddr*)&socketMsg, sizeof(socketMsg)))
	{
		printf("绑定失败，错误码：%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	//开始监听，监听客户端来的链接
	if (SOCKET_ERROR == listen(socketServer, SOMAXCONN))   //参数2是挂起连接队列的最大长度，SOMAXCONN是让系统自动选择最合适的个数
	{
		printf("监听失败，错误码：%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	/*
		select
		第一步：定义一个装客户端socket结构
		fd_set allSockets;
		
		FD_ZERO(&allSockets); 集合清零

		FD_SET(socketServer, &allSockets);  集合添加一个元素 当socket数量不足64且此socket不存在的时候

		FD_CLR(socketServer, &clientSockets);  集合中删除指定的socket    我们要手动释放closesocket

		int a = FD_ISSET(socketServer, &clientSockets);   判断一个socket是否在集合中  不在返回0，在返回非0

		第二步：select
		作用：监视socket集合，如果某个socket发生事件（链接或者收发数据），通过返回值以及参数告诉我们
		select();      参数2处理accept与recv傻等问题，这是select结构比基本模型得最大得作用，参数3，4一般可以为NULL
		参数1. 忽略，此处填0，为了兼容berkeley sockets 向下兼容旧标准
		参数2.（1）检查是否有可读的socket：即客户端发来消息了，该socket就会被设置
			  （2）&setRead   初始化为所有的socket,通过select投放给系统，系统将有事件发生的socket再赋值回来，调用后，这个参数就只剩下有请求的socket

		参数3.（1）检查是否有可写的socket：就是可以给哪些客户端套接字发消息，即send，只要链接成功建立起来，那该客户端套接字就是可写的
			  （2）&setWrite  初始化为所有的socket，通过select投放给系统，系统将可写的socket再赋值回来，调用后，这个参数就是装着可以被send数据的客户端socket.一般我们就直接send了，所以这个参数逻辑上，用的不是非常多

		参数4.（1）检查套接字上的异常错误：用法跟2.3一样，将有异常错误的套接字重新装进来，反馈给我们
			  （2）得到异常套接字上的具体错误码  getsockopt

		参数5.（1）最大等待时间  比如当客户端没有请求时，那么select函数可以等一会，一段时间过后还没有，就继续执行select下面的语句，如果有就立即执行下面的语句
			  （2）TIMEVAL  两个成员:tv_sec 秒，tv_usec 微秒   如果是0 0 则是非阻塞状态，立即返回   如果3 4 就是无客户端响应的情况下等待3秒4微妙
		      （3）NULL select完全阻塞  直到客户端有反应，才继续

		返回值（1）0 客户端再等待时间内没有反应 处理 continue就行了  （2）>0   有客户端请求交流了  （3）SOCKET_ERROR 发生了错误， WSAGetLastError()得到错误码
	*/
	

	//fd_set allSockets;
	FD_ZERO(&allSockets);
	FD_SET(socketServer, &allSockets);   //服务器装进去

	while (1)
	{
		//临时socket集合，调用select函数后只返回有响应的socket,集合中的socket就变了，所以不能直接使用保存所有socket的allsockets
		fd_set readSockets = allSockets;		//select处理参数2，可读的socket集合
		fd_set writeSockets = allSockets;		//select处理参数3，可写的socket集合
		//FD_CLR(socketServer, &writeSockets);  //测试参数2时，可把服务器socket先删除，也可没有，不去删除
		fd_set errorSockets = allSockets;		//select处理参数4，异常错误集合

		//时间段
		struct timeval st;
		st.tv_sec = 3;
		st.tv_usec = 0;

		//select
		int nRes = select(0, &readSockets, &writeSockets, &errorSockets, &st);   //测试：参数2.3.4分别测试，测试参数3时只要有客户端链接服务器，select函数结果就会大于0，循环执行大于0的分支

		if (0 == nRes)		//没有响应
		{
			continue;
		}
		else if (nRes > 0)   //有响应
		{
			//处理异常错误集合
			for (u_int i = 0; i < errorSockets.fd_count; i++)   
			{
				char str[100] = { 0 };
				int len = 99;
				if (SOCKET_ERROR == getsockopt(errorSockets.fd_array[i], SOL_SOCKET, SO_ERROR, str, &len))
				{
					printf("无法得到错误信息\n");
				}
				printf("%s\n", str);
			}
			//处理可写集合
			for (u_int i = 0; i < writeSockets.fd_count; i++)   
			{
				//printf("服务器%d,%d:可写\n", socketServer, writeSockets.fd_array[i]);    //printf测试，发现printf不会打印writeSockets中的服务器socket
				if (SOCKET_ERROR == send(writeSockets.fd_array[i], "ok", 2, 0))		//如果有发送信息send返回值只有大于0和SOCKET_ERROR两种情况，客户端正常退出或强制退出时此处send都返回SOCKET_ERROR
				{																	//如果什么都不发，send返回0
					int a = WSAGetLastError();
				}
			}
			//处理可读集合,客户端关闭退出也在这处理
			for (u_int i = 0; i < readSockets.fd_count; i++)  
			{
				if (readSockets.fd_array[i] == socketServer)  
				{
					//accept
					SOCKET socketClient = accept(socketServer, NULL, NULL);
					if (INVALID_SOCKET == socketClient)
					{
						//链接出错
						continue;
					}
					FD_SET(socketClient, &allSockets);
					printf("链接成功\n");
					//send
				}
				else
				{
					//客户端
					char strBuf[1500] = { 0 };
					int nRecv = recv(readSockets.fd_array[i], strBuf, 1500, 0);
					if (0 == nRecv)
					{
						//客户端下线了
						//从集合中拿掉
						SOCKET socketTemp = readSockets.fd_array[i];
						FD_CLR(readSockets.fd_array[i], &allSockets);
						//释放
						closesocket(socketTemp);
					}
					else if (nRecv > 0)
					{
						//接收到了消息
						printf(strBuf);
					}
					else  //SOCK_ERROR
					{
						//强制下线也是出错 10054
						int a = WSAGetLastError();
						switch (a)
						{
						case 10054:
							{
								SOCKET socketTemp = readSockets.fd_array[i];
								FD_CLR(readSockets.fd_array[i], &allSockets);
								closesocket(socketTemp);
								break;
							}
						}
					}
				}
			}
		}
		else
		{
			//发生错误了
			//根据具体的错误码进行处理，包括结束while循环，或者等待或者continue下一次循环
			break;
		}
	}

	//释放所有socket
	for (u_int i = 0; i < allSockets.fd_count; i++)
	{
		closesocket(allSockets.fd_array[i]);
	}
	//清理网络库
	WSACleanup();
	system("pause");
	return 0;    //正常关闭控制台窗口
}



/*
流程总结    
socket集合   select判断有没有响应的	1.返回0：没有，继续挑
									2.返回>0:有响应的   （1）可读的（accept, recv） 
														（2）可写的 send 不是非得从此处调用send  
														（3）异常的 getsockopt
									3.SOCKET_ERROR  函数本身执行错误

执行阻塞
假如发送1500个字节，send把1500个字节复制到协议缓冲区，复制的过程是卡死的，如果复制中途有客户端响应处理不了，给多个客户端send消息只能一个个send, 
我们自己写的函数也是执行阻塞，但是调用频率低，也不会有大量的数据复制，send / recv调用频率高，可能导致等待时间太久

select是阻塞的  1.不等待    执行阻塞 + 时间非阻塞状态	参数5时间设置为0，遍历完就立即下一次循环，但是遍历集合时不能干别的  傻等  没有目标的等待
				2.半等待	执行阻塞 + 软阻塞			参数5时间设置为不为0，比如3s，遍历集合完再等待3s					傻等  没有目标的等待
				3.全等待	执行阻塞 + 硬阻塞			参数5时间设置为NULL, 直到有客户端发消息或链接						死等  等到可用的目标响应
*/


