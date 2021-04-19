#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class Server
{
public:
	Server(int port)
		: m_port{port}
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

		m_address.sin_family = AF_INET;
		m_address.sin_addr.s_addr = INADDR_ANY;
		m_address.sin_port = htons(m_port);

		if (bind(m_server_fd, (struct sockaddr *)&m_address, sizeof(m_address)) < 0)
	    {
	        std::cerr << "rshell server: error while binding socket to address." << std::endl;
	        exit(1);
	    }

	    std::cout << "rshell server: entering listening state." << std::endl;
	    if (listen(m_server_fd, 3) < 0)
		{
			std::cerr << "rshell server: error while setting server to listening state." << std::endl;
			exit(1);
		}

		std::cout << "rshell server: waiting for connection." << std::endl;
		int addrlen = sizeof(m_address);
		if ((m_client_socket = accept(m_server_fd, (struct sockaddr *)&m_address, (socklen_t*)&addrlen)) < 0)
	    {
	    	std::cerr << "rshell server: error while accepting client's connection." << std::endl;
	        exit(1);
	    }

	    std::cout << "rshell server: got connection from IP address " << get_ip((struct sockaddr_in *)&m_address) << "." << std::endl;
	    
	}

	std::pair<std::string, int> read_cmd()
	{
		char buffer[1024];
		int bytesread = read(m_client_socket, buffer, sizeof(buffer));
	    return std::pair<std::string, int>(std::string(buffer), bytesread);
	}

	void send_response(std::string response)
	{
		
	}


	std::string get_ip(struct sockaddr_in* ip_addr_in)
	{
		struct in_addr ip_addr = ip_addr_in->sin_addr;
		char str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);

		return std::string(str);
	}

private:
	int m_port;
	int m_server_fd;
	int m_client_socket;
	struct sockaddr_in m_address;

};