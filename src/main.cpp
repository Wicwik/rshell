/*	
 *	Simple shell program, by Robert Belanec
 *	belanecrobert22@gmail.com
 *  I tried to write selfdescribing code.
 */

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cctype>
#include <optional>
#include <map>
#include <regex>
#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pwd.h>
#include <signal.h>

#include "lib/server.hh"
#include "lib/client.hh"


int cd(char **args)
{
	if (args[1] == 0)
	{
		std::cerr << "rhsell: expected argument to \'cd\'" << std::endl;
	}
	else
	{
		if (chdir(args[1]) != 0)
		{
			perror("rshell");
		}
	}

	return 1;
}

// handler function for server interrupt signal
void int_handler_server(int sig)
{
	std::cout << "rshell: server: recieved SIGINT(" << sig << ") - shutting down" << std::endl;
	exit(0);
}

// handler function for client interrupt signal
Client *client_for_handler;
void int_handler_client(int sig)
{
	std::cout << "\nrshell: recieved SIGINT(" << sig << ") - shutting down" << std::endl;
	client_for_handler->disconnect();
	exit(0);
}

// function to print help
void print_help()
{
	std::string help = "rshell - simple shell program, by Robert Belanec\n";

	help+= "Usage: rshell [-c address port] [-s port] [-h|a]\n\n";
	help += "-c, --client 					connects to server as a client\n";
	help += "-s, --server 					creates rshell server on specified port\n";
	help += "-h, --help 					print this message\n";
	help += "--halt							abort program\n";

	std::cout << help;
}

// function to get current time as string
std::string get_time()
{
	time_t rt;
	struct tm * t;

	time(&rt);
	t = localtime(&rt);

	return std::string(std::to_string(t->tm_hour) + ":" + std::to_string(t->tm_min));
}

// function to get hostname as string
std::string get_hostname()
{
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);

	return std::string(hostname);
}

// function to get username as string (whoami)
std::string get_username()
{
	struct passwd *pw;
	uid_t uid;

	uid = geteuid();
	pw = getpwuid(uid);

	if (pw)
	{
		return std::string(pw->pw_name);
	}

	return "";
}

// function to create simple prompt
void print_prompt()
{
	std::cout << get_time() << " " << get_username() << "@" << get_hostname() << " #> ";
}

// function to check if string is integer
bool is_int(std::string& str)
{
	try
	{
		size_t pos = 0;
		std::stoi(str, std::addressof(pos));

		while (std::isspace(str.back())) // pop spaces
		{
			str.pop_back();
		}
		
		return pos == str.size();
	}

	catch (const std::exception&) // if stoi throws exception there was a noninteger character encoutered
	{ 
		return false; 
	}
}

// function for str to in conversion using string stream
int str_to_int(std::string str)
{
	std::stringstream ss(str);
	int out = 0;
	ss >> out;

	return out;
}

// function for splitting strings on delimiter
std::vector<std::string> split_string_on_delimiter(std::string s, std::string delimiter)
{
	std::vector<std::string> split_strings;
	size_t pos = 0;

	// we need to handle backslash special character escaping for space separately
	if (delimiter == " ")
	{
		while ((pos = s.find(delimiter)) != std::string::npos)
		{
			if (pos != 0 && s[pos-1] == '\\') // if character before delimiter is 
			{
				// erase backslash character
				s.erase(std::find(s.begin() + static_cast<int>(pos-1), s.begin() + static_cast<int>(pos-1), '\\'));
			}

			// create substring and push it to the splitstrings
			split_strings.push_back(s.substr(0, pos));
			s.erase(0, pos + delimiter.length());
		}
	}
	else
	{
		// if delimiter is not space, continue find from position
		while ((pos = s.find(delimiter, pos)) != std::string::npos)
		{
			if (pos != 0 && s[pos-1] == '\\')
			{
				s.erase(std::find(s.begin() + static_cast<int>(pos-1), s.begin() + static_cast<int>(pos-1), '\\'));

				continue;
			}

			split_strings.push_back(s.substr(0, pos));
			s.erase(0, pos + delimiter.length());
		}
	}

	// push whats left to split_strings
	split_strings.push_back(s);

	return split_strings;
}


