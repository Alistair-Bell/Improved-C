#include "Lexer.h"

#define HC_LEXER_TOKEN_STRING_LENGTH 255
#define HC_LEXER_TOKEN_ROUND_COUNT 255

inline U8 HC_LexerCheckTerminationCharacterOrWhitespace(const char currentChar, U32 *newLine)
{
    *newLine += (currentChar == '\n');
    
    switch (currentChar)
    {
        /* 
            All the following are ascii values
            See http://www.asciitable.com/ 
            We use ascii not unicode 
            The orginal OS used 8 bit unsigned ascii as Terry was against 7 bit signed
            The lexer treats all chars as ascii but using unicode can cause unwanted effects
        */

        case 10:  return HC_True;  /* \n */
        case 32:  return HC_True;  /*  */
        case 34:  return HC_True;  /* " */
        case 39:  return HC_True;  /* ' */
        case 40:  return HC_True;  /* ( */
        case 41:  return HC_True;  /* ) */
        case 44:  return HC_True;  /* , */
        case 46:  return HC_True;  /* . */
        case 59:  return HC_True;  /* ; */
        case 91:  return HC_True;  /* [ */
        case 93:  return HC_True;  /* ] */
        case 123: return HC_True;  /* { */
        case 125: return HC_True;  /* } */
        default:  return HC_False; /* any other char */

    }
    
    
    return currentChar == '\n'
        || currentChar == ' ' 
        || currentChar == '(' 
        || currentChar == ')' 
        || currentChar == '{' 
        || currentChar == '}'
        || currentChar == '['
        || currentChar == ']'
        || currentChar == ';'
        || currentChar == '\''  /* single quote */
        || currentChar == '\"'; /* double quote */
}
inline U8 HC_LexerCheckTerminationCharacterNotWhitespace(const char currentChar, U32 *newLine)
{
    return HC_LexerCheckTerminationCharacterOrWhitespace(currentChar, newLine) && (currentChar != ' ' && currentChar != '\n');
}

static inline U8 HC_LexerAddToken(HC_Lexer *l, HC_Token *t, HC_Token *pt, U64 lineCount, const char *src, const U64 count)
{
    HC_TokenHandleInfo h;
    memset(&h, 0, sizeof(HC_TokenHandleInfo));
    strncpy(h.Source, src, count);
    h.Lexer         = l;
    h.PreviousToken = pt;
    h.SourceLength  = count;
    h.Line          = lineCount;
    HC_TokenCreate(t, &h);
    return HC_True;
}
static inline U8 HC_LexerHandleNewToken(HC_Lexer *l, U8 *strMode, U8 *commentMode, U64 *tokenCount, const U64 lineCount, const char *src, const U64 count)
{
    HC_Token t;
    HC_Token *lt;
    if (l->File->TokenCount > 1)
        lt = &l->File->Tokens[*tokenCount - 1];
    else
        lt = NULL;

    HC_LexerAddToken(l, &t, lt, lineCount, src, count);
    
    switch (t.Hash)
    {
        case HC_LEXICAL_TOKENS_STARTING_COMMENT_STRING_HASH:
            *commentMode = HC_True;
            break;
        case HC_LEXICAL_TOKENS_ENDING_COMMENT_STRING_HASH:
            *commentMode = HC_False;
            break;
        case HC_LEXICAL_TOKENS_DOUBLE_QUOTE_STRING_HASH:
        {
            *strMode = 1 - (*strMode);
            if (!(*commentMode))
                goto addToken;
        }

        case HC_LEXICAL_TOKENS_SINGLE_QUOTE_STRING_HASH:
            *strMode = 1 - (*strMode);
            if (!(*commentMode))
                goto addToken;
        
        default:
        {
         
            if (!(*commentMode))
                goto addToken;
            else
                goto end;

            addToken:
            {
                l->File->Tokens = realloc(l->File->Tokens, sizeof(HC_Token) * (*tokenCount + 1));
                memcpy(&l->File->Tokens[*tokenCount], &t, sizeof(HC_Token));
                *tokenCount += 1;
            }
        }
    }
    end:
        return HC_True;
}

