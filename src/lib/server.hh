#pragma once

#include <iostream>

class Server
{
public:
	Server(std::string ip, std::string port)
		: m_ip{ip}
		, m_port{port}
	{
	}

private:
	std::string m_ip;
	std::string m_port;

}