// function to remove comments
std::string remove_comments(std::string s)
{
	size_t pos = 0;

	while(true)
	{
		if ((pos = s.find("#", pos)) == std::string::npos) return s;

		// also works with special character escaping
		if (s[pos-1] == '\\')
		{
			s.erase(std::find(s.begin() + static_cast<int>(pos-1), s.begin() + static_cast<int>(pos-1), '\\'));
			pos++;

			continue;
		}
		else
		{
			return s.substr(0, pos);
		}
	}
}

// check if there is an output redirect also works with special character escaping
bool is_redirect_output(std::string &s)
{
	size_t pos = 0;
	while ((pos = s.find(">", pos)) != std::string::npos)
	{
		if (s[pos-1] == '\\')
		{	
			pos+=2;

			continue;
		}
		else
		{
			return true;
		}
	}

	pos = 0;
	
	// if there is no real >/< remove all backslashes
	while ((pos = s.find(">", pos)) != std::string::npos)
	{
		if (s[pos-1] == '\\')
		{	
			s.erase(std::find(s.begin() + static_cast<int>(pos-1), s.begin() + static_cast<int>(pos-1), '\\'));
			pos++;

			continue;
		}
	}

	return false;
}

// check if there is an input redirect also works with special character escaping
bool is_redirect_input(std::string &s)
{
	size_t pos = 0;
	while ((pos = s.find("<", pos)) != std::string::npos)
	{
		if (s[pos-1] == '\\')
		{	
			pos+=2;

			continue;
		}
		else
		{
			return true;
		}
	}

	pos = 0;

	// if there is no real >/< remove all backslashes
	while ((pos = s.find("<", pos)) != std::string::npos)
	{
		if (s[pos-1] == '\\')
		{	
			s.erase(std::find(s.begin() + static_cast<int>(pos-1), s.begin() + static_cast<int>(pos-1), '\\'));
			pos++;

			continue;
		}
	}

	return false;
}

// separate string on delimiter on two strings
std::pair<std::string, std::string> separate(std::string s, std::string delimiter)
{
	size_t pos = 0;
	while ((pos = s.find(delimiter, pos)) != std::string::npos)
	{
		if (s[pos-1] == '\\') // works with escaping
		{	
			s.erase(std::find(s.begin() + static_cast<int>(pos-1), s.begin() + static_cast<int>(pos-1), '\\'));
			pos++;

			continue;
		}
		else
		{
			return std::pair<std::string, std::string>{s.substr(0, pos), s.substr(pos+1)};
		}
	}

	return std::pair<std::string, std::string>{"", ""};
}

// function that forks another proccess and redirects stdin/stdout to in and out
int spawn_process(int in, int out, std::string cmd)
{
	int status; // process status

	// check if there are redirects
	bool redout = is_redirect_output(cmd);
	bool redin = is_redirect_input(cmd);

	// if there are file redirects redirect them
	if (redout)
	{
		std::pair<std::string, std::string> cmd_file_pair = separate(cmd, ">"); // separate on delimeter on file and cmd
		cmd = cmd_file_pair.first;

		std::string file_str = cmd_file_pair.second;
		file_str.erase(std::remove_if(file_str.begin(), file_str.end(), [](unsigned char x){return std::isspace(x);}), file_str.end()); // remove whitespaces

		// in case of output file redirect, we need to create new file, and make it write only
		out = open(const_cast<char*>(file_str.c_str()), O_WRONLY|O_CREAT|O_TRUNC, 0666); 
		if (-1 == out) std::cerr << "rshell: error while opening file output" << std::endl;
	}

	if (redin)
	{
		std::pair<std::string, std::string> cmd_file_pair = separate(cmd, "<");
		cmd = cmd_file_pair.first;

		std::string file_str = cmd_file_pair.second;
		file_str.erase(std::remove_if(file_str.begin(), file_str.end(), [](unsigned char x){return std::isspace(x);}), file_str.end()); // remove whitespaces

		in = open(const_cast<char*>(file_str.c_str()), O_RDONLY); // open input file to read only
		if (-1 == in) std::cerr << "rshell: error while opening file input" << std::endl;
	}

	// create arguments for execvp
	char **sargv = nullptr;
	std::vector<char*> cstrings;
	if (cmd != "")
	{
		std::vector<std::string> strings = split_string_on_delimiter(cmd, " "); // split strings on space
		cstrings.reserve(cstrings.size());

		// convert to vector of cstrings
		for (auto &s : strings)
		{
			if (s != "") cstrings.push_back(const_cast<char*>(s.c_str()));
		}
		cstrings.push_back(nullptr); // must be null terminated
		sargv = &cstrings[0]; // variable to be passed to execvp
	}

	if (std::string(cstrings[0]) == "cd")
	{
		cd(sargv);
		return 0;
	}

	pid_t fpid;
	switch ((fpid = fork())) // fork new process
	{
		case -1:
			std::cerr << "rshell: error while forking this process with PID: " << fpid << std::endl;
			exit(1);
		
		case 0: // if child
			if (in != 0) // redirect stdin/stdout if needed
			{
				if (-1 == dup2(in, fileno(stdin)))
				{ 
					std::cerr << "rshell: error while redirecting input" << std::endl;
					exit(1);
				}

				fflush(stdin);
				close(in);
			}

			if (out != 1)
			{
				if (-1 == dup2(out, fileno(stdout))) 
				{
					std::cerr << "rshell: error while redirecting output" << std::endl;
					exit(1);
				}

				fflush(stdout);
				close(out);
			}

			if (-1 == execvp(sargv[0], sargv)) // execute commands
			{
				std::cerr << "rshell: " << cmd << ": command not found" << std::endl;
				exit(1);
			}
			break;

		default: // if parrent, wait for process to end
			wait(&status);

	}

	return status;
}

