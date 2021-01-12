//时间：2019年8月6日21:38:03
//异步选择模型

/*
	核心：消息队列  操作系统为每个窗口创建一个消息队列并且维护，所以我们要使用消息队列，那就要创建一个窗口
	第一步：将我们的socket操作绑定在一个消息上，并投递给操作系统 WSAAsyncSelect
	第二部：取消息分类处理  如果有操作了，就会得到指定的消息
	该模型只能用于windows，主要学这种处理思想
*/

//#include <windows.h>
#include <TCHAR.H>     //_T头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#define UM_ASYNCSELECTMSG WM_USER+1      //WM_USER以下是系统定义使用的数，WM_USER之后的数是给程序员使用的

LRESULT CALLBACK WinBackProc(HWND hWnd, UINT msgID, WPARAM wparam, LPARAM lparam);

#define MAX_SOCK_COUNT 1024 
SOCKET g_sockall[MAX_SOCK_COUNT];  //保存消息处理时创建的socket
int g_count = 0;     //数组有效个数

/*
	参数一：当前运行在系统上的应用程序的实例句柄，或者说ID或编号
	参数二：前一个实例句柄，应用程序可打开多个，会把前一个实例句柄传过来，不过现在已经无效了，功能已取消
	参数三：传递命令行参数，与main主程序一样
	参数四：窗口的默认显示方式
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE HPreInstance, LPSTR lpCmdLine, int nShowCmd)
{
	//第一步，创建窗口结构体
	WNDCLASSEX wc;					//WNDCLASS和WNDCLASSEX两种结构体 ex表示额外的，多几个变量
	wc.cbClsExtra = 0;				//额外空间
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbWndExtra = 0;				//额外空间
	wc.hbrBackground = NULL;		//NULL窗口默认白色
	wc.hCursor = NULL;				//鼠标的形态,
	wc.hIcon = NULL;				//左上角的图标
	wc.hIconSm = NULL;				//缩小化的图标，底下一栏
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WinBackProc;
	wc.lpszClassName = _T("mywindow");		//窗口名字     //字符集四种解决办法，除了这三种(L, _T, TEXT("mywindow"))还有一种修改字符集，不推荐，从代码角度修改
	wc.lpszMenuName = NULL;					//菜单栏
	wc.style = CS_HREDRAW | CS_VREDRAW;     //风格

	//第二步，注册结构体     把上述类型的窗口投递给系统
	RegisterClassEx(&wc);

	//第三步，创建窗口
	HWND hWnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, TEXT("mywindow"), _T("我的窗口"), WS_OVERLAPPEDWINDOW, 200, 200, 600, 400, NULL, NULL, hInstance, NULL);
	if (NULL == hWnd)
	{
		return 0;
	}

	//第四步，显示窗口
	ShowWindow(hWnd, nShowCmd);

	//更新窗口    重新绘制窗口   非必要的
	UpdateWindow(hWnd);

	//socket初始化
	//打开网络库
	WSADATA wsadata;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		printf("网络库打开失败");
		return 0;
	}

	//检验版本
	if (2 != HIBYTE(wsadata.wVersion) || 2 != LOBYTE(wsadata.wVersion))
	{
		printf("版本错误\n");
		WSACleanup();
		return 0;
	}

	//创建服务器socket
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == socketServer)
	{
		printf("创建服务器socket失败，错误码：%d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	//绑定IP和端口
	struct sockaddr_in socketMsg;
	socketMsg.sin_family = AF_INET;
	socketMsg.sin_port = htons(12345);
	socketMsg.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	if (SOCKET_ERROR == bind(socketServer, (const struct sockaddr *)&socketMsg, sizeof(socketMsg)))
	{
		printf("绑定IP和端口失败，错误码：%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	//监听客户端来的链接
	if (SOCKET_ERROR == listen(socketServer, SOMAXCONN))
	{
		printf("监听失败，错误码:%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	//消息处理：绑定消息与socket并且投递出去
	//作用：将socket与消息绑定在一起，并投递给操作系统
	//参数一：socket
	//参数二：窗口句柄：绑定到哪个窗口上   本质就是窗口的ID,编号
	//参数三：消息编号：自定义消息    本质就是一个数
	//参数四：消息类型，与事件选择一样
	if (SOCKET_ERROR == WSAAsyncSelect(socketServer, hWnd, UM_ASYNCSELECTMSG, FD_ACCEPT))
	{
		printf("消息队列投递失败，错误码：%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}

	//装服务器socket
	g_sockall[g_count] = socketServer;
	g_count++;

	//第五步，消息循环:socket绑定消息投递给系统后，当socket发生事件或客户端有请求,这些请求以消息的形式进入消息队列，循环从消息队列中依次往外拿消息，然后分发消息
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))    //只有关闭窗口才会结束循环      //一次只取一个
	{
		TranslateMessage(&msg);    //翻译消息
		DispatchMessage(&msg);     //分发消息
	}

	//释放socket
	for (int i = 0; i < g_count; i++)
	{
		closesocket(g_sockall[i]);
	}
	//关闭网络库
	WSACleanup();

	return 0;
}

int x = 0;

//第六步，回调函数，一次只处理一个
LRESULT CALLBACK WinBackProc(HWND hWnd, UINT msgID, WPARAM wparam, LPARAM lparam)     //分发到这个函数    参数一：窗口句柄；参数二：消息ID；参数三：socket；参数四：传输的消息
{
	HDC hdc = GetDC(hWnd);   //获取当前窗口的设备环境句柄
	switch (msgID)			 //根据消息进行相应处理
	{
	case UM_ASYNCSELECTMSG:
		{   //case内部不能直接定义变量，加花括号可解决
			//测试用MessageBox(NULL, L"有信号了", L"提示", MB_OK);   //参数一：填写NULL代表弹出的窗口是独立窗口，填写hWnd代表弹出的窗口是hWnd窗口的子窗口;参数二：弹出窗口的内容；参数三：弹出窗口的标题；参数四：弹出窗口中按钮的类型
			//获取socket
			SOCKET sock = (SOCKET)wparam;   //wparam存的是socket
			//获取消息
			if (0 != HIWORD(lparam))		//lparam高位存的是产生的错误码，如果客户端发送消息，发送过程中出错了，会得到错误码，不为0就代表有错，等于0才能处理消息
			{
				if (WSAECONNABORTED == HIWORD(lparam))    //关闭客户端会执行到这
				{
					TextOut(hdc, 0, x, _T("close"), strlen("close"));
					x += 15;
					//关闭该socket上的消息
					WSAAsyncSelect(sock, hWnd, 0, 0);
					//关闭socket
					closesocket(sock);
					//记录数组中删除该socket
					for (int i = 0; i < g_count; i++)
					{
						if (sock == g_sockall[i])
						{
							g_sockall[i] = g_sockall[g_count - 1];
							g_count--;
							break;
						}
					}
				}
				break;
			}
			//具体消息
			switch (LOWORD(lparam))    //lparam低位存的是具体的消息种类
				{
				case FD_ACCEPT:
				{
					//窗口打印消息
					TextOut(hdc, 0, x, _T("accept"), strlen("accept"));    //参数一：窗口里的一部分，非客户区，设备环境句柄; 参数二三：坐标; 参数四：窗口打印的消息; 参数五：消息字节大小,不带'\0'可以刚好输出
					x += 15;
					//创建客户端socket
					SOCKET socketClient = accept(sock, NULL, NULL);
					if (INVALID_SOCKET == socketClient)
					{
						printf("创建客户端socket出错，错误码：%d\n", WSAGetLastError());
						break;
					}
					//将客户端socket投递给消息队列
					if (SOCKET_ERROR == WSAAsyncSelect(socketClient, hWnd, UM_ASYNCSELECTMSG, FD_READ|FD_WRITE|FD_CLOSE))
					{
						printf("消息队列投递失败，错误码：%d\n", WSAGetLastError());
						closesocket(socketClient);
						break;
					}
					//记录客户端socket，为了后续释放掉
					g_sockall[g_count] = socketClient;
					g_count++;
				}
					break;

				case FD_READ:
				{
					//read
					TextOut(hdc, 0, x, _T("read"), strlen("read"));
					char str[1024] = { 0 };
					if (SOCKET_ERROR == recv(sock, str, 1023, 0))
					{
						printf("获取消息失败,错误码：%d\n", WSAGetLastError());
						break;
					}
					TextOut(hdc, 50, x, str, strlen(str));     //此处输出字符串要改成多字节形式，因为比较复杂直接在字符集修改成多字节形式，L不能使用了，都改成通用的_L("")
					x += 15;
				}
					break;

				case FD_WRITE:
					//send
					TextOut(hdc, 0, x, _T("write"), strlen("write"));
					x += 15;
					break;

				case FD_CLOSE:
					TextOut(hdc, 0, x, _T("close"), strlen("close"));
					x += 15;
					//关闭该socket上的消息
					WSAAsyncSelect(sock, hWnd, 0, 0);
					//关闭socket
					closesocket(sock);
					//记录数组中删除该socket
					for (int i = 0; i < g_count; i++)
					{
						if (sock == g_sockall[i])
						{
							g_sockall[i] = g_sockall[g_count - 1];
							g_count--;
							break;
						}
					}
				}
		}
		break;

	case WM_CREATE:    //只在窗口创建的时候执行一次，初始化，网络初始化也可放在此处
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	ReleaseDC(hWnd, hdc);  //释放hdc
	return DefWindowProc(hWnd, msgID, wparam, lparam);
}



/*
优化：每个窗口维护一定的，然后创建多个线程，每个线程一个窗口，每个窗口投递一定数量的客户端
问题：在一次处理过程中，客户端产生多次send，服务器会产生多次接收消息，第一次接收消息会收完所有信息   调试：在消息循环前while加断点，客户端发送多个消息，取消断点继续，服务器只有一次read,但是把所有消息数据都打印了
*/
