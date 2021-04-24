#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>

// client class handling sockets and connections
class Client
{
public:
	// constructor needs IP address and port
	Client(std::string ip, int port)
		: m_ip{ip}
		, m_port{port}
	{
	}

	// initiate client
	void init()
	{
		// create server socket
		if ((m_server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			std::cerr << "rshell client: error while creating socket." << std::endl;
		    exit(1);
		}

		m_address.sin_family = AF_INET;
    	m_address.sin_port = htons(m_port);

    	// parse string IP address to address struct
    	if(inet_pton(AF_INET, m_ip.c_str(), &m_address.sin_addr)<=0) 
    	{
    		std::cerr << "rshell client: error, bad IP address." << std::endl;
		    exit(1);
    	}

    	// connect to server
    	std::cout << "rshell client: connecting to " << m_ip << " at port " << m_port << "." << std::endl;
    	if (connect(m_server_socket, (struct sockaddr *)&m_address, sizeof(m_address)) < 0)
    	{
    		std::cerr << "rshell client: error, connection failed." << std::endl;
		    exit(1);
    	}
	}

	// send command to server
	void send_cmd(std::string line)
	{
		send(m_server_socket , line.c_str() , line.size()+1 , 0);
	}

	// disconnect from server
	void disconnect()
	{
		std::string message = "ByeBye";
		send(m_server_socket , message.c_str() , message.size()+1 , 0);
	}

	// read response from server
	std::string read_response()
	{
		char response[5000000];
		std::string str_response;

		while (true)
		{
			int bytesread = read(m_server_socket, response, sizeof(response)-1);

			// if end was recieved
			if (bytesread == 1 && response[0] == 0)
			{
				return str_response;
			}

			// add response to response string
			if (bytesread > 0)
			{
				response[bytesread] = '\0';
				str_response += std::string(response);
			} 
		}
	}

	// request and read prompt from server
	std::string read_server_prompt()
	{
		std::string message = "prompt";
		send(m_server_socket , message.c_str() , message.size()+1 , 0); // request

		char response[5000000];
		std::string str_response;

		while (true)
		{
			int bytesread = read(m_server_socket, response, sizeof(response)-1); // response

			if (bytesread > 0)
			{
				response[bytesread] = '\0';
				return std::string(response);
			} 
		}
	}

private:
	std::string m_ip; // server IP address
	int m_port; // server port
	int m_server_socket; // server socket
	struct sockaddr_in m_address; // server address structure

};