// function that pipes commands
int pipe_cmds(std::vector<std::string> cmds, int in = fileno(stdin), int out = fileno(stdout))
{
	int fd[2]; // pipe array

	if (out != fileno(stdout)) // id we are redirecting output to socket, redirect also stderr to that socket
	{
		if (-1 == dup2(out, fileno(stderr))) std::cerr << "rshell: error while redirecting error" << std::endl;

		fflush(stderr);	
	}

	std::string last_cmd = cmds.back();
	if (cmds.size() > 1) // only if we have more than one cmd
	{	
		cmds.pop_back(); // last command will not be executed using pipe

		for (const auto &cmd : cmds)
		{
			pipe(fd); // create pipe

			spawn_process(in, fd[1], cmd); // output is end of the pipe

			close(fd[1]); // we can close the pipe output

			in = fd[0]; // next input is begining of the pipe
		}
	}

	return spawn_process(in, out, last_cmd); // spawn last process
}

// args parsing function
std::optional<std::map<std::string, std::string>> parse_args(int argc, char** argv)
{
	std::map<std::string, std::string> args;

	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];

		// if server
		if (arg == "-s" || arg == "--server")
		{
			if (!args["server_port"].empty())
			{
				std::cerr << "rshell: server port is alleardy specified with another argument." << std::endl;
				return std::nullopt;
			}

			if (!args["client_ip"].empty() || !args["client_port"].empty())
			{
				std::cerr << "rshell: cannot be a server and a client at the same time." << std::endl;
				return std::nullopt;
			}

			if (i == argc - 1 || argc < 3)
			{
				std::cerr << "rshell: " << arg << " parameter requires port number." << std::endl;
				return std::nullopt;
			}

			args["server_port"] = argv[++i];

			if (!is_int(args["server_port"]))
			{
				std::cerr << "rshell: server port must be a valid number." << std::endl;
				return std::nullopt;
			}

			continue;
		}

		// if client
		if (arg == "-c" || arg == "--client")
		{
			if (!args["client_ip"].empty() || !args["client_port"].empty())
			{
				std::cerr << "rshell: client port or ip is alleardy specified with another argument." << std::endl;
				return std::nullopt;
			}

			if (!args["server_ip"].empty())
			{
				std::cerr << "rshell: cannot be a server and a client at the same time." << std::endl;
				return std::nullopt;
			}

			if (i == argc - 1 || argc < 4)
			{
				std::cerr << "rshell: " << arg << " parameter requires ip and port number." << std::endl;
				return std::nullopt;
			}

			args["client_ip"] = argv[++i];
			args["client_port"] = argv[++i];

			if (!is_int(args["client_port"]))
			{
				std::cerr << "rshell: client port must be a valid number." << std::endl;
				return std::nullopt;
			}

			continue;
		}

		// help
		if (arg == "-h" || arg == "--help")
		{
			print_help();
			exit(0);
		}

		// abort
		if (arg == "--halt")
		{
			exit(0);
		}
	}

	// if no args are provided return nullopt
	if (args.empty())
	{
		return std::nullopt;
	}

	return args;
}

