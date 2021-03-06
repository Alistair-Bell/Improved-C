#include "Compiler.h"


static HC_Compiler *Compiler;

static U8 HC_CompilerValidateFile(const char *file)
{
    U64 fileStrlen = strlen(file);
    if (fileStrlen <= 2)
    {
        printf("Argument %s is an invalid file\n", file);
        return HC_False;
    }
    char extension[2];
    memset(extension, 0, sizeof(extension));
    strncpy(extension, file + (fileStrlen - 2), 2);
    
    /* file extensions are important for the lexer */
    U8 sourceFile = (strcmp(extension, "HC") == 0);
    U8 headerFile = (strcmp(extension, "HH") == 0);
    if (!(headerFile || sourceFile))
    {
        printf("Invalid extension %s, valid HH or HC\n", extension);
        return HC_False;
    }

    return HC_True;
}
static const char *HC_CompilerParseArguments(const U32 argumentCount, char **arguments)
{
    if (argumentCount <= 1)
    {
        printf("No arguments specified\n");
        return NULL;
    }
    const char *file = arguments[1];
    if (!HC_CompilerValidateFile(file))
        return NULL;

    return arguments[1];
}

I32 main(I32 argumentCount, char **arguments)
{
    Compiler            = malloc(sizeof(HC_Compiler));
     
    HC_LexerCreateInfo li;
    memset(&li, 0, sizeof(HC_LexerCreateInfo));
    const char *loading = HC_CompilerParseArguments(argumentCount, arguments);
    if (loading == NULL)
        return HC_False;

    li.Loading = (HC_LexerLoadStreamInfo) { .Input = loading, .LexerFlags = 0 };

    HC_CompilerCreateInfo ci;
    memset(&ci, 0, sizeof(HC_CompilerCreateInfo));
    ci.LexerInfo = li;

    HC_CompilerCreate(Compiler, &ci);
    HC_CompilerRun(Compiler);

    HC_CompilerDestroy(Compiler);
    free(Compiler);
    return 1;
}
