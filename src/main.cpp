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

#include "lib/server.hh"
#include "lib/client.hh"

std::string get_time()
{
	time_t rt;
	struct tm * t;

	time(&rt);
	t = localtime(&rt);

	return std::string(std::to_string(t->tm_hour) + ":" + std::to_string(t->tm_min));
}

std::string get_hostname()
{
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);

	return std::string(hostname);
}

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

void print_prompt()
{
	std::cout << get_time() << " " << get_username() << "@" << get_hostname() << " #> ";
}

bool is_int(std::string& str)
{
	try
	{
		size_t pos = 0;
		std::stoi(str, std::addressof(pos));

		while (std::isspace(str.back())) 
		{
			str.pop_back();
		}
		
		return pos == str.size();
	}

	catch (const std::exception&) 
	{ 
		return false; 
	}
}

int str_to_int(std::string str)
{
	std::stringstream ss(str);
	int out = 0;
	ss >> out;

	return out;
}

std::vector<std::string> split_string_on_delimiter(std::string s, std::string delimiter)
{
	std::vector<std::string> split_strings;
	size_t pos = 0;

	while ((pos = s.find(delimiter)) != std::string::npos)
	{
		split_strings.push_back(s.substr(0, pos));
		s.erase(0, pos + delimiter.length());
	}

	split_strings.push_back(s);

	return split_strings;
}

std::string remove_comments(std::string s)
{
	size_t pos = 0;
	if ((pos = s.find("#")) == std::string::npos) return s;

	return s.substr(0, pos);
}

bool is_redirect_output(std::string s)
{
	return s.find(">") != std::string::npos;
}

bool is_redirect_input(std::string s)
{
	return s.find("<") != std::string::npos;
}

std::pair<std::string, std::string> separate(std::string s, std::string delimiter)
{
	size_t pos = s.find(delimiter);
	return std::pair<std::string, std::string>{s.substr(0, pos), s.substr(pos+1)};
}

int spawn_process(int in, int out, std::string cmd)
{
	int status;

	bool redout = is_redirect_output(cmd);
	bool redin = is_redirect_input(cmd);

	if (redout)
	{
		std::pair<std::string, std::string> cmd_file_pair = separate(cmd, ">");
		cmd = cmd_file_pair.first;

		std::string file_str = cmd_file_pair.second;
		file_str.erase(std::remove_if(file_str.begin(), file_str.end(), [](unsigned char x){return std::isspace(x);}), file_str.end());

		out = open(const_cast<char*>(file_str.c_str()), O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (-1 == out) std::cerr << "rshell: error while opening file output" << std::endl;
	}

	if (redin)
	{
		std::pair<std::string, std::string> cmd_file_pair = separate(cmd, "<");
		cmd = cmd_file_pair.first;

		std::string file_str = cmd_file_pair.second;
		file_str.erase(std::remove_if(file_str.begin(), file_str.end(), [](unsigned char x){return std::isspace(x);}), file_str.end());

		in = open(const_cast<char*>(file_str.c_str()), O_RDONLY);
		if (-1 == in) std::cerr << "rshell: error while opening file input" << std::endl;
	}

	char **sargv = nullptr;
	std::vector<char*> cstrings;
	if (cmd != "")
	{
		std::vector<std::string> strings = split_string_on_delimiter(cmd, " ");
		cstrings.reserve(cstrings.size());

		for (auto &s : strings)
		{
			if (s != "") cstrings.push_back(const_cast<char*>(s.c_str()));
		}
		cstrings.push_back(nullptr);
		sargv = &cstrings[0];
	}

	pid_t fpid;
	switch ((fpid = fork()))
	{
		case -1:
			std::cerr << "rshell: error while forking this process with PID: " << fpid << std::endl;
			exit(1);
		
		case 0:
			if (in != 0)
			{
				if (-1 == dup2(in, fileno(stdin))) std::cerr << "rshell: error while redirecting input" << std::endl;

				fflush(stdin);
				close(in);
			}

			if (out != 1)
			{
				if (-1 == dup2(out, fileno(stdout))) std::cerr << "rshell: error while redirecting output" << std::endl;

				fflush(stdout);
				close(out);
			}

			if (-1 == execvp(sargv[0], sargv)) std::cerr << "rshell: " << cmd << ": command not found" << std::endl;
			break;

		default:
			wait(&status);

	}

	return status;
}

int pipe_cmds(std::vector<std::string> cmds, int in = fileno(stdin), int out = fileno(stdout))
{
	int fd[2];

	std::string last_cmd = cmds.back();
	if (cmds.size() > 1)
	{	
		cmds.pop_back();

		for (const auto &cmd : cmds)
		{
			pipe(fd);

			spawn_process(in, fd[1], cmd);

			close(fd[1]);

			in = fd[0];
		}
	}

	return spawn_process(in, out, last_cmd);
}

std::optional<std::map<std::string, std::string>> parse_args(int argc, char** argv)
{
	std::map<std::string, std::string> args;

	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];

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
	}

	if (args.empty())
	{
		return std::nullopt;
	}

	return args;
}

void normal_mode()
{
	while (true)
	{
		print_prompt();

		std::string line;
		std::getline(std::cin, line);

		if (line == "exit") 
		{
			exit(0);
		}

		line = remove_comments(line);
		std::vector<std::string> cmds = split_string_on_delimiter(line, ";");

		for (const auto &cmd : cmds)
		{
			std::string parsed_cmd = cmd;

			pipe_cmds(split_string_on_delimiter(parsed_cmd, "|"));
		}
	}
}

void remote_mode(Client client)
{
	std::string prompt = client.read_server_prompt();

	while (true)
	{
		
		std::cout << prompt;

		std::string line;
		std::getline(std::cin, line);

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

		line = remove_comments(line);
		client.send_cmd(line);
		
		std::string response = client.read_response();

		if (response != "")
		{
			std::cout << response;	
		}
	}
}


int main(int argc, char **argv)
{
	pid_t pid = getpid();
	pid_t ppid = getppid();

	std::cout << "My PID: " << pid << " Parrent PID: " << ppid << std::endl;

	auto args =  parse_args(argc, argv).value_or(std::map<std::string, std::string>());

	if (args.empty())
	{
		normal_mode();
	}

	if (!args["server_port"].empty())
	{
		Server server{str_to_int(args["server_port"])};
		server.init(1);
		server.wait_for_connection();

	    while(true)
	    {
	    	std::pair<std::string, int> read_pair = server.read_cmd();
	    	std::string buffer_str = read_pair.first;
	    	int bytesread = read_pair.second;

	    	if (bytesread > 0)
	    	{
	    		if (buffer_str == "ByeBye")
	    		{
	    			break;
	    		}

	    		if (buffer_str == "prompt")
	    		{
	    			server.send_prompt(get_time() + " " + get_username() + "@" + get_hostname() + " #> ");
	    			continue;
	    		}

		    	std::vector<std::string> cmds = split_string_on_delimiter(buffer_str, ";");

				for (const auto &cmd : cmds)
				{
					std::string parsed_cmd = cmd;

					pipe_cmds(split_string_on_delimiter(parsed_cmd, "|"), fileno(stdin), server.get_client_socket());
				}

				server.send_end();
			}
	    }
	}
	else if (!args["client_ip"].empty() && !args["client_port"].empty())
	{
		Client client{args["client_ip"], str_to_int(args["client_port"])};
		client.init();
		remote_mode(client);
	}
	else
	{
		std::cerr << "rshell: invalid argumet encoutered." << std::endl;
	}

	return 0;
}