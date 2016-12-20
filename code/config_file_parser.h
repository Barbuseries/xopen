#ifndef CONFIG_FILE_PARSER_H
#define CONFIG_FILE_PARSER_H
#include "xopen_common.h"

int makeInstructionsFromConfig(char *configFile, Instruction *allInstructions, int allInstructionsSize);

#endif
