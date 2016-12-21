#ifndef XOPEN_COMMON_H
#define XOPEN_COMMON_H

#define ME "xopen"

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

	// NOTE: Only one tag for now.
	char *tag;
	size_t tagLength;
};

#endif
