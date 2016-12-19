#include "ef_utils.h"
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <string.h>
#include <getopt.h>

// TODO: - Add content of directories recursively.
// 
//       - Differentiate DIRECTORIES and files. (DIR, FILE)?
//         What to do with directories then?
//         Idea: Add a label system (%DIR% and %FILE% would be
//         reserved, %IMG%, %VIDEO%, ... could be associted with
//         extensions by a user).
//         
//       - Allow more powerful syntax.
//
//       - Add options:
//         --recursive: (See first point.)
//         --as LABEL: (See label sytem) open ALL files given with the command associated to the LABEL.
//         --only LABEL [LABEL ...]: Only open files associated to the LABEL(s).
//       
//       - Make sure only one command is associated with a given
//         extension.

#define ME "xopen"

#define MAJOR_VERSION 0
#define MINOR_VERSION 3

#define VERSION TO_STRING(JOIN3(MAJOR_VERSION, ., MINOR_VERSION))

enum OptionFlag
{
	OptionFlag_NONE		= 0,
	OptionFlag_WHICH	= 1 << 0,
};

struct Instruction
{
	char command[255];
	char *arguments[255];
	
	int commandLength;
	int argumentCount;
};

static char usage[] =
{
	"Usage: "
	ME
	" FILE [FILE ...] [OPTION ...]\n\n"
	"Execute a predefined command based on given files' extension.\n\n"
	"Syntax of config file is:\n"
	"CMD - EXTENSION [EXTENSION ...]\n\n"
	"(EXTENSION is dot-less: 'pdf' not '.pdf')\n\n"
	"If a line does not have any extension, it's the default command\n"
	"(used when no other line matches).\n"
	"In that case, '-' can be omitted.\n\n"
	"A command can be any executable in your PATH or any function in\n"
	"~/.bashrc.\n\n"
	"Example:\n"
	"evince - pdf\n"
	"mpv - mp4 mkv\n"
	"emacs\n\n"
	"This will execute 'evince' on '.pdf' files, mpv on '.mp4' and '.mkv'\n"
	"and emacs on everything else.\n\n"
	"Options:\n"
	"      --help        Show this (hopefully) helpful message.\n"
	"      --version     Show this program's version.\n"
	"  -w, --which       Show which command would be executed on each given file."
};

// NOTE: This part can be reused.
static char version[] =
{
	ME
	" "
	VERSION
	"\n\n"
	"Copyright © 2016 Barbu\n"
	"This work is free. You can redistribute it and/or modify it under the\n"
	"terms of the Do What The Fuck You Want To Public License, Version 2,\n"
	"as published by Sam Hocevar. See http://www.wtfpl.net/ for more details."
};


// Copy output of pipe_fd to buffer. Remove trailing newline if any.
static inline int copyOutput(int pipe_fd[2], char *buffer, int bufferSize)
{
	close(pipe_fd[1]);

	int bytesRead = read(pipe_fd[0], buffer, bufferSize - 1);

	close(pipe_fd[0]);

	if (*(buffer + bytesRead - 1) == '\n')
	{
		*(buffer + bytesRead - 1) = '\0';
	}

	return bytesRead;
}

// TODO: Flush pipes.
// NOTE: This part can be reused.
/* Exec command with args (commandPath is absolute, args[0] must be
   the command name).
   Store child's status code in statusCode if not in background.
   Store child's stdout in stdoutBuffer (if any).
   Store parent or child's stderr in stderrBuffer (if any).
 
   Return < 0 if error on parent's part.
          > 0 if error on child's part.
          = 0 otherwhise.
*/
static int childExec(char *commandPath, char *args[], int *statusCode = NULL,
					 char *stdoutBuffer = NULL, int stdoutBufferSize = 0,
					 char *stderrBuffer = NULL, int stderrBufferSize = 0,
					 b32 inBackground = false)
{
	// NOTE: Using macros for this is totally unnecessary.
	//       But it's fun!
#define PIPE_STREAM(stream, code) do									\
	{																	\
		if (JOIN(stream, Buffer))										\
		{																\
			if (pipe(JOIN(pipe_, stream)) == -1)						\
			{															\
				if (stderrBuffer)										\
				{														\
					char buffer[255];									\
					sprintf(buffer, "%s: unable to create %s pipe.\n", ME, TO_STRING(stream)); \
					strncpy(stderrBuffer, buffer, stderrBufferSize);	\
				}														\
				return code;											\
			}															\
		}																\
	} while(0)

#define DUP_STREAM(stream, alias) do				\
	{												\
		if (JOIN(stream, Buffer))					\
		{											\
			dup2(JOIN(pipe_, stream)[1], alias);	\
			close(JOIN(pipe_, stream)[0]);			\
			close(JOIN(pipe_, stream)[1]);			\
		}											\
	} while(0)

#define COPY_STREAM(stream) do											\
	{																	\
		if (JOIN(stream, Buffer))										\
		{																\
			copyOutput(JOIN(pipe_, stream), JOIN(stream, Buffer), JOIN(stream, BufferSize)); \
		}																\
	} while(0)
	
	int pipe_stdout[2],
		pipe_stderr[2];

	PIPE_STREAM(stderr, -1);
	PIPE_STREAM(stdout, -2);

	switch(fork())
	{
		case -1:
		{
			if (stderrBuffer)
			{
				char buffer[255];
				sprintf(buffer, "%s: unable to start child process.\n", ME);
				strncpy(stderrBuffer, buffer, stderrBufferSize);
			}

			return -3;
		}
		case 0:
		{
			DUP_STREAM(stderr, STDERR_FILENO);
			DUP_STREAM(stdout, STDOUT_FILENO);
			
			execv(commandPath, args);
			
			char buffer[255];
			sprintf(buffer, "%s: failed to execute %s.\n", ME, args[0]);
			perror(buffer);
			
			return 1;
		}
		default:
		{
			COPY_STREAM(stderr);
			COPY_STREAM(stdout);

			if (!inBackground)
			{
				wait(statusCode);
			}

			break;
		}
	}

#undef PIPE_STREAM
#undef DUP_STREAM
#undef COPY_STREAM
	
	return 0;
}

