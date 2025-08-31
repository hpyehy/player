#pragma once
#include "Logger.h"
#include "CServer.h"
#include "HttpParser.h"
#include "Crypto.h"
#include "MysqlClient.h"
#include "jsoncpp/json.h"
#include <map>

DECLARE_TABLE_CLASS(edoyunLogin_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")  //QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "18888888888", "")  //手机
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")    //姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")    //昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_register_time, DEFAULT, "DATETIME", "", "", "")
DECLARE_TABLE_CLASS_EDN()

/*
* 1. 客户端的地址问题
* 2. 连接回调的参数问题
* 3. 接收回调的参数问题
*/
#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));continue;}

class CEdoyunPlayerServer :public CBusiness{
private:
	CEpoll m_epoll;
	std::map<int, CSocketBase*> m_mapClients;
	CThreadPool m_pool;
	unsigned m_count;
	CDatabaseClient* m_db;

public:
	CEdoyunPlayerServer(unsigned count) :CBusiness() {
		m_count = count;
	}

	~CEdoyunPlayerServer() {
		if (m_db) {
			CDatabaseClient* db = m_db;
			m_db = NULL;
			db->Close();
			delete db;
		}
		m_epoll.Close();
		m_pool.Close();
		for (auto it : m_mapClients) {
			if (it.second) {
				delete it.second;
			}
		}
		m_mapClients.clear();
	}

