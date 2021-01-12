//时间：2019年8月3日16:46:15
//事件选择模型

/*
windows处理用户行为的两种方式：  1.消息机制：核心是消息队列   处理过程：所有的用户操作，比如点鼠标，摁键盘，点软件上的按钮。。等等，所有操作均依次按顺序被记录，装进一个队列
									（整体使用）				 特点：（1）消息队列由操作系统维护，咱们做的操作，然后把消息取出来，分类处理  
																	   （2）有先后顺序
																  其他：win32,MFC都是基于消息队列，异步选择模型也是基于这个消息队列的

								2.事件机制：核心是事件集合	  处理过程：（1）根据需求，我们为用户的特定操作绑定一个事件，事件由我们自己调用API创建，需要多少创建多少
									（局部使用）						（2）将事件投递给系统，系统就帮咱们监视着，所以不能无限创建，太多系统运行就卡了
																	    （3）如果操作发生了，比如用户点击鼠标了，那么对应的事件就会被置成有信号，也就是类似1变2了，用个数标记
																		（4）我们直接获取到有信号的事件，然后处理
																  特点：（1）所有事件都是咱们自己定义的，系统只是帮咱们置有无信号，所以我们自己掌管定义 
																		（2）无序的
																  其他：事件选择就是应用这个

事件选择的逻辑 WSAEventSelect  整体逻辑跟select差不多,进化版
第一步：创建一个事件对象（变量）  WSACreateEvent
第二步：为每一个事件对象绑定个socket以及操作accept,read,close并投递给系统   
		(1)投递给系统，咱们就完全不用管了，系统自己监管 ，咱们就去做别的事去了  
	   （2）WSAEventSelect
第三步：查看事件是否有信号   WSAWaitForMultipleEvents
第四步：有信号的话就分类处理  WSAEnumNetworkEvents
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

//定义事件集合以及socket集合的结构体
struct fd_es_set
{
	unsigned short count;
	SOCKET sockall[WSA_MAXIMUM_WAIT_EVENTS];      //WSAWaitForMultipleEvents所能询问的一组事件的最大个数是WSA_MAXIMUM_WAIT_EVENTS，是64，所以事件数组最大也设置64，多了也询问不了，所以可以多组多次询问或者一次询问一个，不以组的形式询问
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

	//struct fd_es_set esSet = {0};  定义成全局变量以便回调函数使用

	//创建事件                                                      
	WSAEVENT eventServer =  WSACreateEvent();   //成功返回一个事件      
												/*
													WSAEVENT->HANDLE->void*   句柄：1.ID
																					2.内核对象： 操作系统是一个大的应用程序exe，运行360，QQ以及我们写的EventSelect可以理解为函数，在操作系统上运行该应用就类似于调用函数，
																								 我们在EventSelect内定义a就相当于局部变量,内核对象相当于系统在系统的空间里申请的变量，相当于全局的，只能由操作系统申请以及操作
																					（1）由系统在内核申请
																					（2）由操作系统访问
																					（3）我们不能定位其内容，也不能修改，不能在编写其他程序去修改   
																							1）void*是通用类型指针，不知道是什么类型，不能解引用取内容，只能先强转再解引用，不知道类型不能修改    
																							2）对内核的保护，对规则的保护，从而使操作系统有序平稳有效的运行，而不会随便出问题
																					（4）调用系统函数创建，调用系统函数释放        如果我们没有调用释放，那么他可能就一直存在于内核，造成内核内存泄漏，占用的是操作系统的空间（全局的，所有程序都在用的），单单重启软件是没法释放的，这种只能重启电脑 （比如QQ申请12345端口，360就申请不了12345端口）
																					（5）内核对象有哪些：socket，thread,event,file都是内核对象（Kernel Objects）
												*/
	
	if (WSA_INVALID_EVENT == eventServer)   //失败返回WSA_INVALID_EVENT
	{
		printf("创建事件失败，错误码为%d\n", WSAGetLastError());
		closesocket(socketServer);
		WSACleanup();
		return 0;
	}
	//WSAResetEvent(eventServer);   //将指定事件主动置成无信号      重置WSAEventSelect函数使用的事件对象状态的正确方法是将事件对象的句柄传递给hEventObject参数中的WSAEnumNetworkEvents函数，这将重置事件对象并以原子方式调整套接字上活动FD事件的状态
	//WSASetEvent(eventServer);   //将指定事件主动置成有信号

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
												/*
												功能：给事件绑上socket与操作码，并投递给操作系统
												参数1：被绑定的socket   最终，每个socket都会绑定一个事件
												参数2：事件对象   逻辑就是将参数1与参数2绑定在一起
												参数3：具体事件:    （1）FD_ACCEPT		有客户端链接		与服务器socket绑定
																	（2）FD_READ		有客户端发来消息	与客户端socket绑定  可多个属性并列用|，WSAEventSelect(,,FD_ACCEPT);  WSAEventSelect(,,FD_READ);如果前后调用两次后一次绑定会覆盖掉前一次
																	（3）FD_CLOSE		客户端下线了		与客户端socket绑定  包含强制下线，正常下线
																	（4）FS_WRITE		可以给客户端发信息  与客户端socket绑定  会在accept后立即产生该信号，可以说明客户端连接成功，即可随时send
																	（5）FD_CONNECT		客户端事件模型使用，客户端一方，给服务器绑定这个
																	（6）0				取消事件监视，事件和socket还是绑定的             WSAEventSelect(,,FD_ACCEPT|FD_READ)； WSAEventSelect(,,0);  取消了事件信号
																	（7）FD_OOB			带外数据   一般不使用
																	（8）FD_QOS			1）套接字服务质量状态发生变化消息通知  比如：当网络发生拥堵时：用户下载，看电影，聊天，听歌好多用网事件一起再做，那么计算机网速是有限的，每秒可以处理多少数据，这时候计算机就会把要紧事优先，比如降低下载的速度，以保证看电影流畅，这时候，下载的服务质量就发生了变化。如果投放了这个事件就会接收到信号了。
																						2）可以通过WSAloctl得到服务质量信息      char strOut[2048] = {0};  DWORD nLen = 2048; WSAloctl(socketServer,SIO_QOS,0,0,strout,nLen,&nLen,NULL,NULL)
																	（9）FD_GROUP_QOS   保留     还没有对其赋值具体意义，还没用呢    想要接收套接字组Qos更改的通知
																	（10）用在重叠I/O模型中  FD_ROUTING_INTERFACE_CHANGE    1）想要接受指定目标的路由接口更改通知
																															2）数据到达对方的所经过的线路改变了，是由于动态路线优化的选择
																															3）要通过函数WSAloct注册之后，才可使用   第二个参数改为SIO_ROUTING_INTERFACE_CHANGE
																							FD_ADDRESS_LIST_CHANGE			1）想要接收套接字地址族的本地地址列表更改通知       服务器链接了很多客户端，那服务器就记录着所有的客户端的地址信息，也就是相当于一个列表，当多一个或者少一个，就是变化了，就能得到相关的信号了
																															2）要通过函数WSAloct注册之后，才可使用   第二个参数改为SIO_ADDRESS_LIST_CHANGE
												返回值：成功返回0，失败返回SOCKET_ERROR
												*/
	

	//装进去
	esSet.eventall[esSet.count] = eventServer;
	esSet.sockall[esSet.count] = socketServer;
	esSet.count++;


	while (1)
	{
		//询问事件
		DWORD nRes = WSAWaitForMultipleEvents(esSet.count, esSet.eventall, FALSE, WSA_INFINITE, FALSE);
													/*
														作用：获取发生信号的事件
														参数1：事件个数  定义事件列表（数组）个数     
															（1）WSA_MAXIMUM_WAIT_EVENTS  64个，该函数参数1最大64个
															（2）可以变大， 方法比较复杂，不像select模型，直接就能变大，因为select本身就是个数组，然后遍历就行了，比较直接，时间选择是异步投放，由系统管理，不能随便修改了，要按规则来
														参数2：事件列表
														参数3：事件等待方式   
															（1）TRUE   所有的事件都产生信号，才返回
															（2）FALSE   1）.任何一个事件产生信号，立即返回    
																			2）.返回值减去WSA_WAIT_EVENT_0表示事件对象的索引，其状态导致函数返回   
																			3）.如果在调用期间发生多个事件对象的信号，则这是信号事件对象的数组索引，返回所有信号事件中索引值最小的
														参数4：超时间隔 以毫秒为单位，跟select参数5一样的意义        
															（1）123：等待123毫秒  期间有信号立即返回，如果超时返回WSA_WAIT_TIMEOUT
															（2）0：检查事件对象的状态并立即返回，不管有没有信号
															（3）WSA_INFINITE  等待直到事件发生
														参数5：
															（1）TRUE	重叠I/O使用
															（2）FALSE  事件选择模型填写FALSE
														返回值：（1）数组下标的运算值		
																	1）参数3为TRUE 所有的事件均有信号
																	2）参数3为FALSE 返回值减去WSA_WAIT_EVENT_0 == 数组中事件的下标
																（2）WSA_WAIT_IO_COMPLETION    参数5为TRUE,才会返回这个值
																（3）WSA_WAIT_TIMEOUT     超时了，continue即可
													*/
		
		
		if (WSA_WAIT_FAILED == nRes)
		{
			//出错了
			printf("询问事件失败，错误码为%d\n", WSAGetLastError());
			break;
		}
		//如果参数4写具体超时间隔，此时事件要加上超时判断
		//if (WSA_WAIT_TIMEOUT == nRes)
		//{
		//	continue;
		//}
		DWORD nIndex = nRes - WSA_WAIT_EVENT_0;     

		//列举事件,得到下标对应的具体操作
		WSANETWORKEVENTS  NetworkEvents;
		if (SOCKET_ERROR == WSAEnumNetworkEvents(esSet.sockall[nIndex], esSet.eventall[nIndex], &NetworkEvents))
		{
			printf("得到下标对应的具体操作失败，错误码：%d\n", WSAGetLastError());
			break;
		}
											/*
												作用：获取事件类型，并将事件上的信号重置      accept,recv,send等等
												参数1：对应的socket
												参数2：对应的事件
												参数3：（1）触发的事件类型在这里装着
														（2）是一个结构体指针   struct_WSANETWORKEVENTS{long lNetworkEvents;  int iErrorCode[FD_MAX_EVENTS];}   
																						成员1：具体操作    一个信号可能包含两个消息，以按位或的形式存在
																						成员2：错误码数组    1）FD_ACCEPT事件错误码在FD_ACCEPT_BIT下标里    
																												2）没有错误，对应的就是0
												返回值：成功  0  ； 失败 SOCKET_ERROR
											*/
		


		/*
		事件分类处理逻辑                程序打断点，虽然程序卡住了，但是事件投递给了系统，期间客户端发送给服务器的全部信息关联的事件都会产生有信号的，是异步的，  如果客户端连续send几次，服务器会一次recv全部接收，因为系统底层协议栈照常运行
		1.多if逻辑更好，更严谨，但效率低第一点    
			测试while后加断点，打开客户端发一句消息，实际执行了两次socket, 第一次lNetworkEvents = 8, 服务器socket执行accept操作，第二次是客户端socket，lNetworkEvents = 3, 服务器执行write和read
		2.if - else是多选一，有小bug, 但bug情况不多见 
			测试while后加断点，打开客户端发一句消息，实际执行了两次socket, 第一次是服务器socket，执行accept操作，第二次是客户端socket，但是因为if_else只选择其中一个信号，所以服务器只执行了write
		3.switch的话，大bug，需要很多改进  
			测试while后加断点，打开客户端发一句消息，实际执行了两次socket, 第一次是服务器socket执行accept操作，第二次过来是客户端socket，但是write和read信号是一起过来的, lNetworkEvents = 3，write是2，read是1，case匹配不到3，继续下一次循环
		*/
		if (NetworkEvents.lNetworkEvents & FD_ACCEPT)     //if结果为真说明：不管其他信号如何，FD_ACCEPT这个信号是有信号过来的
		{
			if (0 == NetworkEvents.iErrorCode[FD_ACCEPT_BIT])
			{
				//正常处理
				SOCKET socketClient = accept(esSet.sockall[nIndex], NULL, NULL);
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

		if (NetworkEvents.lNetworkEvents & FD_WRITE)       //事件选择模型做了优化，只在aceept后紧接着产生一次，成功链接后随时都可以send，不需要一直触发事件，而select模型send是一直有效的，有相应的
		{
			if (0 == NetworkEvents.iErrorCode[FD_WRITE_BIT])
			{
				//客户端链接服务器后的第一次进行初始化，后面随时都可以send，不一定需要进入if判断
				if (SOCKET_ERROR == send(esSet.sockall[nIndex], "connect success", strlen("connect success"), 0))    //strlen比sizeof得到的字节数少一个，'\0'
				{
					printf("send失败，错误码为：%d\n", WSAGetLastError());      //send函数执行出现错误
					continue;
				}
				printf("write event\n");
			}
			else
			{
				printf("socket error套接字错误，错误码为：%d\n", NetworkEvents.iErrorCode[FD_WRITE_BIT]);   //套接字出现错误
			}
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			if (0 == NetworkEvents.iErrorCode[FD_READ_BIT])
			{
				char strRecv[1500] = {0};
				if (SOCKET_ERROR == recv(esSet.sockall[nIndex], strRecv, 1499, 0))    //客户端退出在FD_CLOSE分支进行
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
			//if (0 == NetworkEvents.iErrorCode[FD_CLOSE_BIT])
			//{
			//}
			//else
			//{
			//	printf("client force out客户端强制退出，错误码：%d\n", NetworkEvents.iErrorCode[FD_CLOSE_BIT]);
			//}
			//三种返回值：WSAENETDOWN    WSAECONNRESET   其他情况都是返回WSAECONNABORTED，包括正常退出，强制退出

			//不管错误码是0是1都要清理
			//打印
			printf("client close\n");
			printf("client force out客户端强制退出，错误码：%d\n", NetworkEvents.iErrorCode[FD_CLOSE_BIT]);
			//清理下线的客户端 套接字 事件
			//套接字
			closesocket(esSet.sockall[nIndex]);
			esSet.sockall[nIndex] = esSet.sockall[esSet.count - 1];     //由于事件选择时无序的，所以直接把当前要关闭的socket和最后一个socket交换，然后数量减1
			//事件
			WSACloseEvent(esSet.eventall[nIndex]);
			esSet.eventall[nIndex] = esSet.eventall[esSet.count - 1];
			//数量减1
			esSet.count--;
		}
	}

	//释放事件句柄，释放socket
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
