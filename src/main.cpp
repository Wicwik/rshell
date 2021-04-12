#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cctype>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>


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


int main(int argc, char **argv)
{
	pid_t pid = getpid();
	pid_t ppid = getppid();

	std::cout << "My PID: " << pid << " Parrent PID: " << ppid << std::endl;
	int status;

	while (1)
	{
		std::cout << "#> ";

		std::string line;
		std::getline(std::cin, line);
		//std::cout << line << std::endl;

		if (line == "exit") exit(0);

		std::vector<std::string> cmds = split_string_on_delimiter(line, ";");
		for (const auto &cmd : cmds)
		{
			std::string parsed_cmd = cmd;

			bool redout = is_redirect_output(parsed_cmd);
			bool redin = is_redirect_input(parsed_cmd);

			int out;
			int save_out;

			int in;
			int save_in;

			if (redout)
			{
				std::pair<std::string, std::string> cmd_file_pair = separate(parsed_cmd, ">");
				parsed_cmd = cmd_file_pair.first;
				//std::cout << parsed_cmd << std::endl;

				std::string file_str = cmd_file_pair.second;
				file_str.erase(std::remove_if(file_str.begin(), file_str.end(), [](unsigned char x){return std::isspace(x);}), file_str.end());

				out = open(const_cast<char*>(file_str.c_str()), O_WRONLY|O_CREAT, 0666);
				if (-1 == out) std::cerr << "rshell: error while opening file output" << std::endl;

				save_out = dup(fileno(stdout));

				if (-1 == dup2(out, fileno(stdout))) std::cerr << "rshell: error while redirecting output" << std::endl;
			}

			if (redin)
			{
				std::pair<std::string, std::string> cmd_file_pair = separate(parsed_cmd, "<");
				parsed_cmd = cmd_file_pair.first;

				std::string file_str = cmd_file_pair.second;
				file_str.erase(std::remove_if(file_str.begin(), file_str.end(), [](unsigned char x){return std::isspace(x);}), file_str.end());

				in = open(const_cast<char*>(file_str.c_str()), O_RDONLY);
				if (-1 == in) std::cerr << "rshell: error while opening file input" << std::endl;

				save_in = dup(fileno(stdin));

				if (-1 == dup2(in, fileno(stdin))) std::cerr << "rshell: error while redirecting input" << std::endl;
			}

			char **sargv = nullptr;
			std::vector<char*> cstrings;
			if (parsed_cmd != "")
			{
				parsed_cmd = remove_comments(parsed_cmd);

				std::vector<std::string> strings = split_string_on_delimiter(parsed_cmd, " ");
				cstrings.reserve(cstrings.size());

				for (auto &s : strings)
				{
					if (s != "") cstrings.push_back(const_cast<char*>(s.c_str()));
					// std::cout << cstrings.back() << std::endl;
				}
				cstrings.push_back(nullptr);
				sargv = &cstrings[0];
			}
		
			pid_t fpid;
			switch ((fpid = fork()))
			{
				case -1:
					std::cerr << "Error while forking this process with PID: " << pid << std::endl;
					exit(1);
				
				case 0:
					if (-1 == execvp(sargv[0], sargv)) std::cerr << "rshell: " << parsed_cmd << ": command not found" << std::endl;
					break;

				default:
					wait(&status);

					if (redout)
					{
						fflush(stdout); 
						close(out);

						dup2(save_out, fileno(stdout));
					}

					if (redin)
					{
						fflush(stdin);
						close(in);

						dup2(save_in, fileno(stdin));
					}
			}
		}
	}

	return 0;
}