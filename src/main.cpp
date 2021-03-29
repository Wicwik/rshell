#include <iostream>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
	pid_t pid = getpid();
	pid_t ppid = getppid();

	std::cout << "My PID: " << pid << " Parrent PID: " << ppid << std::endl;

	std::string line;
	int status;
	while (1)
	{
		std::cout << "# ";
		std::cin >> line;

		if (line == "exit") exit(0);

		pid_t fpid;
		switch ((fpid = fork()))
		{
			case -1:
				std::cerr << "Error while forking this process with PID: " << pid << std::endl;
				exit(1);
			
			case 0:
				if (execlp(line.c_str(), line.c_str(), NULL) == -1) std::cerr << "rshell: " << line << ": command not found" << std::endl;
				break;

			default:
				wait(&status);
		}
	}

	return 0;
}