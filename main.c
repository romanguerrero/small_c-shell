/*			*			*			*			*				*
* file: main.c
* auth: Roman Guerrero
* desc: Small shell written in C
* 1. Provide a prompt for running commands
* 2. Handle blank lines & comments
* 3. Execute 3 built-into the shell commands: exit, cd, & status
* 4. Execute other commands by creating new processes
* 5. Support input and output redirection
* 6. Support running commands in foreground & background processes
* 7. Implement custom hanlders for 2 signals, SIGINT SIGTSTP
*			*			*			*			*				*/


// ------------------ Headers ------------------ //


#include <stdio.h> // perror, printf
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  // open
#include <unistd.h> // fork, close, execv, getpid
#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid
#include <signal.h>  // Signal handlers


// ------------------ Helper Struct ------------------ //


/// struct shell_info
/// description
/// - Stores current information about the shell
struct shell_info {
	int  background;
	int  exit_status;
	int  output_redirect;
	int  input_redirect;
	char output_filename[256];
	char input_filename[256];
};

/// function - init_shell_info
/// description
/// - Sets the values of shell_info to 0 or empty string
void init_shell_info(struct shell_info *info) {
	info->background = 0;
	info->input_redirect = 0;
	info->output_redirect = 0;
	memset(info->input_filename, 0, sizeof(info->input_filename));
	memset(info->output_filename, 0, sizeof(info->output_filename));
}

// ------------------ Function Prototypes ------------------ //
void other_cmd(char* args[], struct shell_info *info);
void custom_SIGINT();
void custom_IG();

int stop_background;  // Global variable declaration for Custom Sig Handlers

// ------------------ Helper Functions ------------------ //

/// function - print_args(char* args[])
/// description
/// - Prints arguments
void print_args(char* args[]) {
	int i = 0;
	while (args[i]) {
		printf("%s \n", args[i]);
		i++;
	}
}

/// function - free_memory()
/// description
/// - Frees dynamically allocated memory
void free_memory(char* line, char* args[])
{
	free(line);
	line = NULL;

	int i = 0;
	while (args[i]) {
		free(args[i]);   // Free each call to malloc
		args[i] = NULL;  // Point to NULL
		i++;
	}
}


// ------------------ Built-in functions ------------------ //

/// function - my_exit()
/// description
/// Built in exit command
int my_exit()
{
	// kill children

	printf("exiting shell \n");
	fflush(stdout);

	return 0;  // terminates smallsh by ending do-while loop
}


/// function - my_cd(char ** args)
/// description
/// Built in change directory command
/// Takes 0 or 1 arguments
/// Sets CWD to env var HOME if 0 args
/// Sets CWD to argument 1 otherwise
void my_cd(char* args[])
{
	if (!args[1]) {  // No arguments, set directory to HOME
		if (chdir(getenv("HOME")) != 0)
			printf("chdir() failed");  // Display in event of error
	}
	else {
		if (chdir(args[1]) != 0)
			printf("chdir() failed");
	}
}


/// function - my_status(int exit_status)
/// description
/// - Prints out either the exit status or the terminating signal of the last
///   foreground process ran by the shell
/// - Decodes status by using termination MACROS
void my_status(int exit_status)
{
	if (WIFEXITED(exit_status)) {
		printf("exit value %d \n", WEXITSTATUS(exit_status));
		fflush(stdout);
	}
	else {
		printf("terminated by signal %d \n", WTERMSIG(exit_status));
		fflush(stdout);
	}
}


// ------------------ I/O Redirection Functions ------------------ //


/// function - output_redirection
/// description
/// - Opens output file
/// - Use dup2 to direct output to file
/// - Close file
/// references
/// - Handling input/output redirection - https://stackoverflow.com/a/11518304/10895933
void output_redirection(char* filename)
{
	// Output file redirected via stdout should be opened for:
	//   Writing only
	//   It should be truncated (empty but not delete) if already exists
	//   OR Created if it does not exist
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);

	if (fd == -1) {  // Error checking
		perror("Output file could not be opened \n");  // Print error message
		exit(1);  // Set the exit status to 1
	}

	int d = dup2(fd, 1);  // Direct output to file descriptor

	if (d == -1) {
		perror("Output file could not be redirected");
		exit(1);
	}

	close(fd);  // Close file
}


