#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// server class handling sockets and connections
class Server
{
public:
	// constructor needs only port number
	Server(int port)
		: m_port{port}
	{
	}

	void init(int opt)
	{
		// create server socket
		if ((m_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		{
		    std::cerr << "rshell server: error while creating socket." << std::endl;
		    exit(1);
		}

		// set options to reuse port
		if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))
	    {
	        std::cerr << "rshell server: error setting socket options." << std::endl;
	    }

	    // seet address to any internal and desired port
		m_address.sin_family = AF_INET;
		m_address.sin_addr.s_addr = INADDR_ANY;
		m_address.sin_port = htons(m_port);

		// bind address and port to socket file descriptor
		if (bind(m_server_fd, (struct sockaddr *)&m_address, sizeof(m_address)) < 0)
	    {
	        std::cerr << "rshell server: error while binding socket to address." << std::endl;
	        exit(1);
	    }

	    // set listening state with backlog 5
	    std::cout << "rshell server: entering listening state." << std::endl;
	    if (listen(m_server_fd, 5) < 0)
		{
			std::cerr << "rshell server: error while setting server to listening state." << std::endl;
			exit(1);
		}
	    
	}

	// wait on accepting connection
	void wait_for_connection()
	{
		while (true)
		{
			std::cout << "rshell server: waiting for connection." << std::endl;
			int addrlen = sizeof(m_address);
			if ((m_client_socket = accept(m_server_fd, (struct sockaddr *)&m_address, (socklen_t*)&addrlen)) < 0)
		    {
		    	std::cerr << "rshell server: error while accepting client's connection." << std::endl;
		        exit(1);
		    }

		    std::cout << "rshell server: got connection from IP address " << get_ip((struct sockaddr_in *)&m_address) << "." << std::endl;

		    // after accept, for new process to handle connections
		    int pid;
		    if ((pid=fork()) == 0)
		    {
		    	m_client_ip = get_ip((struct sockaddr_in *)&m_address);
		    	close(m_server_fd);
		    	return;
		    }
		    std::cout << "rshell server: forked handler with PID: " << pid << "." << std::endl;
		    // parent returns to waiting for connection

		    close(m_client_socket);
		}
		
	}

	// getter for client IP adress
	std::string get_client_ip()
	{
		return m_client_ip;
	}

	// read command from client
	std::pair<std::string, int> read_cmd()
	{
		char buffer[1024]; // max 1024
		int bytesread = read(m_client_socket, buffer, sizeof(buffer));
	    return std::pair<std::string, int>(std::string(buffer), bytesread);
	}

	// send end to client to stop waiting for response
	void send_end()
	{
		char message[1] = {0}; // end is 0 char
		send(m_client_socket , message , 1 , 0);
	}

	// get ip address string from in_addr struct
	std::string get_ip(struct sockaddr_in* ip_addr_in)
	{
		struct in_addr ip_addr = ip_addr_in->sin_addr;
		char str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);

		return std::string(str);
	}

	// getter for cleint socket
	int get_client_socket()
	{
		return m_client_socket;
	}

	// send prompt to client
	void send_prompt(std::string prompt)
	{
		send(m_client_socket , prompt.c_str(), prompt.size()+1 , 0);
	}

private:
	int m_port; // server port
	int m_server_fd; // server socket file descriptor
	int m_client_socket; // client socket file descriptor
	struct sockaddr_in m_address; // address of server

	std::string m_client_ip; // client ip in string

};