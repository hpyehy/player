#include "CServer.h"
#include "Logger.h"

CServer::CServer()
{
	m_server = NULL;
	m_business = NULL;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)
{
	int ret = 0;
	if (business == NULL)return -1;
	m_business = business;
	//设置业务子进程：交由业务类处理客户端逻辑
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process);
	if (ret != 0)return -2;
	ret = m_process.CreateSubProcess();//这里会在子进程中运行上面绑定的 BusinessProcess
	if (ret != 0)return -3;
	ret = m_pool.Start(2);
	if (ret != 0)return -4;
	ret = m_epoll.Create(2);
	if (ret != 0)return -5;

	m_server = new CSocket();
	if (m_server == NULL)return -6;
	ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISREUSE));
	if (ret != 0)return -7;
	ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
	if (ret != 0)return -8;
	for (size_t i = 0; i < m_pool.Size(); i++) {
		ret = m_pool.AddTask(&CServer::ThreadFunc, this);
		if (ret != 0)return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != NULL) {
		usleep(10);
	}
	return 0;
}

int CServer::Close()
{
	if (m_server) {
		CSocketBase* sock = m_server;
		m_server = NULL;
		m_epoll.Del(*sock);
		delete sock;
	}
	m_epoll.Close();
	m_process.SendFD(-1);
	m_pool.Close();
	return 0;
}

int CServer::ThreadFunc()
{
	TRACEI("epoll %d server %p", (int)m_epoll, m_server);
	int ret = 0;
	EPEvents events;
	while ((m_epoll != -1) && (m_server != NULL)) {
		ssize_t size = m_epoll.WaitEvents(events, 500);
		if (size < 0)break;
		if (size > 0) {
			TRACEI("size=%d event %08X", size, events[0].events);
			for (ssize_t i = 0; i < size; i++)
			{
				if (events[i].events & EPOLLERR) {
					break;
				}
				else if (events[i].events & EPOLLIN) {
					//这里只处理客户端的连接请求，然后发送
					if (m_server) {
						CSocketBase* pClient = NULL;
						//Link 调用 accept 接收客户端连接生成 fd，*pClient = new CSocket(fd);储存这个fd
						ret = m_server->Link(&pClient);
						if (ret != 0)continue;
						//ret = m_process.SendFD(*pClient);
						// 服务器把客户端连接的 fd 发送给 CEdoyunPlayerServer，在那边进行监听和处理
						// 与 Threadpool相比，就是不在这里处理任务，而是换一个地方处理任务的区别
						ret = m_process.SendSocket(*pClient, *pClient);
						TRACEI("SendSocket %d", ret);
						int s = *pClient;
						delete pClient;
						if (ret != 0) {
							TRACEE("send client %d failed!", s);
							continue;
						}
					}
				}
			}
		}
	}
	TRACEI("服务器终止");
	return 0;
}
