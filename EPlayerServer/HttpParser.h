#pragma once
#include "Public.h"
#include "http_parser.h"
#include <map>

class CHttpParser
{
private:
	http_parser m_parser;
	http_parser_settings m_settings;
	std::map<Buffer, Buffer> m_HeaderValues;
	Buffer m_status;
	Buffer m_url;
	Buffer m_body;
	bool m_complete;
	Buffer m_lastField;
public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
public:
	size_t Parser(const Buffer& data);
	//GET POST ... 参考http_parser.h HTTP_METHOD_MAP宏
	unsigned Method() const { return m_parser.method; }
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; }
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:
	static int OnMessageBegin(http_parser* parser);
	static int OnUrl(http_parser* parser, const char* at, size_t length);
	static int OnStatus(http_parser* parser, const char* at, size_t length);
	static int OnHeaderField(http_parser* parser, const char* at, size_t length);
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
	static int OnHeadersComplete(http_parser* parser);
	static int OnBody(http_parser* parser, const char* at, size_t length);
	static int OnMessageComplete(http_parser* parser);
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
};

class UrlParser
{
public:
	UrlParser(const Buffer& url);
	~UrlParser() {}
	int Parser();
	Buffer operator[](const Buffer& name)const;
	Buffer Protocol()const { return m_protocol; }
	Buffer Host()const { return m_host; }
	//默认返回80
	int Port()const { return m_port; }
	void SetUrl(const Buffer& url);
	const Buffer Uri()const { return m_uri; }
private:
	//假设URL为: https:// www.example.com:8080/path/to/page?user=admin&token=1234
	Buffer m_url;                 // 存储完整的 URL
	Buffer m_protocol;            // "https"			//存储协议 (http, https, ftp等)
	Buffer m_host;                // "www.example.com"  //存储主机名 (如：www.example.com)
	Buffer m_uri;                 // "/path/to/page"	URI（统一资源标识符）是 HTTP 请求中路径部分，表示客户端想访问的“资源”
	int m_port;                   // 8080				//存储端口号(默认80)
	std::map<Buffer, Buffer> m_values;  //{ "user": "admin", "token": "1234" }	//存储查询参数 (键值对形式)

};