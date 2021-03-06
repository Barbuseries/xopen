#include "ef_utils.h"
#include "config_file_parser.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <string.h>
#include <getopt.h>
#include <dirent.h>

// TODO: - Add options:
//         --as EXTENSION/TAG: (See tag sytem) open ALL files given with the command associated with the EXTENSION/TAG.
//         
//       - Allow more powerful syntax.
//
//       - Make sure only one command is associated with a given
//         extension.


#define MAJOR_VERSION 0
#define MINOR_VERSION 4

#define VERSION TO_STRING(JOIN3(MAJOR_VERSION, ., MINOR_VERSION))

enum OptionFlag
{
	OptionFlag_None							= 0,
	OptionFlag_Which						= 1 << 0,
	OptionFlag_Recursive					= 1 << 1,
	OptionFlag_Recursive_Keep_Directories	= 1 << 2,
	OptionFlag_Only							= 1 << 3,
};


static char usage[] =
{
	"Usage: "
	ME
	" FILE [FILE ...] [OPTION ...]\n\n"
	"Execute a predefined command based on given files' extension.\n\n"
	"Syntax of config file is:\n"
	"CMD - EXTENSION [EXTENSION ...] [@TAG]\n\n"
	"(EXTENSION is dot-less: 'pdf' not '.pdf')\n\n"
	"The extension for directories is '/'.\n\n"
	"If a line does not have any extension, it's the default command\n"
	"(used when no other line matches).\n"
	"In that case, '-' can be omitted.\n\n"
	"A command can be any executable in your PATH or any function in\n"
	"~/.bashrc.\n\n"
	"Example:\n"
	"evince - pdf\n"
	"mpv - mp4 mkv @VIDEO\n"
	"nautilus - /\n"
	"emacs\n\n"
	"This will execute 'evince' on '.pdf' files, mpv on '.mp4' and '.mkv',\n"
	"nautilus on directories and emacs on everything else.\n\n"
	"Options:\n"
	"      --help        Show this (hopefully) helpful message.\n"
	"      --version     Show this program's version.\n"
	"  -w, --which       Show which command would be executed on each given file.\n"
	"  -e, --execute     Execute each command with it's associated files.\n"
	"                    (Default)\n"
	"  -r, --recursive   Add sub-directories recursively.\n"
	"  -R, --recursive-keep-directories\n"
	"                    Add sub-directories recursively and add them as well.\n"
	"  -d, --directory   Add directories themselves not their content.\n"
	"                    (Default)\n"
	"  -o, --only EXTENSION/TAG\n"
	"                    Only execute commands associated with EXTENSION or TAG.\n"
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
	"as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.\n"
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

static void getFileExtension(char *file, char *extension)
{
	char *fileOffset = file + strlen(file) - 1;
	
	// Put offset on last dot (or slash, as it can no longer be an
	// extension).
	while ((*fileOffset != '.') &&
		   (*fileOffset != '/') && 
		   (--fileOffset - file) >= 0);

	// No extension found.
	if ((fileOffset - file) < 0 ||
		(*fileOffset == '/'))
	{
		extension[0] = '\0';
	}
	else
	{
		ASSERT(strlen(fileOffset + 1) < 64);
			
		strcpy(extension, fileOffset + 1);
	}
}

static void getExtension(char *entry, char *extension)
{
	struct stat entryStat;
	stat(entry, &entryStat);

	// Extension for directories is '/' (as it's both
	// meaningful and impossible to have).
	if (S_ISDIR(entryStat.st_mode))
	{
		extension[0] = '/'; extension[1] = '\0';
		return;
	}

	getFileExtension(entry, extension);
}

static b32 instructionHasExtension(Instruction *instruction, char *extension, size_t extensionLength)
{
	for (int extensionIndex = 0; extensionIndex < instruction->extensionCount; ++extensionIndex)
	{
		if ((instruction->extensionsLength[extensionIndex] == extensionLength) &&
			(strncmp(extension, instruction->extensions[extensionIndex], extensionLength) == 0))
		{
			return true;
		}
	}

	return false;
}

static inline b32 instructionHasTag(Instruction *instruction, char *tag, size_t tagLength)
{
	if ((tagLength > 0) &&
		(instruction->tagLength == tagLength) &&
		(strncmp(tag, instruction->tag, tagLength) == 0))
	{
		return true;
	}
	
	return false;
}

static Instruction *getInstructionByExtension(char *extension, size_t extensionLength,
											  Instruction *allInstructions, int instructionCount)
{
	Instruction *instruction = allInstructions;
	
	for (int index = 0; index < instructionCount; ++index)
	{
		if (instructionHasExtension(instruction, extension, extensionLength))
		{
			return instruction;
		}
		
		++instruction;
	}

	return NULL;
}

static Instruction *getInstructionByTag(char *tag, size_t tagLength,
										Instruction *allInstructions, int instructionCount)
{
	Instruction *instruction = allInstructions;
	
	for (int index = 0; index < instructionCount; ++index)
	{
		if (instructionHasTag(instruction, tag, tagLength))
		{
			return instruction;
		}
		
		++instruction;
	}

	return NULL;
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
	
	int helpFlag = 0,
		versionFlag = 0;
	
	char onlyArray[10][64];
	size_t onlyArrayLength[10];
	int onlyArrayCount = 0;
	
	i32 optionFlags = OptionFlag_None;

	// NOTE: I will probably have to parse the command line myself, as
	//       getopt does not support multiple arguments for given option.
	static struct option longOptions[] =
		{
			{"help"							, no_argument, &helpFlag, 1},
			{"version"						, no_argument, &versionFlag, 1},
			{"which"						, no_argument, 0, 'w'},
			{"execute"						, no_argument, 0, 'e'},
			{"recursive"					, no_argument, 0, 'r'},
			{"recursive-keep-directories"	, no_argument, 0, 'R'},
			{"directory"					, no_argument, 0, 'd'},
			{"only"							, required_argument, 0, 'o'},
			{0								, 0, 0, 0}
		};
			
	int c;
	
	for(;;)
	{
		int optionIndex = 0;
		
		// I'll add an 'i' soon.
		// Promised.
		c = getopt_long(argc, argv, "werRdo:", longOptions, &optionIndex);

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
				optionFlags |= OptionFlag_Which;
				break;
			}
			case 'e':
			{
				optionFlags &= ~OptionFlag_Which;
				break;
			}
			case 'r':
			{
				optionFlags &= ~OptionFlag_Recursive_Keep_Directories;
				optionFlags |= OptionFlag_Recursive;
				break;
			}
			case 'R':
			{
				optionFlags &= ~OptionFlag_Recursive;
				optionFlags |= OptionFlag_Recursive_Keep_Directories;
				break;
			}
			case 'd':
			{
				optionFlags &= ~OptionFlag_Recursive;
				break;
			}
			case 'o':
			{
				optionFlags |= OptionFlag_Only;
				size_t length = strlen(optarg);
				
				if (length >= ARRAY_SIZE(onlyArray[0]))
				{
					char buffer[255];

					sprintf(buffer, "%s: -o/--only: %s is too long (> %d characters).\n",
							ME, optarg, (i32) ARRAY_SIZE(onlyArray[0]) - 1);
					fprintf(stderr, buffer);

					return -1;
				}
				
				onlyArrayLength[onlyArrayCount] = length;
				strcpy(onlyArray[onlyArrayCount++], optarg);
				
				break;
			}
			default:
			{
				return -1;
				break;
			}
		}
	}

	if (helpFlag)
	{
		printf("%s", usage);
		return 0;
	}

	if (versionFlag)
	{
		printf("%s", version);
		return 0;
	}

	// TODO: Properly get the number of entries.
	char allEntries[1024][255];
	int entryCount = 0;

	entryCount = argc - optind;

	if (entryCount <= 0)
	{
		char buffer[255];
		sprintf(buffer, "%s: no file given.\n", ME);
		fprintf(stderr, buffer);

		return 1;
	}

	// TODO: If onlyArgs:
	//       - Move instruction creation here.
	//       - For each argv left:
	//         If directory:
	//           - If recursive, add content.
	//           - If recursive and keep, add content BUT go to file part.
	//         If file:
	//           - Treat onlyArg element as extension:
	//             - If file same extension, add file.
	//             - If any file added, skip following step.
	//           - Treat onlyArg element as tag:
	//             - Get instruction by tag.
	//             - If file extension in instruction extensions, add file.
	//       - Save current index.
	//       - Do same on recursive add, as soon as i == current index.
	//
	//       - Go to exectute (or make a function exectuteInstructions).

	// Copy entries given from argv.
	for (int i = 0; i < entryCount; ++i)
	{
		strcpy(allEntries[i], argv[optind + i]);

		int indexLastChar = strlen(allEntries[i]) - 1;

		if ((indexLastChar > 0)
			&& allEntries[i][indexLastChar] == '/')
		{
			allEntries[i][indexLastChar] = '\0';
		}
	}

	// Add sub-directories recursively.
	if ((optionFlags & OptionFlag_Recursive) ||
		(optionFlags & OptionFlag_Recursive_Keep_Directories))
	{
		int i = 0;
		
		while(i != entryCount)
		{
			char *entry = allEntries[i];
			struct stat entryStat;
			stat(entry, &entryStat);

			if (S_ISDIR(entryStat.st_mode))
			{
				DIR *d;
				struct dirent *dir;
				char dirPath[255];
				strcpy(dirPath, entry);

				if (!(optionFlags & OptionFlag_Recursive_Keep_Directories))
				{
					// Remove the directory from the list and move
					// following entries one entry back.
					memcpy(allEntries + i, allEntries + i + 1,
						   (entryCount - i - 1) * sizeof(char) * ARRAY_SIZE(allEntries[0]));
					--entryCount;
					--i;
				}

				d = opendir(dirPath);

				if (d)
				{
					while ((dir = readdir(d)) != NULL)
					{
						ASSERT(entryCount + 1 < (i32) ARRAY_SIZE(allEntries));

						// Current and previous directory.
						if ((strcmp(dir->d_name, ".") == 0) ||
							(strcmp(dir->d_name, "..") == 0))
						{
							continue;
						}

						char buffer[255];
						sprintf(buffer, "%s/%s", dirPath, dir->d_name);
						
						strcpy(allEntries[entryCount++], buffer);
					}

					closedir(d);
				}
			}

			++i;
		}
	}
	
	Instruction allInstructions[42];
	int instructionCount = makeInstructionsFromConfig(configFile, allInstructions,
													  (i32) ARRAY_SIZE(allInstructions));
	Instruction *defaultInstruction = allInstructions;

	// Default instruction is the only one without any associated
	// extension (just after reading the config file).
	for (int index = 0; index < instructionCount; ++index)
	{
		if (!(defaultInstruction->extensionCount))
		{
			break;
		}

		++defaultInstruction;
	}
		
	if ((defaultInstruction - allInstructions) >= instructionCount)
	{
		defaultInstruction = NULL;
	}

	// Find corresponding command (based on entry's extension).
	for (int i = 0; i < entryCount; ++i)
	{
		char *entry = allEntries[i];
		char extension[64];
		
		// No directory if --recursive is set.
		if (optionFlags & OptionFlag_Recursive)
		{
			getFileExtension(entry, extension);
		}
		else
		{
			getExtension(entry, extension);
		}		
		
		size_t extensionLength = strlen(extension);

		b32 toSkip = false;
		
		if (onlyArrayCount)
		{
			toSkip = true;

			// Extension explicitly asked.
			for (int index = 0; index < onlyArrayCount; ++index)
			{
				if ((extensionLength == onlyArrayLength[index]) &&
					(strncmp(extension, onlyArray[index], extensionLength) == 0))
				{
					toSkip = false;
				}
			}
		}

		Instruction *instruction = getInstructionByExtension(extension, extensionLength,
															 allInstructions, instructionCount);

		if (instruction)
		{
			if (toSkip)
			{
				for (int index = 0; index < onlyArrayCount; ++index)
				{
					if (instructionHasTag(instruction, onlyArray[index], onlyArrayLength[index]))
					{
						toSkip = false;
					}
				}
			}

			if (toSkip)
			{
				continue;
			}
			
			ASSERT(instruction->argumentCount < (i32) ARRAY_SIZE(instruction->arguments));
					
			instruction->arguments[instruction->argumentCount++] = entry;
		}
		else if (defaultInstruction)
		{
			ASSERT(defaultInstruction->argumentCount < (i32) ARRAY_SIZE(defaultInstruction->arguments));

			if (toSkip)
			{
				for (int index = 0; index < onlyArrayCount; ++index)
				{
					if (instructionHasTag(defaultInstruction, onlyArray[index], onlyArrayLength[index]))
					{
						toSkip = false;
					}
				}
			}

			if (toSkip)
			{
				continue;
			}
				
			defaultInstruction->arguments[defaultInstruction->argumentCount++] = entry;
			strcpy(defaultInstruction->extensions[defaultInstruction->extensionCount], extension);
			defaultInstruction->extensionsLength[defaultInstruction->extensionCount++] = extensionLength;
		}
		else if (!toSkip)
		{
			// TODO?: Keep separate error message or group by extension?
			char buffer[255];
			sprintf(buffer, "%s: %s: no command specified for extension '%s'.\n",
					ME, entry, extension);
			fprintf(stderr, buffer);
		}
	}

	// Execute each command with associated entries.
	for (int i = 0; i < instructionCount; ++i)
	{
		Instruction instruction = allInstructions[i];

		if (!instruction.argumentCount)
		{
			continue;
		}

		// TODO: Move this part to config_file_parser (because
		//       shell functions' path will be infered there).
		char *whichArgs[] =
			{
				"which",
				instruction.command,
				NULL
			};
		
		int statusCode = 0;
		int status = childExec("/usr/bin/which", whichArgs, &statusCode,
							   instruction.commandPath, ARRAY_SIZE(instruction.commandPath));

		if (status != 0)
		{		
			continue;
		}

		// NOTE: If which did not find the command, we assume it's a
		//       shell function defined in ~/.bashrc.
		char *path = (statusCode == 0) ? instruction.commandPath : (char *) "~/.bashrc";
		
		if (optionFlags & OptionFlag_Which)
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
				
				ASSERT(((current + strlen(argument) + 1) - bashCommand) < (i32) ARRAY_SIZE(bashCommand));
				
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