/// function - input_redirection
/// description
/// - Opens input file
/// - Use dup2 to direct input to file
/// - Close file
/// references
/// - Handling input/output redirection - https://stackoverflow.com/a/11518304/10895933
void input_redirection(char* filename)
{
	// Input file redirected via stdin should be opened for reading only
	int fd = open(filename, O_RDONLY);

	if (fd == -1) {  // Error checking
		perror("Input file could not be opened \n");  // Print error message
		exit(1);  // Set the exit status to 1
	}

	int d = dup2(fd, 0);  // Direct output to file descriptor

	if (d == -1) {
		perror("Input file could not be redirected");
		exit(0);
	}

	close(fd);  // Close file
}


// ------------------ User Input Functions ------------------ //

/// function - get_input()
/// description
/// - Gets input from user
/// - Allocates memory
/// - Returns pointer to user input
/// references
/// - How to use fgets - http://sekrit.de/webdocs/c/beginners-guide-away-from-scanf.html
/// - remove newline from fgets - https://stackoverflow.com/a/2693826/10895933
char* get_input()
{
	char buf[2048];
	char* line = NULL;

	printf(": ");  // Prompt user
	fflush(stdout);  // Flush per requirements

	fgets(buf, 2048, stdin);  // Get input from user

	strtok(buf, "\n");  // Remove newline from fgets

	line = malloc(sizeof(char) * sizeof(buf));  // Allocate memory to line

	strcpy(line, buf);  // Store input in line

	return line;  // return user input
}

/// function parse_line(char* line)
/// description
/// - Splits line param into tokens delimited by whitespace " "
/// - Allocates memory for each token
/// - Stores tokens into args
void parse_line(char* line, struct shell_info *info, char* args[])
{
	char* saveptr;
	char* token;
	int i = 0;

	token = strtok_r(line, " ", &saveptr);  // Get first arg into token
	args[i] = malloc(strlen(token));  // Allocate memory for command
	strcpy(args[i], token);  // Copy into args
	i++;  // Argument counter

	while ((token = strtok_r(NULL, " ", &saveptr))) {  // Get rest of arguments

		if (strcmp(token, "<") == 0) {  // Identify any input file
			info->input_redirect = 1;  // Set input file flag
			token = strtok_r(NULL, " ", &saveptr);  // Get filename
			strcpy(info->input_filename, token);  // Save filename
		}

		else if (strcmp(token, ">") == 0) {  // Repeat for potential outfile
			info->output_redirect = 1;
			token = strtok_r(NULL, " ", &saveptr);
			strcpy(info->output_filename, token);
		}

		else if (strcmp(token, "&") == 0) {  // Identify background flag
			info->background = 1;
		}

		else if (strcmp(token, "$$") == 0) {  // Changes $$ to pid
			int pid = getpid();
			args[i] = malloc(6);
			sprintf(args[i], "%d", pid);
			i++;
		}

		else {
			args[i] = malloc(strlen(token));  // Bloc saves arguments for rest of line
			strcpy(args[i], token);
			i++;
		}
	}
}


// ------------------ Execute Commands Functions ------------------ //

/// function - execute_cmd(char* args[])
/// description
/// - Executes command stored in arguments
/// - Follows arguments appropriately
int execute_cmd(char* args[], struct shell_info *info)
{
	int cmd;
	int status = 1;  // Return this to indicate if shell should continue

	// Translate command to variable for switch statement

	if (strcmp(args[0], "exit") == 0) {  // Exit
		status = my_exit();
	}
	else if (strcmp(args[0], "cd") == 0) {  // Change Directory
		my_cd(args);
	}
	else if (strcmp(args[0], "status") == 0) {  // Status
		my_status(info->exit_status);
	}
	else if (strcmp(args[0], "\n") == 0 || args[0][0] == '#') {  // Blank line or comment
		// Do nothing
	}
	else {
		other_cmd(args, info);  // Execute non-built in commands
	}

	// Wait for terminated children / clean up zombies
	int corpse;
	while ((corpse = waitpid(-1, &info->exit_status, WNOHANG)) > 0) {
		printf("background pid %d is done: ", corpse);
		fflush(stdout);
		my_status(info->exit_status);  // Print how child terminated
	}


	return status;
}