U8 HC_LexerCreate(HC_Lexer *lexer, HC_LexerCreateInfo *info)
{
    assert(lexer != NULL);
    assert(info != NULL);
    memset(lexer, 0, sizeof(HC_Lexer));

    HC_LexerLoadStream(lexer, &info->Loading);
    return HC_True;
}
U8 HC_LexerLoadStream(HC_Lexer *lexer, HC_LexerLoadStreamInfo *info)
{
    HC_CompilingFile file;
    memset(&file, 0, sizeof(HC_CompilingFile));
    file.CurrentStream = calloc(0, sizeof(char *));

    HC_Stream stream;
    memset(&stream, 0, sizeof(HC_Stream));
    stream.Path = info->Input;
    stream.Reallocatable = 1;
    stream.Data = file.CurrentStream;
    stream.StreamSize = &file.CharCount;
    U8 result = HC_StreamCreate(&stream);
    
    if (!result)
    {
        printf("Unable to load stream\n");
        return HC_False;
    }
    
    lexer->File = malloc(sizeof(HC_Lexer));

    file.CurrentStream  = stream.Data;
    file.Tokens         = calloc(0, sizeof(HC_Token));
    file.TokenCount     = 0;
    file.FileName       = info->Input;
    memcpy(lexer->File, &file, sizeof(HC_CompilingFile));
    return HC_True;
}
U8 HC_LexerParse(HC_Lexer *lexer)
{
    assert(lexer != NULL);
    if (lexer->File->CharCount <= 0)
    {
        printf("Error: Lack of data present! [%s]", lexer->File->FileName);
        return HC_False;
    }

    U64 i = 0; /* counter */
    U64 leftPinscor     = 0; /* start index for the new token string */
    U64 rightPinscor    = 0; /* end index for the new token string */
    char localSource[lexer->File->CharCount]; /* local source to prevent stream modifications */
    U64 tokenCount = 0;
    
    U8 stringMode   = HC_False;
    U8 commentMode  = HC_False;

    U32 newLine = 1;
    
    HC_Token tokens[HC_LEXER_TOKEN_ROUND_COUNT]; /* all the tokens found */
    memset(tokens, 0, sizeof(HC_Token) * HC_LEXER_TOKEN_ROUND_COUNT); 
    
    char localBuffer[HC_LEXER_TOKEN_STRING_LENGTH]; /* local buffer for the raw token data */
    memset(localBuffer, 0, sizeof(char) * HC_LEXER_TOKEN_STRING_LENGTH);

    char localBufferCopy[HC_LEXER_TOKEN_STRING_LENGTH];
    memset(localBufferCopy, 0, sizeof(char) * HC_LEXER_TOKEN_STRING_LENGTH);

    strcpy(localSource, lexer->File->CurrentStream);
    while (HC_True)
    {
        char currentChar = localSource[i];
        if (HC_LexerCheckTerminationCharacterOrWhitespace(currentChar, &newLine))
        {
            rightPinscor = i;
            U64 diff = rightPinscor - leftPinscor;
            
            if (leftPinscor == rightPinscor || rightPinscor < leftPinscor || diff == 0)
                goto update;

            strncpy(localBuffer, localSource + leftPinscor, diff);
            HC_LexerStripToken(localBuffer);
            
            /* First char token */
            if (HC_LexerCheckTerminationCharacterNotWhitespace(localBuffer[0], &newLine))
            {
                HC_LexerHandleNewToken(lexer, &stringMode, &commentMode, &tokenCount, newLine, localBuffer, 1);
                /* Offset new buffer by one to ignore the last added*/
                strcpy(localBufferCopy, localBuffer);
                strncpy(localBuffer, localBufferCopy + 1, strlen(localBufferCopy));
            }

            /* Rest token */
            if (strlen(localBuffer) != 0)
                HC_LexerHandleNewToken(lexer, &stringMode, &commentMode, &tokenCount, newLine, localBuffer, diff);

            update:
                leftPinscor = rightPinscor;
        }
        
        if (i == strlen(localSource) + 1)
            break;
        
        i++;
        memset(localBuffer, 0, sizeof(localBuffer));
    }
    {
        /* Last token */
        U64 diff = strlen(localSource) - leftPinscor;
        strncpy(localBuffer, localSource + leftPinscor, diff);

        HC_LexerStripToken(localBuffer);
        if (strlen(localBuffer) != 0)
        {
            lexer->File->Tokens = realloc(lexer->File->Tokens, sizeof(HC_Token) * (tokenCount + 1));
            
            if (tokenCount > 1)
                HC_LexerAddToken(lexer, &lexer->File->Tokens[tokenCount], &lexer->File->Tokens[tokenCount - 1], newLine, localBuffer, diff);
            else
                HC_LexerAddToken(lexer, &lexer->File->Tokens[tokenCount], NULL, newLine, localBuffer, diff);
            tokenCount++;
        }
    }
    
    lexer->File->TokenCount = tokenCount;
    return HC_True;
}

U8 HC_LexerDestroy(HC_Lexer *lexer)
{
    free(lexer->File->CurrentStream);
    free(lexer->File->Tokens);
    free(lexer->File);

    return HC_True;
}
U8 HC_LexerStripToken(char *src)
{
    U32 i;
    U32 begin = 0;
    U32 end = strlen(src) - 1;

    while (isspace((U8) src[begin]))
        begin++;

    while ((end >= begin) && isspace((U8) src[end]))
        end--;

    // Shift all characters back to the start of the string array.
    for (i = begin; i <= end; i++)
        src[i - begin] = src[i];

    src[i - begin] = '\0'; // Null terminate string.
    return HC_True;
}
