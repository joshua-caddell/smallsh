#include<unistd.h> // for exec(), write(), dup2(), chdir(), fork()
#include<stdlib.h> // for exit()
#include<sys/wait.h> //for waitpid()
#include<string.h> // stringcmp(), strtok()
#include<sys/types.h> // wait()
#include<stdio.h> //getline()
#include<fcntl.h> //write(), read()
#include<signal.h>

int removeIndex(int arr[], int i, int size);
void sigact(int sig);
int splitArguments(char original[], char *split[]);
void executeBuiltIn(char *args[], int pids[],int size, char *cmd, int status, int flag);
int main()
{
	int i, j, 
	    numArgs,//size of arg[]
	    pidSize = 0, //size of pidArr[]
	    status, bg_status, //passed to waitpid() by reference
	    sigflag = -1, //used as a boolean value to report how a process was terminated 
	    pidArr[50]; // will hold PIDs for back ground processes
	pid_t pid;
	char command[2049];//will hold 2048 characters plus the terminating null char
	//char cwd[1024];
	char *arg[513]; //will hold 512 arguments + NULL
	char *builtin[] = {"exit", "cd", "status"};//names of the builtin commands
	struct sigaction sigact; //struct for signal handling
	
	/* set the sa_handler to ignore then pass to
	 * sigaction() to ignore SIGINT signal */
	sigact.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigact, NULL);

	while(1)//seemingly infinite loop
	{	
		/* iterate through pidArr to check for terminated
		 * background processes */
		for(i = 0; i < pidSize; i++)
		{	
			/* waitpid checks if process has terminated 
			* WNOHANG prevents the program from waiting
			* for the process to terminate*/
			pid = waitpid(pidArr[i], &bg_status, WNOHANG);
			
			if( pid > 0)			
			{
				/* print the pid that finished and its terminating value 
				 * or signal. macros are used to interpret the value of
				 * of the status */
				if(WIFEXITED(bg_status))//process terminated normally
				{
					printf("background PID %d is done. exit value %d\n", pid, WEXITSTATUS(bg_status));
					fflush(stdout);
				}
				else//process killed by signal
				{
					printf("background pid %d is done. terminated by signal %d\n", pid, WTERMSIG(bg_status));
					fflush(stdout);
				}
				
				/* remove the PID from the array */
				pidSize = removeIndex(pidArr, i, pidSize);
				break;
			}
		}
		
		write(1, ":", 1);
		fflush(stdout);
		
		/*clear out command and read in 2048 bytes */
		memset(command, 0, sizeof(command));
		fgets(command, 2048, stdin);	
		
		/* blank lines and comment lines are ignored */
		if((strcmp(command, "\n") != 0) && (command[0] != '#'))
		{
			/* call funciton to put user input into an array */
			numArgs = splitArguments(command, arg);

			/*compare first argument ot the built in commands */
			for(i = 0; i < 3; i++)
			{
				if(strcmp(arg[0], builtin[i]) == 0)
					break;
			}
			
			/* call function to execute built in commands */
			if(i < 3)
				executeBuiltIn(arg, pidArr, pidSize, builtin[i], status, sigflag);
			else
			{
				char *redirect = NULL, 
				     *file = NULL;

				/* this if block will execute background commands */
				if(strcmp(arg[numArgs], "&") == 0)
				{
					pid_t f;
					int fd[2], p, w_file, r_file; // file descriptors
					
					/* execvp() requires a null temrinated array */
					arg[numArgs] = NULL;
					numArgs--;

					/* search the arguments for a redirection operator */
					for(i = 0; i <= numArgs; i++)
					{
						if((strcmp(arg[i], ">") == 0) || (strcmp(arg[i], "<") == 0))
						{	
							/* proper syntax requires a file name to immediately
							 * follow the redirection operator */
							redirect = arg[i]; 
							file = arg[i+1]; 
							for(j = i; j <= numArgs; j++)
								arg[j] = NULL;
							break;
						}
					}

					/* pass the array of file descriptors to pipe */
					p = pipe(fd);	
			
					if(p == -1)//pipe failure
					{
						exit(1);
					}

					f = fork();

					if(f == -1)//for failure
					{
						exit(1);
					}
					else if(f == 0)//child process
					{
						int dev; //file descriptor
						
						if(redirect != NULL)//user called for file redirection
						{	
							if(strcmp(redirect, ">") == 0)
							{
								/* open a file for writing only */
								w_file = open(file, O_WRONLY | O_CREAT, 0644);
								if(w_file < 0)//open failure
								{
									exit(1);
								}
					
								dev = open("/dev/null", O_RDONLY);
								if(dev < 0)
									exit(1);
								/* redirect input to dev/null */
								dup2(dev, 0);
								close(dev); 
								/*redirect output to user file */
								dup2(w_file, 1);
								close(w_file);
							}
							else if(strcmp(redirect, "<") == 0)
							{
								// open the input file indicated in the cmd line 
								r_file = open(file, O_RDONLY);
								if(r_file < 0)
								{
									exit(1);
								}
								// open dev/null fro output redirection 
								dev = open("/dev/null", O_WRONLY);
								if(dev < 0)
									exit(1);
								//redirect i/o and close files *
								dup2(dev, 1);		
								close(dev);
								dup2(r_file, 0);
								close(r_file);
							}
						}
						else//no file redirection in cmd line
						{	
							/* open dev/null for read/write */
							dev = open("/dev/null", O_RDWR, 0644);
							if(dev < 0)
								exit(1);

							// redirect i/o to dev null and close file 
							dup2(dev, 0);
							dup2(dev, 1);
							close(dev);
						}
						/* pass commands to execvp */
						execvp(arg[0], arg);
						exit(1);//only executes if execp() fails
					}
					else //parent
					{
						close(fd[1]);
						/* don't wait for the child to complete
						 * add bg pid to array
						 * print bg pid */
						pidArr[pidSize] = waitpid(f, &bg_status, WNOHANG);	
						printf("background pid is %d\n", f);
						fflush(stdout);
						pidSize++; 
					}
				}
				else//foreground process
				{
					pid_t f, w_status;
					int fd[2], p, w_file, r_file;
					
					/* check for user defined file redirection */	
					for(i = 0; i <= numArgs; i++)
					{
						if((strcmp(arg[i], ">") == 0) || (strcmp(arg[i], "<") == 0))
						{	
							redirect = arg[i];
							file = arg[i+1];
							for(j = i; j <= numArgs; j++)
								arg[j] = NULL;
							break;
						}
					}

					p = pipe(fd);	

					if(p == -1)//pipe failure
					{
						write(2, "pipe() error\n", 13);
						fflush(stdout);
					}

					f = fork();

					if(f == -1)//for failure
					{
						write(2, "fork() error\n", 13);
						fflush(stdout);
					}
					else if(f == 0)//child process
					{
						/* change handling of sigint signal
						 * back to default so that a ^c can 
						 * kill fg child process */
						sigact.sa_handler = SIG_DFL;
						sigaction(SIGINT, &sigact, NULL);
					
						if(redirect != NULL)
						{	
							if(strcmp(redirect, ">") == 0)
							{
								w_file = open(file, O_WRONLY | O_CREAT, 0644);
								if(w_file < 0)
								{
									write(1, "error opening file\n", 19);
									fflush(stdout);
									exit(1);
								}
								dup2(w_file, 1);// redirection stdout to file
								close(w_file);
							}
							else if(strcmp(redirect, "<") == 0)
							{
								r_file = open(file, O_RDONLY);
								if(r_file < 0)
								{
									write(1, "error opening file\n", 19);
									fflush(stdout);
									exit(1);
								}
								    	
								dup2(r_file, 0);//redirect stdin to file
								close(r_file);
							}
						}
						else
						{	
							/* redirect output to pipe */
							dup2(fd[1], 1);
							close(fd[1]);
						}
						
						/* pass arguments to exec */
						execvp(arg[0], arg);
						/*only executes on exec failure */
						write(1, "exec() error\n", 13);
						fflush(stdout);
						exit(1);
					}
					else //parent
					{
						close(fd[1]);
						/* wait for child to exit */
						w_status = waitpid(f, &status, 0);
					
						if(WIFEXITED(status))//process terminated normally
						{
							sigflag = 0;//signal flag is false
							char ch_out[1024];
							/* use while loop to read from the pipe */
							while(read(fd[0], ch_out, sizeof(ch_out)) != 0){}
							/*write the contents of the pipe */
							write(1, ch_out, sizeof(ch_out));
							fflush(stdout);
							memset(ch_out, 0, sizeof(ch_out));
						}
						else if(WIFSIGNALED(status))//child terminated by signal 
						{
							sigflag = 1;
							printf("terminated by signal %d\n", WTERMSIG(status));
							fflush(stdout);
						}
						else
						{
							write(1, "error exiting child\n", 20);
							fflush(stdout);
						}	
					}
				}
			}	
		}
	}
}

