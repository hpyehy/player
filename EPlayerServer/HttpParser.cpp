#include "HttpParser.h"

CHttpParser::CHttpParser()
{
	// 表示本次 HTTP 报文是否完整接收完成（用于判断 http_parser_execute 的完整性）
	m_complete = false;

	// 初始化 http_parser 结构体（全部清零，防止脏数据）
	memset(&m_parser, 0, sizeof(m_parser));

	// 将 this 指针传入 parser.data，供回调函数中使用（parser->data → 转回 CHttpParser 实例）
	m_parser.data = this;

	// 初始化 HTTP parser 状态机为“请求解析”模式（HTTP_REQUEST / HTTP_RESPONSE / HTTP_BOTH）
	http_parser_init(&m_parser, HTTP_REQUEST);

	// 清空 http_parser_settings 结构体（即所有回调函数设置为 NULL）
	memset(&m_settings, 0, sizeof(m_settings));

	// 设置当报文开始时触发的回调
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin;

	// 设置当解析到 URL 时触发的回调（如：GET /login?user=...）
	m_settings.on_url = &CHttpParser::OnUrl;

	// 设置当解析到状态码时触发（仅对响应有效，解析请求时通常不会触发）
	m_settings.on_status = &CHttpParser::OnStatus;

	// 设置当解析到 header 字段名时触发（如 Host、User-Agent）
	m_settings.on_header_field = &CHttpParser::OnHeaderField;

	// 设置当解析到 header 字段值时触发（如 192.168.1.100:5000）
	m_settings.on_header_value = &CHttpParser::OnHeaderValue;

	// 设置当整个 header 解析完（检测到 \r\n\r\n）时触发
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;

	// 设置当解析到 body（请求体）时触发（如 POST 请求中携带 JSON）
	m_settings.on_body = &CHttpParser::OnBody;

	// 设置当整个 HTTP 报文解析完毕时触发
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete;
}

CHttpParser::~CHttpParser()
{}

CHttpParser::CHttpParser(const CHttpParser& http)
{
	memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
	m_parser.data = this;
	memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
	m_status = http.m_status;
	m_url = http.m_url;
	m_body = http.m_body;
	m_complete = http.m_complete;
	m_lastField = http.m_lastField;
}

CHttpParser& CHttpParser::operator=(const CHttpParser& http)
{
	if (this != &http) {
		memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
		m_parser.data = this;
		memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
		m_status = http.m_status;
		m_url = http.m_url;
		m_body = http.m_body;
		m_complete = http.m_complete;
		m_lastField = http.m_lastField;
	}
	return *this;
}

size_t CHttpParser::Parser(const Buffer& data)
{
	m_complete = false;
	//http_parser_settings m_settings 的作用就是绑定并注册解析过程中每个阶段对应的回调函数
	size_t ret = http_parser_execute(
		&m_parser, &m_settings, data, data.size());
	if (m_complete == false) {
		m_parser.http_errno = 0x7F;
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageBegin();
}

int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnUrl(at, length);
}

int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnStatus(at, length);
}

int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderField(at, length);
}

int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderValue(at, length);
}

int CHttpParser::OnHeadersComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnHeadersComplete();
}

int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnBody(at, length);
}

int CHttpParser::OnMessageComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageComplete();
}

int CHttpParser::OnMessageBegin()
{
	return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
	m_url = Buffer(at, length);
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length);
	return 0;
}

int CHttpParser::OnMessageComplete()
{
	m_complete = true;
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
}

int UrlParser::Parser()
{
	//分三步：协议、域名和端口、uri、键值对
	//解析协议
	const char* pos = m_url;
	const char* target = strstr(pos, "://");
	if (target == NULL)return -1;
	m_protocol = Buffer(pos, target);//将 :// 之前的部分提取为协议,即http 或者 https

	//解析域名和端口
	pos = target + 3;
	target = strchr(pos, '/');// 查找https://后面的第一个 / ，用于区分域名部分和 URI 部分

	//说明 URL 仅由`协议://域名`组成，没有端口，例如 http://example.com ,此时直接赋值 m_host = pos;并返回
	if (target == NULL) {
		if (m_protocol.size() + 3 >= m_url.size())
			return -2;
		m_host = pos;//此时 pos 就指向域名的位置
		return 0;
	}

	//target存在说明端口号存在，处理端口号
	Buffer value = Buffer(pos, target);// pos 现在位于 "://"之后，target 位于 / 的位置，截取的这一段就是 域名+端口(主机部分)
	if (value.size() == 0)return -3;
	target = strchr(value, ':');//冒号前是域名，后是端口号
	if (target != NULL) {
		m_host = Buffer(value, target);//冒号前是域名
		m_port = atoi(Buffer(target + 1, (char*)value + value.size()));//冒号后是端口号
	}
	else {
		m_host = value;
	}
	pos = strchr(pos, '/');// pos指向 / 的位置

	//解析uri (路径,在 /***? 的范围内就是uri )
	target = strchr(pos, '?');//查找?（uri的结束，查询参数的开始）
	if (target == NULL) {
		m_uri = pos;//查询结束，只有路径，没有查询参数，不必继续
		return 0;
	}
	else {
		// 获取路径之后，找出查询参数(键值对形式 "? user=admin & token=1234" )
		m_uri = Buffer(pos, target);
		//解析key和value
		pos = target + 1;//pos 指向 ？的下一个位置
		const char* t = NULL;
		do {
			target = strchr(pos, '&');//通过 & 分割多个 key=value
			if (target == NULL) {
				t = strchr(pos, '=');//用于找到 = 符号(键=值)
				if (t == NULL)return -4;
				m_values[Buffer(pos, t)] = Buffer(t + 1);
			}
			else {
				Buffer kv(pos, target);
				t = strchr(kv, '=');
				if (t == NULL)return -5;
				m_values[Buffer(kv, t)] = Buffer(t + 1, (char*)kv + kv.size());
				pos = target + 1;
			}
		} while (target != NULL);
	}
	return 0;
}

Buffer UrlParser::operator[](const Buffer& name) const
{
	auto it = m_values.find(name);
	if (it == m_values.end())return Buffer();
	return it->second;
}

void UrlParser::SetUrl(const Buffer& url)
{
	m_url = url;
	m_protocol = "";
	m_host = "";
	m_port = 80;
	m_values.clear();
}