	// 开始业务逻辑
	virtual int BusinessProcess(CProcess* proc) {
		using namespace std::placeholders;
		int ret = 0;
		m_db = new CMysqlClient();
		if (m_db == NULL) {
			TRACEE("no more memory!");
			return -1;
		}
		//初始化数据库连接（MySQL）
		KeyValue args;
		args["host"] = "192.168.1.100";
		args["user"] = "root";
		args["password"] = "123456";
		args["port"] = 3306;
		args["db"] = "edoyun";
		ret = m_db->Connect(args);
		ERR_RETURN(ret, -2);
		//创建用户表 `edoyunLogin_user_mysql`(根据上面的宏)
		edoyunLogin_user_mysql user;
		ret = m_db->Exec(user.Create());
		ERR_RETURN(ret, -3);

		//设置连接/接收回调
		ret = setConnectedCallback(&CEdoyunPlayerServer::Connected, this, _1);//当新客户端连接时触发，打印客户端 IP 和端口，用于观察连接。
		ERR_RETURN(ret, -4);
		ret = setRecvCallback(&CEdoyunPlayerServer::Received, this, _1, _2);//当客户端发送数据时触发(通过下面的回调函数)
		ERR_RETURN(ret, -5);
		//启动 epoll 和线程池
		ret = m_epoll.Create(m_count);//这里的 m_count 其实没有什么意义，大于0即可
		ERR_RETURN(ret, -6);
		ret = m_pool.Start(m_count);
		ERR_RETURN(ret, -7);
		for (unsigned i = 0; i < m_count; i++) {
			ret = m_pool.AddTask(&CEdoyunPlayerServer::ThreadFunc, this);
			ERR_RETURN(ret, -8);
		}
		int sock = 0;
		sockaddr_in addrin;
 
		while (m_epoll != -1) {
			//循环调用 `proc->RecvSocket()` 接收主进程中 SendSocket 发送过来的客户端连接的 socket，用于监听客户端发送的请求
			//接收 addrin 用于下面 Init 初始化客户端 连接信息
			ret = proc->RecvSocket(sock, &addrin);
			TRACEI("RecvSocket ret=%d", ret);
			if (ret < 0 || (sock == 0))break;
			//只有有客户端连接进来的时候才会进入下面的代码
			CSocketBase* pClient = new CSocket(sock);
			if (pClient == NULL)continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));//初始化客户端连接
			WARN_CONTINUE(ret);
			//把客户端连接添加到 epoll，并调用 Connected(pClient) 回调
			ret = m_epoll.Add(sock, EpollData((void*)pClient));
			if (m_connectedcallback) {
				(*m_connectedcallback)(pClient);// Connected 函数
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}

private:
	//打印客户端 IP 和端口，用于观察连接
	int Connected(CSocketBase* pClient) {
		//TODO:客户端连接处理 简单打印一下客户端信息
		sockaddr_in* paddr = *pClient;
		TRACEI("client connected addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}

	//接收请求 → 解析验证 → 返回响应
	int Received(CSocketBase* pClient, const Buffer& data) {
		//HTTP 解析
		int ret = 0;
		Buffer response = "";
		ret = HttpParser(data);//客户端发来的数据进行 HTTP 解析(如提取 method、URI、参数)
		TRACEI("HttpParser ret=%d", ret);
		//验证结果的反馈
		if (ret != 0) {//验证失败
			TRACEE("http parser failed!%d", ret);
		}
		response = MakeResponse(ret);//根据 ret 的值(成功或失败)生成一个带有状态信息的 HTTP 响应字符串(带 JSON 内容)
		ret = pClient->Send(response);//将响应发送给客户端，这样客户端就知道是否登录成功了,成功写日志，失败也记录错误
		if (ret != 0) {
			TRACEE("http response failed!%d [%s]", ret, (char*)response);
		}
		else {
			TRACEI("http response success!%d", ret);
		}
		return 0;
	}

	// 对客户端发来的完整 HTTP 字符串解析，得到各部分的内容(如提取 method、URI、参数)
	int HttpParser(const Buffer& data) {
		CHttpParser parser;
		//解析 HTTP 请求头、URI、方法，返回 size 表示解析的字节数
		size_t size = parser.Parser(data);// HttpParser.cpp 50行, 调用 http_parser_execute
		//解析失败
		if (size == 0 || (parser.Errno() != 0)) {
			TRACEE("size %llu errno:%u", size, parser.Errno());
			return -1;
		}

		if (parser.Method() == HTTP_GET) {//目前只处理 GET 请求
			//get 处理
			UrlParser url("https://192.168.1.100" + parser.Url());//构造完整 URL: https:/192.168.1.100/<url-path>
			int ret = url.Parser();//拆解协议、主机、端口、URI、键值对
			if (ret != 0) {
				TRACEE("ret = %d url[%s]", ret, "https://192.168.1.100" + parser.Url());
				return -2;
			}
			//URI（统一资源标识符）是 HTTP 请求中路径部分，表示客户端想访问的“资源”
			Buffer uri = url.Uri();//返回解析出 m_uri ，这里是login
			TRACEI("**** uri = %s", (char*)uri);
			if (uri == "login") {
				/*	客户端发起登录请求，提供用户名（user）、时间戳（time）、随机字符串（salt），再用它们加密生成 sign 签名。
					服务端解析 uri 判断是登录，验证签名以确认请求是否可信。*/

					//处理登录
				Buffer time = url["time"];
				Buffer salt = url["salt"];//salt 是一个客户端随机生成的字符串，用来增强签名的安全性（防止重放攻击）
				Buffer user = url["user"];//用户名
				Buffer sign = url["sign"];//sign 是客户端通过特定算法（比如 MD5）对登录信息进行签名生成的字符串，用于身份验证
				TRACEI("time %s salt %s user %s sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);

				//数据库的查询
				edoyunLogin_user_mysql dbuser;//构造一个表结构对象，对应MySQL 中的已存在的表 edoyunLogin_user_mysql
				Result result;
				//生成查询语句 SELECT * FROM edoyunLogin_user_mysql WHERE user_name="xxx"
				Buffer sql = dbuser.Query("user_name=\"" + user + "\"");

				ret = m_db->Exec(sql, result, dbuser);
				if (ret != 0) {
					TRACEE("sql=%s ret=%d", (char*)sql, ret);
					return -3;
				}
				if (result.size() == 0) {
					TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
					return -4;
				}
				if (result.size() != 1) {
					TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
					return -5;
				}
				//查询结果中取出密码字段 user_password 
				auto user1 = result.front();//正常情况下查询结果只有一条
				Buffer pwd = *user1->Fields["user_password"]->Value.String;

				TRACEI("password = %s", (char*)pwd);
				//登录请求的验证
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer md5str = time + MD5_KEY + pwd + salt;//使用服务端的密钥拼接字符串
				Buffer md5 = Crypto::MD5(md5str);
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign) {
					return 0;
				}
				return -6;
			}
		}
		else if (parser.Method() == HTTP_POST) {
			//post 处理
		}
		return -7;
	}

	//根据业务处理的返回值 ret，构造一个完整的 HTTP 响应字符串,
	//包括 状态码（status）文本信息（message）响应头（如时间、服务器信息、内容长度等）响应体（JSON 格式）
	Buffer MakeResponse(int ret) {
		//使用 Json::Value 构造 JSON 格式的响应体
		Json::Value root;
		root["status"] = ret;
		if (ret != 0) {
			root["message"] = "登录失败，可能是用户名或者密码错误！";
		}
		else {
			root["message"] = "success";
		}

		Buffer json = root.toStyledString();
		Buffer result = "HTTP/1.1 200 OK\r\n";//构造 HTTP 头部字段
		//获取当前时间 → 添加 Date: 字段
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;

		//指定服务器名、内容类型、防止网页被 iframe 引用
		Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
		//添加安全性字段与空行（标志头部结束）
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";

		result += Date + Server + Length + Stub + json;//拼接完整响应内容
		/*
		HTTP/1.1 200 OK
		Date: xxxxxx
		Server: Edoyun/1.0
		Content-Type: text/html; charset=utf-8
		X-Frame-Options: DENY
		Content-Length: xxx
		X-Content-Type-Options: nosniff
		Referrer-Policy: same-origin

		{ "status": 0, "message": "success" }
		*/
		TRACEI("response: %s", (char*)result);
		return result;
	}

private:
	int ThreadFunc() {
		int ret = 0;
		EPEvents events;
		while (m_epoll != -1) {
			ssize_t size = m_epoll.WaitEvents(events);
			if (size < 0)break;
			if (size > 0) {
				for (ssize_t i = 0; i < size; i++)
				{
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
						if (pClient) {
							Buffer data;
							ret = pClient->Recv(data);//这里收到的就是客户端发来的 http 请求
							TRACEI("recv data size %d", ret);
							if (ret <= 0) {
								TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));
								m_epoll.Del(*pClient);
								continue;
							}
							if (m_recvcallback) {
								(*m_recvcallback)(pClient, data);//调用 Received 接收客户端发送来的数据
							}
						}
					}
				}
			}
		}
		return 0;
	}
};