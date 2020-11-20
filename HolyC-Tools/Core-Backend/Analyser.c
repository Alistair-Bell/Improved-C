#include "Analyser.h"

/*
    All warnings and errors are treated as fatal
    Good code has no warnings, its the programers problem to fix the warnings
    Its lazy to ignore warnings, that is why cmake uses -Wfatal
*/

typedef struct HC_SyntaxAnalyserExpressionCreateInfo
{
    U64 Offset;
    U64 Count;
    U16 Scope;
} HC_SyntaxAnalyserExpressionCreateInfo;

static inline U0 HC_QuickSort(U16 *numbers, const U16 first, const U16 last)
{
    U16 i, j, pivot, temp;
    if (0 < last)
    {
        pivot = first;
        i     = first;
        j     = last;
        

        while (i < j)
        {
            while (numbers[i] <= numbers[pivot]&& i < last)
            {
                i++;
                while(numbers[j] > numbers[pivot])
                {
                    j--;
                    if(i < j)
                    {
                        temp       = numbers[i];
                        numbers[i] = numbers[j];
                        numbers[j] = temp;
                    }
                }
            }
        }

        temp            = numbers[pivot];
        numbers[pivot]  = numbers[j];
        numbers[j]      = temp;
        HC_QuickSort(numbers, first, j - 1);
        HC_QuickSort(numbers, j + 1, last);
    }
}
static inline I64 HC_BinarySearch(U64 *hashes, U32 low, U32 high, U64 searching)
{
    if (low <= high)
	{
		U32 mid = low + (high - low) / 2;
		if (searching == hashes[mid])
			return mid;
		
        else if (searching < hashes[mid])
			return HC_BinarySearch(hashes, low, mid - 1, searching);
		
        else
			return HC_BinarySearch(hashes, mid + 1, high, searching);
	}
	return -1;
}
static U8 HC_SyntaxAnalyserCheckRepeatedSymbol(HC_SyntaxAnalyser *analyser, U16 scope, U64 hash)
{
    HC_SyntaxAnalyserSymbolTable *t = &analyser->SymbolTables[scope - 1];
    U64 hashes[t->Count];
    memcpy(hashes, t->Symbols, sizeof(HC_SyntaxAnalyserSymbol) * t->Count);
    return (HC_BinarySearch(hashes, 0, t->Count, hash) != -1);
}
static U8 HC_SyntaxAnalyserCheckBuiltInType(const U64 hash)
{
    switch (hash)
    {
        case HC_LEXICAL_TOKENS_U0_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_I8_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_U8_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_I16_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_U16_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_I32_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_U32_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_I64_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_U64_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_F32_STRING_HASH: return HC_True;
        case HC_LEXICAL_TOKENS_F64_STRING_HASH: return HC_True;
    }
    return HC_False;
}

static U8 HC_SyntaxAnalyserCreateExpressions(HC_SyntaxAnalyser *analyser, HC_SyntaxAnalyserExpressionCreateInfo *info)
{
    HC_Token *indexer = &analyser->Analysing[info->Offset + 0];
    
    /* Check built in type */
    if (HC_SyntaxAnalyserCheckBuiltInType(indexer->Hash))
    {
        if (info->Count == 1)
        {
            printf("Invalid syntax [line %d]: Expected expression after %s\n", indexer->Line, indexer->Source);
            return HC_False;
        }
        
        indexer++;
        
        /* Check repetition */
        if (HC_SyntaxAnalyserCheckBuiltInType(indexer->Hash) || HC_SyntaxAnalyserCheckRepeatedSymbol(analyser, info->Scope, indexer->Hash))
        {
            printf("Invalid syntax [line %d]: Repetition of type or symbol declaration %s\n", indexer->Line, indexer->Source);
            return HC_False;
        }
        else
        {
            printf("New symbol added: of %s with type %s\n", indexer->Source, analyser->Analysing[info->Offset].Source);
            /* Add to symbol table and validate no others in outwards scopes */
        }

    }
    /* Not built in type (referencing old symbol) */
    else
    {
        
    }
    
    return HC_True;
}
U8 HC_SyntaxAnalyserAddSymbol(HC_SyntaxAnalyser *analyser, HC_Token *current, U16 scope)
{
    return HC_True;
}