/// function - other_cmd
void other_cmd (char* args[], struct shell_info *info)
{
	pid_t spawnPid = fork();  // Fork a new process

	struct sigaction SIG_H = { 0 };

	switch (spawnPid) {
	case -1:
		perror("fork() \n");
		exit(1);
		break;

	case 0:  // In child process

		custom_IG();  // Children ignore SIGTSTP


		if (info->background && !stop_background) {
			printf("background pid is %d \n", getpid());  // Display background pid
			fflush(stdout);

			// Background cmd should use /dev/null for if input | output if respective redirection not specified
			if (!info->output_redirect)
				output_redirection("/dev/null");

			if (!info->input_redirect)
				input_redirection("/dev/null");
		}
		else {
			SIG_H.sa_handler = SIG_DFL;  // Restore ^C default functionality in child
			sigfillset(&SIG_H.sa_mask);  // Only in foreground
			SIG_H.sa_flags = SA_RESETHAND;
			sigaction(SIGINT, &SIG_H, NULL);
		}

		// ------------------ I/O Redirection ------------------ //

		if (info->input_redirect) {
			input_redirection(info->input_filename);  // Input redirection if applicable
		}

		if (info->output_redirect) {
			output_redirection(info->output_filename);  // Output redirection if applicable
		}

		// ------------------ Execute Command ------------------ //

		execvp(args[0], args);  // Replace the current program with command (aka execute command)
		perror("execvp");  // this only returns if there is an exec error
		exit(2);
		break;

	default:  // In parent process

		if (info->background && !stop_background) {  // Run in background
			spawnPid = waitpid(spawnPid, &info->exit_status, WNOHANG);

		}
		else {  // Run in foreground

			spawnPid = waitpid(spawnPid, &info->exit_status, 0);  // Wait for child's termination

			if (info->exit_status != 0) {  // Print out abnormal exit if applicable
				my_status(info->exit_status);
			}

		}
		break;
	}
}


// ------------------ Signal Functions ------------------ //



/// function - handle_SIG()
/// description
/// - Custom handler for SIGINT / ^C
/// - Children running as a foreground process terminates itself
/// reference
/// - How can a process kill itself - https://stackoverflow.com/a/7851269/10895933
void handle_SIG(int signo)
{
	// stop_background is a global variable

	if (stop_background == 0) {

		// Print msg - No more background
		char* msg = "Entering foreground-only mode (& is now ignored) \n";
		write(STDOUT_FILENO, msg, 50);
		fflush(stdout);

		stop_background = 1;  // set stop_background flag on
	}
	else {
		// Print msg - background is back on
		char* msg = "Exiting foreground-only mode \n";  // print output testing
		write(STDOUT_FILENO, msg, 30);
		fflush(stdout);

		stop_background = 0;  // Set stop_background flag off
	}
}

///
/// description
/// - Custome Signal Handlers for ^C && ^Z
void custom_SIG()
{
	struct sigaction SIG_IG = { 0 };  // Ignores ^C
	SIG_IG.sa_handler = SIG_IGN;	  // This is reinstated for children
	sigfillset(&SIG_IG.sa_mask);
	SIG_IG.sa_flags = SA_RESTART;
	sigaction(SIGINT, &SIG_IG, NULL);
}

void custom_SIGTSTP()
{
	struct sigaction SIGINT_action = { 0 };  // Init struct to be empty
	SIGINT_action.sa_handler = handle_SIG;  // Register as the signal handler
	sigfillset(&SIGINT_action.sa_mask);  // Block all catchable signals while running
	SIGINT_action.sa_flags = SA_RESTART;  // Clear error caused by signal
	sigaction(SIGTSTP, &SIGINT_action, NULL);  // Install our signal handler
}

void custom_IG()
{
	struct sigaction SIG_IG = { 0 };
	SIG_IG.sa_handler = SIG_IGN;
	sigfillset(&SIG_IG.sa_mask);
	SIG_IG.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIG_IG, NULL);
}

// ------------------ Shell Main Function ------------------ //

/// function - small_shell()
/// description
/// - Controls the flow of the small shell
/// - Gets user input as string
/// - Parses string into command
/// - Executes command
/// - Loops until user gives exit command
void small_shell()
{
	struct shell_info info;
	int status;
	char* args[512];

	for (int i = 0; i < 512; i++) {  // Init args to null
		args[i] = NULL;
	}

	custom_SIG();
	custom_SIGTSTP();

	printf("small c-shell, enter shell commands like ls or echo \n");  // Displays title of program
	fflush(stdout);

	do {
		init_shell_info(&info);  // Initialize shell info to 0

		char* line = get_input();  // Gets user string input

		parse_line(line, &info, args);  // Parses input into arguments

		status = execute_cmd(args, &info);  // Executes passed in command

		free_memory(line, args);

	} while (status);
}


// ------------------ Main Function ------------------ //

/// Main function
int main()
{
	small_shell();

	return 0;
}
