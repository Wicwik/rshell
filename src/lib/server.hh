#pragma once

#include <sys/socket.h>
#include <netinet/in.h>

class Server
{
public:
	Server(std::string ip, std::string port)
		: m_ip{ip}
		, m_port{port}
	{
	}

	void init(int opt)
	{
		if ((m_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		{
		    std::cerr << "rshell server: error while creating socket." << std::endl;
		    exit(1);
		}

		if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	    {
	        std::cerr << "rshell server: error setting socket options." << std::endl;
	        exit(1);
	    }

	    struct sockaddr_in address;
	}

private:
	std::string m_ip;
	std::string m_port;
	int m_server_fd;

};