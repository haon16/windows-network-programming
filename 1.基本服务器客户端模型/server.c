//时间：2019年7月27日16:55:47
//server

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <WinSock2.h>					//打开该文档可看到版本信息
#pragma comment(lib, "ws2_32.lib")		//windows socket第二版32位，lib是源文件编译好的二进制文件
//#include <WinSock.h>    对应库 wsock32.lib 


int main(void)
{
	//1.打开网络库
	WORD wdVersion = MAKEWORD(2, 2);				//创建版本关键字，高字节放副版本号，低字节放主版本号
	//int a = *((char *)&wdVersion);				//结果2，取wdVersion的低地址位
	//int b = *((char *)&wdVersion + 1);			//结果1，取wdVersion的高地址位

	WSADATA wdSockMsg;									//第一种写法，创建变量直接取地址
	//LPWSADATA lpw = malloc(sizeof(WSADATA));			//第二种写法，创建指针变量指向动态分配WSADATA，要free,LPWSADATA类型是WSADATA* 

	int nRes = WSAStartup(wdVersion, &wdSockMsg);			//网络库启动函数，第一个参数是输入的版本关键字，第二个参数是指向WSADATA结构体的地址，启动后把相关信息保存在这个结构体上
														/*
															MAKEWORD(2, 2),当输入的版本不存在：
															（1）输入1.3，2.3 有主版本，没有副版本  得到该主版本的最大副版本1.1，2.2并使用
															（2）输入3.1，3.3 超过最大版本号 使用系统能提供的最大的版本2.2
															（3）输入0.0 0.1 0.1 主版本是0 网络库打开失败，不支持请求的套接字版本

															typedef struct WSAData {
															WORD                    wVersion;		我们要使用的版本
															WORD                    wHighVersion;	系统提供给我们的最高版本
															#ifdef _WIN64
															unsigned short          iMaxSockets;	返回可用的socket的数量，2版本之后就没用了
															unsigned short          iMaxUdpDg;		UDP数据报信息的大小，2版本之后就没用了
															char FAR *              lpVendorInfo;	供应商特定的信息，2版本之后就没用了
															char                    szDescription[WSADESCRIPTION_LEN+1];   当前版本库的描述信息
															char                    szSystemStatus[WSASYS_STATUS_LEN+1];   当前状态
															#else
															......
															} WSADATA, FAR * LPWSADATA;
														*/
	//if (WSAStarup(wdVerison, &wdSockMsg) != 0)
	if (0 != nRes)					//如果网络库打开成功，返回值是0
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
		if (SOCKET_ERROR == WSACleanup())			//正常返回0
		{
			int a = WSAGetLastError();				//清理失败可用该函数获取错误码，利用错误码可查找错误信息
		}
		return 0;
	}

	//3.创建SOCKET
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);		//SOCKET是一种数据类型，一个整数，但是是唯一的，标识着当前的应用程序，协议特点等信息，ID,门牌号   SOCKET构成=IP地址＋端口
																			//每一个客户端有一个SOCKET，服务器有一个SOCKET，通信时，就需要这个SOCKET做参数，给谁通信就传递谁的SOCKET
																			//socket():参数1：地址的类型，参数2：套接字的类型，参数3：协议的类型
	int a = WSAGetLastError();					//正常为0，错误会返回错误码
	if (INVALID_SOCKET == socketServer)			//创建失败
	{
		//获取错误码，
		int a = WSAGetLastError();				//WSAGetLastError获取最近的函数的错误码
		//清理网络库
		WSACleanup();
		return 0;
	}

	//4.绑定地址与端口
	struct sockaddr_in si;
	si.sin_family = AF_INET;								//跟创建socket时的地址类型对应 ipv4:AF_INET ipv6:AF_INET6
	si.sin_port = htons(12345);								//输入的整数转换成unsigned short  本地字节序转成网络字节序  (理论范围0-65535)  (0-1023为系统保留占用端口号 )   //netstat -ano查看端口使用情况  //netstat -aon|findstr "12345"查看端口是否被使用
	si.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");		//127.0.0.1本地回环地址，回送地址    //ipconfig
	/*si.sin_addr.S_un.S_un_b.s_b1 = 192;
	si.sin_addr.S_un.S_un_b.s_b2 = 168;
	si.sin_addr.S_un.S_un_b.s_b3 = 0;
	si.sin_addr.S_un.S_un_b.s_b4 = 1;*/

	if (SOCKET_ERROR == bind(socketServer, (const struct sockaddr*)&si, sizeof(si)))		//参数2类型是指向const struct sockaddr结构体的指针,sockaddr不好写入地址端口 ->写入sockaddr_in结构体再强转，内存结构是一样的
	{																						//sockaddr :  u_short sa_family; CHAR sa_data[14];
		//出错了																			//sockaddr_in :short sin_family; USHORT sin_port; IN_ADDR sin_addr; CHAR sin_zero[8];
		int a = WSAGetLastError();
		//释放
		closesocket(socketServer);				//先释放socket再关闭网络库，否则会报错
		//清理网络库
		WSACleanup();
		return 0;
	}

	//5.开始监听， 监听客户端来的链接     将套接字置于正在侦听传入连接的状态
	if (SOCKET_ERROR == listen(socketServer, SOMAXCONN))			//参数2是挂起连接队列的最大长度，比如有100个用户链接请求，但是系统一次只能处理20个，剩下80个不能不理人家，所以系统就创建个队列记录这些暂时不能处理，一会处理的链接请求，依先后顺序处理   
	{																//SOMAXCONN是让系统自动选择最合适的个数
		//出错了
		int a = WSAGetLastError();
		//释放
		closesocket(socketServer);
		//清理网络库
		WSACleanup();
		return 0;
	}

	//6.创建客户端socket      listen监听客户端来的链接，将客户端的信息绑定到一个socket上，也就是给客户端创建一个socket,通过返回值返回给我们客户端的socket
	struct sockaddr_in clientMsg;				//这个我们不用填写，系统会帮我们填写，也即传址调用
	int len = sizeof(clientMsg);
	SOCKET socketClient = accept(socketServer, (struct sockaddr *)&clientMsg, &len);    //一次只能创建一个，有几个客户端链接，就要调用几次， 这个函数如果没有客户端链接会一直卡在这，阻塞
																						/*
																							参数2：系统帮我们监视着客户端的动态，肯定会记录客户端的信息，也就是IP地址和端口号，并通过这个结构体记录
																							参数3：参数2的大小，
																							参数2，3也能置成NULL，accept函数不直接得到客户端的地址，端口号，接受下一个客户端信息就会覆盖掉当前的信息

																							如果此时不接受，也可在接下来调用getpeername函数获得客户端信息
																							SOCKET socketClient = accept(socketServer, NULL, NULL);
																							getpeername(socketClient, (struct sockaddr *)&clientMsg, &len);
																							得到本地服务器信息：getsockname()
																							getsockname(socketServer, (struct sockaddr *)&clientMsg, &len);   //即使此处填的客户端信息，获取的也是本地服务器的信息
																						*/														
	if (INVALID_SOCKET == socketClient)                                                
	{                                                                                    
		printf("客户端链接失败");                                                       
		//出错了
		int a = WSAGetLastError();
		//释放
		closesocket(socketServer);
		//清理网络库
		WSACleanup();
		return 0;
	}
	printf("客户端链接成功\n");

	if (SOCKET_ERROR == send(socketClient, "我是服务器，我收到了你的消息", sizeof("我是服务器，我收到了你的消息"), 0))
	{
		//出错了
		int a = WSAGetLastError();
		//根据实际情况处理， 根据错误码信息做相应处理：重启/等待/不用理会
	}

	while(1)
	{
		//7.服务端接收客户端信息:得到指定客户端（参数1）发来的消息
		//原理：本质是复制：1.数据的接受都是有协议本身做的，也就是socket的底层做的，系统有一段缓冲区，存储着接受到的数据 2.咱们外边调用recv的作用，就是通过socket找到这个缓冲区，并把数据复制进参数2，复制参数3个
		char buf[1500] = { 0 };
		int res = recv(socketClient, buf, 1499, 0);          
																	/*
																		参数1：客户端的socket，每个客户端对应唯一的socket
																		参数2：客户端消息的存储空间，也就是字符数组      一般1500字节，网络传输的最大单元是1500字节，也就是客户端发过来的数据，一次最大就是1500字节，这个协议规定，总结的最优值，所以客户端一组最多来1500字节，服务器这端1500读一次就够了
																		参数3：想要读取的字节数，一般是参数2的字节数-1，把\0字符串结尾留出来， %s可直接输出
																		参数4：数据的读取方式：一般填0		正常逻辑来说（这种逻辑填0）：1.我们从系统缓冲区把数据读到buf后，系统缓冲区的被读的就应该被删除掉了，不然也是浪费空间，毕竟通信时间长的话，那就爆炸了。
																																		 2.我们从系统缓冲区把数据读到buf，根据需要处理相应的数据，这是我们可控的，系统缓冲区的数据则操作不了
																																		 3.读出来就删的话，有很多好处 1.系统缓冲区读到的数据，比我们的buf多，那么我们读出来的，系统删掉，从而我们就可以依次把所有的数据读完，如果不删，每次都是从头读，则循环里就是每次都是某个字符数组比如“abcd”只读到这四个   2.可以计数收到了多少字节， 因为返回值就是本次读出来的数据
																											参数4：MSG_PEEK   1.窥视传入的数据，数据将复制到缓冲区中，但从不会从输入队列中删除  2.不建议使用：读数据不行/无法计数
																											参数4：MSG_OOB   带外数据 意义：就是传输一段数据，在外带一个额外的特殊数据，相当于小声BB   实际：不建议使用 1.TPC协议规范（RFC793）中OOB的原始描述被“主机要求”规范取代（RFC1122）,但仍有许多机器具有RFC793 OOB实现 2.既然两种数据，完全可以send两次，另一方recv两次，不然还要特殊处理外带数据
																											参数4：MSG_WAITALL 直到系统缓冲区字节数满足参数3所请求的字节数，才开始读取
																		返回值是读出来的字节数大小   读没了？在recv函数卡着，等着客户端发来数据，即阻塞，同步
																	*/
		if (0 == res)							//客户端下线，会释放客户端socket，这端返回0，        
		{
			printf("链接中断，客户端下线\n");
		}
		else if (SOCKET_ERROR == res)			//执行失败，返回SOCKET_ERROR
		{
			//出错了
			int a = WSAGetLastError();
			//根据实际情况处理， 根据错误码信息做相应处理：重启/等待/不用理会
		}
		else
		{
			printf("%d  %s\n", res, buf);
		}

		//7.服务端给客户端发送数据
		//作用：向目标发送数据   本质：send函数将我们得数据复制粘贴进系统的协议发送缓冲区，计算机伺机发送出去，最大传输单元是1500字节
		scanf("%s", buf);
		int b = send(socketClient, buf, strlen(buf), 0);			
																/*
																	参数1：目标的socket，每个客户端对应唯一的socket
																	参数2：给目标发送字节串
																		（1）这个不要超过1500字节:
																			1）.发送时候，协议要进行包装，加上协议信息，也叫协议头，或者叫包头。
																			2）.这个大小不同的协议不一样，链路层14字节，ip头20字节，tcp头20字节，数据结尾还要有状态确认，加起来也几十个字节，所以实际咱们的数据位不能写1500个，要留出来，那就1024或者最多1400就差不多了
																		（2）超过1500个字节系统会分片处理
																			1）.比如2000个字节,系统会分成两个包，假设包头100字节 第一次1400+包头 ==1500， 第二次600+包头==700，分两次发送出去
																			2）.结果：1.系统要给咱们分包再打包，再发送，客户端接收到了还得拆包，组合数据，从而增加了系统得工作，降低效率   2.有的协议，就把分片后得二包直接丢了
																	参数3. 字节个数
																	参数4. 发送规则，一般写0     1.MSG_OOB同recv,   2.MSG_DONTROUTE  指定数据不应受路由限制，windows套接字服务提供程序可以忽略此标志
																	返回值：成功返回写入的字节数，执行失败返回SOCKET_ERROR
																*/
																		
		if (SOCKET_ERROR == b)
		{
			//出错了
			int a = WSAGetLastError();
			//根据实际情况处理， 根据错误码信息做相应处理：重启/等待/不用理会
		}

	}
	closesocket(socketClient);
	closesocket(socketServer); //WSAGetLastError获取最近的函数的错误码
	//清理网络库
	WSACleanup();
	system("pause");
	return 0;
}


