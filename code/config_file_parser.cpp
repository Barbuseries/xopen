#include "ef_utils.h"
#include "config_file_parser.h"

#include <string.h>

enum TokenType
{
	Token_EOF,
	Token_Unknown,

	Token_Minus,
	Token_OpenParenthesis,
	Token_CloseParenthesis,
	Token_At,
	Token_Percent, // TODO: May be used as a substitution indicator.
	Token_Newline,
	
	Token_Literal,
	
	Token_LineComment,

	/* ... */
};

struct Token
{
	char *text;
	size_t length;
	TokenType type;
};

struct Tokenizer
{
    char *at;
};

enum InstructionTokenType
{
	Instruction_Command,
	Instruction_Parameter,
	Instruction_Path,
	Instruction_Argument,
	Instruction_Tag,
};

static char *readEntireFile(char *filename)
{
	char *result = NULL;
	
	FILE *file = fopen(filename, "r");

	if (file)
	{
		fseek(file, 0, SEEK_END);
		
		size_t fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);


		result = (char *) malloc(fileSize + 1);
		
		fread(result, fileSize, 1, file);
		result[fileSize] = '\0';
		
		fclose(file);
	}

	return result;
}

static inline b32 isWhitespace(char c)
{
	return ((c == ' ') ||
			(c == '\t') ||
			(c == '\r'));
}

static inline b32 isAlpha(char c)
{
	return (((c >= 'a') && (c <= 'z')) ||
			((c >= 'A') && (c <= 'Z')));
}

static inline b32 isNumeric(char c)
{
	return (c >= '0') && (c <= '9');
}

static inline b32 isEndOfLine(char c)
{
	return ((c == '\n') ||
			(c == '\r'));
}

static inline b32 isValidLiteralChar(char c)
{
	return (isAlpha(c) ||
			isNumeric(c) ||
			(c == '/'));
}

static inline void skipWhitespace(Tokenizer *tokenizer)
{
	while (isWhitespace(tokenizer->at[0]))
	{
		++(tokenizer->at);
	}
}

static Token getNextToken(Tokenizer *tokenizer)
{
	Token token = {};

	do
	{
		skipWhitespace(tokenizer);
		
		token.length = 1;
		token.text = tokenizer->at;
	
		switch (*tokenizer->at)
		{
			case '\0': {token.type	= Token_EOF; return token;	break;}

			case '-': {token.type	= Token_Minus;	break;}
			case '(': {token.type	= Token_OpenParenthesis;	break;}
			case ')': {token.type	= Token_CloseParenthesis;	break;}
			case '@': {token.type	= Token_At;					break;}
			case '%': {token.type	= Token_Percent;			break;}
			case '\n': {token.type	= Token_Newline;			break;}
			case '#':
			{
				++(tokenizer->at);

				token.type = Token_LineComment;
				token.text = tokenizer->at;
					
				while(tokenizer->at[0] &&
					  !isEndOfLine(tokenizer->at[0]))
				{
					++(tokenizer->at);
				}

				token.length = tokenizer->at - token.text;
				break;
			}
			
			default:
			{
				token.text = tokenizer->at;

				if (isValidLiteralChar(tokenizer->at[0]))
				{
					token.type = Token_Literal;

					while (isValidLiteralChar((++tokenizer->at)[0]))
					{
					}

					token.length = tokenizer->at - token.text;
					--(tokenizer->at);
				}
				else
				{
					token.type = Token_Unknown;
				}
			
				break;
			}
		}
		
		++(tokenizer->at);
	} while (token.type == Token_LineComment);

	return token;
}

static b32 tokenEquals(Token *token, char *text)
{
	char *c1 = token->text, *c2 = text;
	size_t len = token->length;

	for (size_t i = 0; i < len; ++i)
	{
		if (!(*c2) ||
			(*c1 != *c2))
		{
			return false;
		}
		
		++c1;
		++c2;
	}

	return true;
}

static Token getToken(Tokenizer *tokenizer, TokenType type)
{
	Token token = {};

	do
	{
		token = getNextToken(tokenizer);

	} while ((token.type != Token_EOF) && (token.type != type));

	return token;
}

static Token getToken(Tokenizer *tokenizer, char *text)
{
	Token token = {};

	do
	{
		token = getNextToken(tokenizer);

	} while ((token.type != Token_EOF) &&
			 ((token.type != Token_Literal) ||
			  (!tokenEquals(&token, text))));
	
	return token;
}

/*
  IMPORTANT
   
  Memory allocated to parse the config file must not be freed until
  instructions are no longer used.
  This allow not having to copy/paste the strings around.
   
*/
// Return the number of instructions (it it's 0 or if there is an
// error, the memory is freed).
int makeInstructionsFromConfig(char *configFile, Instruction *allInstructions,
							   int allInstructionsSize)
{
	char *content = readEntireFile(configFile);

	ASSERT(content);

	Tokenizer tokenizer = {};
	tokenizer.at = content;

	b32 parsing = true;
	
	// Shorter to write if we start before. 
	Instruction *instruction = allInstructions - 1;
	
	Token token;
	InstructionTokenType instructionTokenType = Instruction_Command;
		
	do
	{
		token = getNextToken(&tokenizer);

		switch (token.type)
		{
			case Token_EOF: {parsing = false; break;}

				//case Token_Percent: {instructionTokenType = Instruction_Parameter ;		break;}
				// TODO: When we allow parameters, check to see if
				//       following char is space or not (if it is,
				//       instructionTokenType is Instruction_Parameter).
				//       
				//       Also check that instruction->command is not
				//       empty, as this would be an error in the config
				//       file. (This stands for other InstructionTypes as well)
			case Token_Minus: {instructionTokenType = Instruction_Argument;				break;}
			case Token_Newline: {instructionTokenType = Instruction_Command;			break;}
				//	case Token_OpenParenthesis: {instructionTokenType = Instruction_Path;		break;}
				//
				//  NOTE: Should the path be given before or after the
				//  command? (or allow both?)
				//	case Token_CloseParenthesis: {instructionTokenType = Instruction_Command;	break;}
				//	case Token_At: {instructionTokenType = Instruction_Tag ;					break;}
			case Token_Literal:
			{
				switch(instructionTokenType)
				{
					case Instruction_Command:
					{
						ASSERT((instruction - allInstructions) < allInstructionsSize);
						++instruction;

						// TODO: See main.cpp. (Move which here).
						//       Separate command from it's path (in
						//       parenthesis) when given.
						instruction->commandPath[0] = '\0';
						
						strncpy(instruction->command, token.text, token.length);
						instruction->commandLength = token.length;

						instruction->argumentCount = 0;
						instruction->extensionCount = 0;

						instructionTokenType = Instruction_Parameter;
						
						break;
					}
					case Instruction_Argument:
					{
						int extensionIndex = instruction->extensionCount++;
						
						strncpy(instruction->extensions[extensionIndex],
								token.text, token.length);
						instruction->extensionsLength[extensionIndex] = token.length;
						
						break;
					}
					default:
					{
						parsing = false;
							
						instruction = allInstructions - 1;
						break;
					}
				}
				
				break;
			}

			default:
			{
				break;
			}
		}
	} while (token.type != Token_EOF && parsing);

	int instructionCount = (instruction - allInstructions) + 1;

	if (!instructionCount)
	{
		free(content);
	}

	return instructionCount;
}
