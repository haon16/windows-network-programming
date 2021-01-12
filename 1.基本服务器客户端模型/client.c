//时间：2019年7月30日12:59:01
//client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WinSock2.h>   //打开该文档可看到版本信息
#pragma comment(lib, "ws2_32.lib")   //windows socket第二版32位，lib是源文件编译好的二进制文件
//#include <WinSock.h>    对应库 wsock32.lib 


int main(void)
{
	//1.打开网络库
	WORD wdVersion = MAKEWORD(2, 2);			//创建版本关键字，高字节放副版本号，低字节放主版本号
	//int a = *((char *)&wdVersion);			//结果2，取wdVersion的低地址位
	//int b = *((char *)&wdVersion + 1);		//结果1，取wdVersion的高地址位

	WSADATA wdSockMsg;								 //第一种写法，创建变量直接取地址
	//LPWSADATA lpw = malloc(sizeof(WSADATA));		 //第二种写法，创建指针变量指向动态分配WSADATA，要free,LPWSADATA类型是WSADATA* 

	int nRes = WSAStartup(wdVersion, &wdSockMsg);     //网络库启动函数，第一个参数是输入的版本关键字，第二个参数是指向WSADATA结构体的地址，启动后把相关信息保存在这个结构体上
	//if (WSAStarup(wdVerison, &wdSockMsg) != 0)
	if (0 != nRes)    //如果网络库打开成功，返回值是0
	{
		switch (nRes)
		{
		case WSASYSNOTREADY:
			printf("重启下电脑试试，或者检查网络库");
			break;
		case WSAVERNOTSUPPORTED:
			printf("请更新网络库");
			break;
		case WSAEINPROGRESS:
			printf("请重新启动软件");
			break;
		case WSAEPROCLIM:
			printf("请尝试关掉不必要的软件，以为当前网络运行提供充足资源");
			break;
			//case WSAEFAULT :   //WSAStartup第二个参数：指针参数写错了，程序员检查
			//	break;
		}
		return 0;
	}
	//printf("%s %s\n", wdSockMsg.szDescription, wdSockMsg.szSystemStatus);

	//2.校验版本
	if (2 != HIBYTE(wdSockMsg.wVersion) || 2 != LOBYTE(wdSockMsg.wVersion))
	{
		//说明版本不对，不是2.2版本
		//清理网络库,有可能清理失败
		if (SOCKET_ERROR == WSACleanup())		//正常返回0
		{
			int a = WSAGetLastError();			//清理失败可用该函数获取错误码，利用错误码可查找错误信息
		}
		return 0;
	}

	//3.创建SOCKET,服务器的socket，客户端主动去连接服务器，无别的地方来连接客户端
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);				//SOCKET是一种数据类型，一个整数，但是是唯一的，标识着当前的应用程序，协议特点等信息，ID,门牌号
																					//socket():参数1：地址的类型，参数2：套接字的类型，参数3：协议的类型
	int a = WSAGetLastError();				//正常为0，错误会返回错误码
	if (INVALID_SOCKET == socketServer)		//创建失败
	{
		//获取错误码，
		int a = WSAGetLastError();			//WSAGetLastError获取最近的函数的错误码
		//清理网络库
		WSACleanup();
		return 0;
	}

	//4.链接服务器
	//作用：链接服务器并把服务器信息与服务器socket绑定到一起
	struct sockaddr_in serverMsg;
	serverMsg.sin_family = AF_INET;
	serverMsg.sin_port = htons(12345);		//要连接服务器，端口号要跟服务器的一样
	serverMsg.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");	//服务器的ip地址

	if (SOCKET_ERROR == connect(socketServer, (struct sockaddr*)&serverMsg, sizeof(serverMsg)))
	{
		int a = WSAGetLastError();
		closesocket(socketServer);
		//清理网络库
		WSACleanup();
		return 0;
	}

	while (1)
	{
		//5.与服务器收发消息
		char buf[1500] = { 0 };
		//int res = recv(socketServer, buf, 1499, 0);

		//if (0 == res)         //客户端下线，会释放客户端socket，这端返回0，        
		//{
		//	printf("链接中断，客户端下线\n");
		//}
		//else if (SOCKET_ERROR == res)    //执行失败，返回SOCKET_ERROR
		//{
		//	//出错了
		//	int a = WSAGetLastError();
		//	//根据实际情况处理， 根据错误码信息做相应处理：重启/等待/不用理会
		//}
		//else
		//{
		//	printf("%d  %s\n", res, buf);
		//}


		//5.与服务器收发消息   
		scanf("%s", buf);
		if (0 == strcmp(buf, "0"))
			break;
		if (SOCKET_ERROR == send(socketServer, buf, strlen(buf), 0))
		{
			//出错了
			int a = WSAGetLastError();
			//根据实际情况处理， 根据错误码信息做相应处理：重启/等待/不用理会
		}
	}
	
	//清理网络库
	closesocket(socketServer);
	WSACleanup();
	system("pause");
	return 0;
}