/*
int WSAAPI WSAStartup(_In_ WORD wVersionRequested, _Out_ LPWSADATA lpWSAData)
SOCKET WSAAPI socket(_In_ int af, _In_ int type, _In_ int protocol)
int WSAAPI bind(_In_ SOCKET s, _In_reads_bytes_(namelen) const struct sockaddr FAR * name, _In_ int namelen)
int WSAAPI listen(_In_ SOCKET s, _In_ int backlog)
SOCKET WSAAPI accept(_In_ SOCKET s, _Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR * addr, _Inout_opt_ int FAR * addrlen)
int WSAAPI recv(_In_ SOCKET s, _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf, _In_ int len, _In_ int flags)
int WSAAPI send(_In_ SOCKET s, _In_reads_bytes_(len) const char FAR * buf, _In_ int len, _In_ int flags)

WSAAPI 调用约定 可以忽略给系统看的，跟咱们无关
调用约定决定了：1.函数名字的编译方式 2.参数的入栈顺序 3.函数的调用时间
*/

/*
在C/C++写网络程序的时候，往往会遇到字节的网络顺序和主机顺序的问题。这是就可能用到htons(), ntohl(), ntohs()，htons()这4个函数。
网络字节顺序与本地字节顺序之间的转换函数：

htonl()--"Host to Network Long"
ntohl()--"Network to Host Long"
htons()--"Host to Network Short"
ntohs()--"Network to Host Short"

之所以需要这些函数是因为计算机数据表示存在两种字节顺序：NBO与HBO

网络字节顺序NBO(Network Byte Order): 按从高到低的顺序存储，在网络上使用统一的网络字节顺序，可以避免兼容性问题。

主机字节顺序(HBO，Host Byte Order): 不同的机器HBO不相同，与CPU设计有关，数据的顺序是由cpu决定的,而与操作系统无关。
如 Intel x86结构下, short型数0x1234表示为34 12, int型数0x12345678表示为78 56 34 12
如 IBM power PC结构下, short型数0x1234表示为12 34, int型数0x12345678表示为12 34 56 78
*/