// normal (offline) mode. not using server - client
void normal_mode()
{
	while (true)
	{
		print_prompt(); // print prompt

		std::string line; 
		std::getline(std::cin, line); // get line

		if (line == "quit")
		{
			exit(0);
		}

		if (line == "halt") 
		{
			exit(0);
		}

		if (line == "help")
		{
			print_help();
			continue;
		}

		line = remove_comments(line); // remove comments
		std::vector<std::string> cmds = split_string_on_delimiter(line, ";"); // separate line based on ; to multiple commands

		for (const auto &cmd : cmds)
		{
			std::string parsed_cmd = cmd;

			pipe_cmds(split_string_on_delimiter(parsed_cmd, "|")); // separate each command based on |
		}
	}
}

void remote_mode(Client client)
{
	// client requests a promt from server
	std::string prompt = client.read_server_prompt();

	while (true)
	{
		std::cout << prompt; // print prompt

		std::string line;
		std::getline(std::cin, line); // get line

		if (line == "")
		{
			continue;
		}

		if (line == "quit") 
		{
			client.disconnect();
			std::cout << "rshell: Disconnected from remote host, entering normal interactive mode." << std::endl;
			normal_mode();
		}

		if (line == "halt") 
		{
			exit(0);
		}

		if (line == "help")
		{
			print_help();
			continue;
		}

		// remove comments
		line = remove_comments(line);
		client.send_cmd(line); // send line to server
		
		std::string response = client.read_response(); // read response from server

		if (response != "")
		{
			std::cout << response; // print response
		}
	}
}


int main(int argc, char **argv)
{
	// get current pid and parrent pid for printing
	pid_t pid = getpid();
	pid_t ppid = getppid();

	std::cout << "My PID: " << pid << " Parrent PID: " << ppid << std::endl;

	// get argumemts
	auto args =  parse_args(argc, argv).value_or(std::map<std::string, std::string>());

	// if argumets are empty go to normal (offline) mode
	if (args.empty() && argc == 1)
	{
		normal_mode();
	}

	// if server port was specified
	if (!args["server_port"].empty())
	{
		signal(SIGINT, int_handler_server); // specify signal handeling
		Server server{str_to_int(args["server_port"])}; // create server
		server.init(1); // init server
		server.wait_for_connection(); // wat for connection

	    while(true)
	    {
	    	std::pair<std::string, int> read_pair = server.read_cmd(); // read command from client
	    	std::string buffer_str = read_pair.first;
	    	int bytesread = read_pair.second;

	    	if (bytesread > 0) // if something was read
	    	{
	    		if (buffer_str == "ByeBye") // if ByeBye string was send from client
	    		{
	    			std::cout << "rshell: server: Got disconnect from: " << server.get_client_ip() << std::endl;
	    		}

	    		if (buffer_str == "prompt") // if prompt was send from client
	    		{
	    			server.send_prompt(get_time() + " " + get_username() + "@" + get_hostname() + " #> ");
	    			continue;
	    		}

		    	std::vector<std::string> cmds = split_string_on_delimiter(buffer_str, ";"); // split string to more commands

				for (const auto &cmd : cmds)
				{
					std::string parsed_cmd = cmd;

					pipe_cmds(split_string_on_delimiter(parsed_cmd, "|"), fileno(stdin), server.get_client_socket()); // execute
				}

				server.send_end(); // send client end to stop reading
			}
	    }
	}
	else if (!args["client_ip"].empty() && !args["client_port"].empty())
	{
		signal(SIGINT, int_handler_client); // signal interupt for client
		Client client{args["client_ip"], str_to_int(args["client_port"])};
		client.init(); // initialize server
		client_for_handler = &client;
		remote_mode(client);
	}
	else
	{
		std::cerr << "rshell: invalid argumet encoutered." << std::endl;
	}

	return 0;
}