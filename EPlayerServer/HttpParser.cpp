#include "HttpParser.h"

CHttpParser::CHttpParser()
{
	// ��ʾ���� HTTP �����Ƿ�����������ɣ������ж� http_parser_execute �������ԣ�
	m_complete = false;

	// ��ʼ�� http_parser �ṹ�壨ȫ�����㣬��ֹ�����ݣ�
	memset(&m_parser, 0, sizeof(m_parser));

	// �� this ָ�봫�� parser.data�����ص�������ʹ�ã�parser->data �� ת�� CHttpParser ʵ����
	m_parser.data = this;

	// ��ʼ�� HTTP parser ״̬��Ϊ�����������ģʽ��HTTP_REQUEST / HTTP_RESPONSE / HTTP_BOTH��
	http_parser_init(&m_parser, HTTP_REQUEST);

	// ��� http_parser_settings �ṹ�壨�����лص���������Ϊ NULL��
	memset(&m_settings, 0, sizeof(m_settings));

	// ���õ����Ŀ�ʼʱ�����Ļص�
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin;

	// ���õ������� URL ʱ�����Ļص����磺GET /login?user=...��
	m_settings.on_url = &CHttpParser::OnUrl;

	// ���õ�������״̬��ʱ������������Ӧ��Ч����������ʱͨ�����ᴥ����
	m_settings.on_status = &CHttpParser::OnStatus;

	// ���õ������� header �ֶ���ʱ�������� Host��User-Agent��
	m_settings.on_header_field = &CHttpParser::OnHeaderField;

	// ���õ������� header �ֶ�ֵʱ�������� 192.168.1.100:5000��
	m_settings.on_header_value = &CHttpParser::OnHeaderValue;

	// ���õ����� header �����꣨��⵽ \r\n\r\n��ʱ����
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;

	// ���õ������� body�������壩ʱ�������� POST ������Я�� JSON��
	m_settings.on_body = &CHttpParser::OnBody;

	// ���õ����� HTTP ���Ľ������ʱ����
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
	//http_parser_settings m_settings �����þ��ǰ󶨲�ע�����������ÿ���׶ζ�Ӧ�Ļص�����
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
	//��������Э�顢�����Ͷ˿ڡ�uri����ֵ��
	//����Э��
	const char* pos = m_url;
	const char* target = strstr(pos, "://");
	if (target == NULL)return -1;
	m_protocol = Buffer(pos, target);//�� :// ֮ǰ�Ĳ�����ȡΪЭ��,��http ���� https

	//���������Ͷ˿�
	pos = target + 3;
	target = strchr(pos, '/');// ����https://����ĵ�һ�� / �����������������ֺ� URI ����

	//˵�� URL ����`Э��://����`��ɣ�û�ж˿ڣ����� http://example.com ,��ʱֱ�Ӹ�ֵ m_host = pos;������
	if (target == NULL) {
		if (m_protocol.size() + 3 >= m_url.size())
			return -2;
		m_host = pos;//��ʱ pos ��ָ��������λ��
		return 0;
	}

	//target����˵���˿ںŴ��ڣ�����˿ں�
	Buffer value = Buffer(pos, target);// pos ����λ�� "://"֮��target λ�� / ��λ�ã���ȡ����һ�ξ��� ����+�˿�(��������)
	if (value.size() == 0)return -3;
	target = strchr(value, ':');//ð��ǰ�����������Ƕ˿ں�
	if (target != NULL) {
		m_host = Buffer(value, target);//ð��ǰ������
		m_port = atoi(Buffer(target + 1, (char*)value + value.size()));//ð�ź��Ƕ˿ں�
	}
	else {
		m_host = value;
	}
	pos = strchr(pos, '/');// posָ�� / ��λ��

	//����uri (·��,�� /***? �ķ�Χ�ھ���uri )
	target = strchr(pos, '?');//����?��uri�Ľ�������ѯ�����Ŀ�ʼ��
	if (target == NULL) {
		m_uri = pos;//��ѯ������ֻ��·����û�в�ѯ���������ؼ���
		return 0;
	}
	else {
		// ��ȡ·��֮���ҳ���ѯ����(��ֵ����ʽ "? user=admin & token=1234" )
		m_uri = Buffer(pos, target);
		//����key��value
		pos = target + 1;//pos ָ�� ������һ��λ��
		const char* t = NULL;
		do {
			target = strchr(pos, '&');//ͨ�� & �ָ��� key=value
			if (target == NULL) {
				t = strchr(pos, '=');//�����ҵ� = ����(��=ֵ)
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