/*
 * function to execute the built in commands
 * paramaters: array of arguments, array of bg PIDs, size PID array, string containing
 * 	builtin command, int for status, int for signal bool
 */
void executeBuiltIn(char *args[], int pids[],int size, char *cmd, int status, int flag)
{
	int i;
	char *dir;

	if(strcmp(cmd, "exit") == 0)
	{
		/* iterate through running child processes
		 * and kill() them */
		for(i = 0; i < size; i++)
			kill(pids[i], SIGKILL);
		exit(0);
	}
	else if(strcmp(cmd, "cd") == 0)
	{
			
		if(args[1] != NULL)//user indicated a directory
		{
			/* cd ~ changes to home directory */
			if(strcmp(args[1], "~") == 0)
			{
				dir = getenv("HOME");
				i = chdir(dir);
			}
			else//go to directory user entered
				i = chdir(args[1]);
			
			if(i < 0)
			{
				write(2, "cd unsuccessful\n", 16);
				fflush(stdout);
			}
		}
		else//user just typed 'cd'
		{
			/* change to home directory */
			dir = getenv("HOME");
			i = chdir(dir);
			if(i < 0)
			{
				write(2, "cd unsuccessful\n", 16); 
				fflush(stdout);
			}
			
		}
	}
	else//status command
	{	
		if(flag == -1)//user has not yet run any child processes
		{
			printf("no children to report\n");
			fflush(stdout);
		}
		else if(flag == 0)//last child to terminated did so normally
		{
			printf("exit value %d\n", WEXITSTATUS(status));
			fflush(stdout);
		}
		else//last child terminated did so by signal
		{
			printf("terminated by signal %d\n", WTERMSIG(status));
			fflush(stdout);
		}

	}
}

/* function to parse the command line into an array */
int splitArguments(char original[], char *split[])
{
	int i = 0;
	char *delim = " \n";
	
	split[i] = strtok(original, delim);
	while(split[i] != NULL)
	{
		i++;
		split[i] = strtok(NULL, delim);
	}
	/* returns the number of arguments, ignoring 
	 * the last element which is null */
	return i - 1;
}

/* function to remove an element from an array */
int removeIndex(int arr[], int i, int size)
{
	int j;

	for(j = i; j < (size - 1); j++)
		arr[j] = arr[j+1];

	return size - 1; //return new size
}
