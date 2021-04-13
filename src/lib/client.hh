#pragma once

class Client
{
public:
	Client(std::string ip, std::string port)
		: m_ip{ip}
		, m_port{port}
	{
	}

private:
	std::string m_ip;
	std::string m_port;

};