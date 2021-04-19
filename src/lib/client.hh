#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>

class Client
{
public:
	Client(std::string ip, int port)
		: m_ip{ip}
		, m_port{port}
	{
	}

	void init()
	{
		if ((m_server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			std::cerr << "rshell client: error while creating socket." << std::endl;
		    exit(1);
		}

		m_address.sin_family = AF_INET;
    	m_address.sin_port = htons(m_port);

    	if(inet_pton(AF_INET, m_ip.c_str(), &m_address.sin_addr)<=0) 
    	{
    		std::cerr << "rshell client: error, bad IP address." << std::endl;
		    exit(1);
    	}

    	std::cout << "rshell client: connecting to " << m_ip << " at port " << m_port << "." << std::endl;
    	if (connect(m_server_socket, (struct sockaddr *)&m_address, sizeof(m_address)) < 0)
    	{
    		std::cerr << "rshell client: error, connection failed." << std::endl;
		    exit(1);
    	}
	}

	void send_cmd(std::string line)
	{
		send(m_server_socket , line.c_str() , line.size()+1 , 0);
	}

	void disconnect()
	{
		std::string message = "ByeBye";
		send(m_server_socket , message.c_str() , message.size()+1 , 0);
	}

	std::string read_response()
	{
		char response[4096];
		while (true)
		{
			int bytesread = read(m_server_socket, response, sizeof(response));

			if (bytesread > 0)
			{
				return std::string(response);
			} 
		}
	}

private:
	std::string m_ip;
	int m_port;
	int m_server_socket;
	struct sockaddr_in m_address;

};