int main(int argc, char* argv[])
{
	// NOTE: This part can be reused.
	char configFile[255];
	char *homeDir = NULL;
	
	if ((homeDir = getenv("XDG_CONFIG_HOME")) != NULL)
	{
		sprintf(configFile, "%s/%s.conf", homeDir, ME);
	}
	else if (((homeDir = getenv("HOME")) != NULL) ||
			 ((homeDir = getpwuid(getuid())->pw_dir) != NULL))
	{
		sprintf(configFile, "%s/.config/%s.conf", homeDir, ME);
	}
	else
	{
		char buffer[255];
		sprintf(buffer, "%s: could not create config file: no home directory found.\n", ME);
		fprintf(stderr, buffer);

		return -1;
	}

	FILE *handle = fopen(configFile, "a+");

	if (handle == NULL)
	{
		char buffer[255];
		sprintf(buffer, "%s: could not read config file '%s'.\n", ME, configFile);
		fprintf(stderr, buffer);

		return -2;
	}
	
	fclose(handle);

	Instruction allInstructions[255] = {};
	int instructionCount = 0;
	
	char **allFiles;
	int fileCount = 0;

	int helpFlag = 0,
		versionFlag = 0;
	
	i32 optionFlags = OptionFlag_NONE;

	static struct option longOptions[] =
		{
			{"help", no_argument, &helpFlag, 1},
			{"version", no_argument, &versionFlag, 1},
			{"which", no_argument, 0, 'w'},
			{0, 0, 0, 0}
		};
			
	int c;
	
	for(;;)
	{
		int optionIndex = 0;
		c = getopt_long(argc, argv, "w", longOptions, &optionIndex);

		if (c == -1)
		{
			break;
		}
			
		switch (c)
		{
			case 0:
			{
				if (longOptions[optionIndex].flag != 0)
				{
					break;
				}
				// printf("option %s\n", longOptions[optionIndex].name);
				// return 0;
			}
			case 'w':
			{
				optionFlags |= OptionFlag_WHICH;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	if (helpFlag)
	{
		printf("%s\n", usage);
		return 0;
	}

	if (versionFlag)
	{
		printf("%s\n", version);
		return 0;
	}	

	fileCount = argc - optind;

	if (fileCount <= 0)
	{
		char buffer[255];
		sprintf(buffer, "%s: no file given.\n", ME);
		fprintf(stderr, buffer);

		return 1;
	}
	
	allFiles = argv + optind;

	for (int i = 0; i < fileCount; ++i)
	{
		char *file = allFiles[i];
		char *extension = file;
			
		while (*extension && (*extension++) != '.');

		// FIXME: Do not assume ouput is less than 255 chars.
		char line[255] = {};
		char errorBuffer[255] = {};
		int statusCode = 0;
		
		char *command = line;
		int commandLength = 0;

		// Search extension in config file.
		// If found, add file to associated command.
		if (*extension)
		{
			// NOTE: Extensions are separated by spaces or are at the
			//       end of a line.
			char regexp[255];
			sprintf(regexp, " %s( |$)", extension);
			
			char *grepArgs[] =
				{
					"grep",
					"--extended-regexp",
					regexp,
					configFile,
					NULL
				};
			
			int status = childExec("/bin/grep", grepArgs, &statusCode,
								   line, ARRAY_SIZE(line),
								   errorBuffer, ARRAY_SIZE(errorBuffer));

			if (status == 0)
			{
				if (statusCode == 0)
				{
					// NOTE: Syntax is CMD - EXTENSION [EXTENSION ...].
					char *commandEnd = strstr(command, " - ");

					if (commandEnd == NULL)
					{
						char buffer[255];
						sprintf(buffer, "%s: invalid configuration for extension '%s'.\n",
								ME, extension);
						fprintf(stderr, buffer);

						return -3;
					}

					// Remove trailing whitespaces.
					while (*commandEnd == ' ')
					{
						--commandEnd;
					}
							
					commandLength = commandEnd - line + 1;
				}
			}
			else if (status > 0)
			{
				fprintf(stderr, errorBuffer);
			}
		}

		// NOTE: Default command if no extension or no associated
		//       command found.
		if (!(*extension) ||
			!(*command))
		{
			char *grepArgs[] =
				{
					"grep",
					"--invert-match",
					" - ",
					configFile,
					NULL
				};
			
			int status = childExec("/bin/grep", grepArgs, &statusCode,
								   line, ARRAY_SIZE(line),
								   errorBuffer, ARRAY_SIZE(errorBuffer));
				
			if (status == 0)
			{
				if (statusCode == 0)
				{
					char *commandEnd = line + strlen(line) - 1;
						
					// Remove trailing whitespaces.
					while (*commandEnd == ' ')
					{
						--commandEnd;
					}
						
					commandLength = commandEnd - line + 1;
				}
			}
			else if (status > 0)
			{
				fprintf(stderr, errorBuffer);
			}
		}

		// TODO?: Keep separate error message or group by extension?
		if (!(*command))
		{
			char buffer[255];
			sprintf(buffer, "%s: %s: no command specified for extension '%s'.\n",
					ME, file, extension);
			fprintf(stderr, buffer);
		}
		else
		{
			b32 found = false;
					
			FOR_EACH(Instruction, instruction, allInstructions)
			{
				if (instruction->command &&
					(instruction->commandLength == commandLength) &&
					(strncmp(instruction->command, command, commandLength) == 0))
				{
					instruction->arguments[instruction->argumentCount++] = file;
					found = true;
					break;
				}
			}

			if (!found)
			{
				Instruction *instruction = allInstructions + instructionCount++;
						
				instruction->commandLength = commandLength;
				strncpy(instruction->command, command, commandLength);
				instruction->arguments[instruction->argumentCount++] = file;
			}
		}
	}

	for (int i = 0; i < instructionCount; ++i)
	{
		Instruction instruction = allInstructions[i];

		char *whichArgs[] =
			{
				"which",
				instruction.command,
				NULL
			};
		
		char commandPath[255] = {};
		int statusCode = 0;

		// TODO: Separate actual command name from its arguments and
		//       its path (if it's a shell function).
		int status = childExec("/usr/bin/which", whichArgs, &statusCode,
							   commandPath, ARRAY_SIZE(commandPath));

		if (status != 0)
		{		
			continue;
		}

		// NOTE: If which did not find the command, we assume it's a
		//       shell function defined in ~/.bashrc.
		char *path = (statusCode == 0) ? commandPath : (char *) "~/.bashrc";
		
		if (optionFlags & OptionFlag_WHICH)
		{
			printf("%s (%s)", instruction.command, path);
			PRINT_N_ARRAY("\n\t%s", "", instruction.arguments, instruction.argumentCount);
			printf("\n\n");
		}
		// It's a script.
		else if (statusCode == 0)
		{
			// + 2: command name + NULL. 
			char *commandArgs[ARRAY_SIZE(((Instruction *) 0)->arguments) + 2] = {};

			commandArgs[0] = instruction.command;
			memcpy(commandArgs + 1, instruction.arguments, instruction.argumentCount * sizeof(char *));

			childExec(path, commandArgs,
					  NULL, NULL, 0, NULL, 0, true);
		}
		// It's a function.
		else
		{
			// TODO: Actually compute max length!
			// NOTE: To be correctly interpreted by bash, it must be:
			//       ". SOURCE_FILE && CMD ARGS" as one string.
			char bashCommand[4096] = {". "};
			char *current = bashCommand + 2;

			// Why strcpy doesn't return where it stopped writing
			// instead of returning the original buffer is beyond
			// me...
			current = strcpy(current, path) + strlen(path);
			*current++ = ' '; *current++ = '&'; *current++ = '&'; *current++ = ' ';
			
			current = strcpy(current, instruction.command) + instruction.commandLength;
			*current++ = ' ';
			
			for (int index = 0; index < instruction.argumentCount; ++index)
			{
				char *argument = instruction.arguments[index];
				current = strcpy(current, argument) + strlen(argument);
				*current++ = ' ';
			}

			*(--current) = '\0';
			
			char *bashArgs[ARRAY_SIZE(((Instruction *) 0)->arguments) + 3] =
				{
					"bash",
					"-c",
					bashCommand,
					NULL,
				};
			
			childExec("/bin/bash", bashArgs,
					  NULL, NULL, 0, NULL, 0, false);
		}
	}

	return 0;
}
