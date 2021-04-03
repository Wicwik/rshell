#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
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


int main(int argc, char **argv)
{
	pid_t pid = getpid();
	pid_t ppid = getppid();

	std::cout << "My PID: " << pid << " Parrent PID: " << ppid << std::endl;

	std::string line;
	int status;
	while (1)
	{
		std::cout << "#> ";
		std::getline(std::cin, line);

		if (line == "exit") exit(0);

		line = remove_comments(line);

		std::vector<std::string> strings = split_string_on_delimiter(line, " ");
		
		std::vector<char*> cstrings;
		cstrings.reserve(cstrings.size());

		for (const auto &s : strings)
		{
			if (s != "") cstrings.push_back(const_cast<char*>(s.c_str()));
		}

		char **sargv = &cstrings[0];

		pid_t fpid;
		switch ((fpid = fork()))
		{
			case -1:
				std::cerr << "Error while forking this process with PID: " << pid << std::endl;
				exit(1);
			
			case 0:
				if (execvp(sargv[0], sargv) == -1) std::cerr << "rshell: " << line << ": command not found" << std::endl;
				break;

			default:
				wait(&status);
		}
	}

	return 0;
}