U8 HC_SyntaxAnalyserCreate(HC_SyntaxAnalyser *analyser, HC_SyntaxAnalyserCreateInfo *createInfo)
{
    assert(analyser != NULL);
    assert(createInfo != NULL);

    memset(analyser, 0, sizeof(HC_SyntaxAnalyser));
    HC_Lexer *l = createInfo->Lexer;

    analyser->StreamName = l->CurrentFile->FileName;
    analyser->AnalysingCount = l->CurrentFile->TokenCount;
    analyser->SymbolTables = calloc(1, sizeof(HC_SyntaxAnalyserSymbolTable));
    analyser->TableCount   = 1;

    /* Global table */
    memset(&analyser->SymbolTables[0], 0, sizeof(HC_SyntaxAnalyserSymbolTable));
    analyser->SymbolTables[0].Symbols = calloc(0, sizeof(HC_SyntaxAnalyserSymbol));
    analyser->SymbolTables[0].Scope   = 0; /* Universal global scope */

    U64 tc = l->CurrentFile->TokenCount;

    analyser->Analysing = calloc(l->CurrentFile->TokenCount, sizeof(HC_Token));
    memcpy(analyser->Analysing, l->CurrentFile->Tokens, tc * sizeof(HC_Token));
    return HC_True;
}
U8 HC_SyntaxAnalyserAnalyse(HC_SyntaxAnalyser *analyser)
{
    assert(analyser != NULL);
    printf("Analysing %s\n", analyser->StreamName);
    
    if (analyser->AnalysingCount <= 1)
    {
        printf("Expected expression!: Lack of tokens recognised \n");
        return HC_False;
    }
    
    U64 i         = 0;
    U64 leftPnsr  = 0;
    U64 rightPnsr = 0;
    U16 scope     = 0;

    while (HC_True)
    {
        rightPnsr = i;
        HC_Token *current = &analyser->Analysing[i];
        switch (current->Token)
        {
            case HC_LEXICAL_TOKENS_SEMI_COLON:
            {
                HC_SyntaxAnalyserExpressionCreateInfo ci;
                ci.Count  = (rightPnsr - leftPnsr);
                ci.Offset = leftPnsr;
                ci.Scope  = scope;
                if (HC_SyntaxAnalyserCreateExpressions(analyser, &ci) == HC_False)
                {
                    printf("Failed to analyse %s\n", analyser->StreamName);
                    return HC_False;
                }
                leftPnsr = rightPnsr + 1; /* ignore next semi colon */
                break;
            }
            case HC_LEXICAL_TOKENS_LEFT_CURLY_BRACKET:
            {
                analyser->SymbolTables = realloc(analyser->SymbolTables, sizeof(HC_SyntaxAnalyserSymbolTable) * (scope + 1));
                scope++;
                break;
            }
            case HC_LEXICAL_TOKENS_RIGHT_CURLY_BRACKET:
            {
                if (scope == 0)
                {
                    printf("Syntax error [line %d]: trying to dereference scope, %s detected\n", current->Line, current->Source);
                    printf("Failed to analyse %s\n", analyser->StreamName);
                    return HC_False;
                }
                analyser->SymbolTables = realloc(analyser->SymbolTables, sizeof(HC_SyntaxAnalyserSymbolTable) * (scope + 1));
                scope--;
                break;
            }
            default:
                goto update;
        }

        update:
        {
            if (i == analyser->AnalysingCount)
                break;

            i++;
        }

    }

    return HC_True;
}
U8 HC_SyntaxAnalyserDestroy(HC_SyntaxAnalyser *analyser)
{
    U64 i;
    for (i = 0; i < analyser->TableCount; i++)
        free(analyser->SymbolTables[i].Symbols);
    free(analyser->SymbolTables);
    free(analyser->Analysing);
    return HC_True;
}