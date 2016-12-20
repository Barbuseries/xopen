#ifndef XOPEN_COMMON_H
#define XOPEN_COMMON_H

struct Instruction
{
	char command[255];
	char commandPath[255];
	size_t extensionsLength[255];

	char extensions[255][64];

	// TODO: Actually get the number of arguments.
	char *arguments[255];
	
	int commandLength;
	int argumentCount;
	int extensionCount;

	// TODO: Add tagging system.
	// char allTags[255];
	// int tagCount;
};

#endif
