#include "int/export.h"

#include <ctype.h>
#include <string.h>

#include "int/intlib.h"
#include "int/memdbg.h"
#include "platform_compat.h"

namespace fallout {

typedef struct ExternalVariable {
    char name[32];
    char* programName;
    ProgramValue value;
    char* stringValue;
} ExternalVariable;

typedef struct ExternalProcedure {
    char name[32];
    Program* program;
    int argumentCount;
    int address;
} ExternalProcedure;

static unsigned int hashName(const char* identifier);
static ExternalProcedure* findProc(const char* identifier);
static ExternalProcedure* findEmptyProc(const char* identifier);
static ExternalVariable* findVar(const char* identifier);
static ExternalVariable* findEmptyVar(const char* identifier);
static void exportRemoveProgramReferences(Program* program);

// 0x56EED0
static ExternalProcedure procHashTable[1013];

// 0x579CEC
static ExternalVariable varHashTable[1013];

// 0x439A10
static unsigned int hashName(const char* identifier)
{
    unsigned int h = 0;
    const char* p = identifier;
    while (*p != '\0') {
        int ch = *p & 0xFF;
        h += (tolower(ch) & 0xFF) + (h * 8) + (h >> 29);
        p++;
    }

    h = h % 1013;
    return h;
}

// 0x439A58
static ExternalProcedure* findProc(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = hashName(identifier);
    unsigned int start = idx;

    ExternalProcedure* externalProcedure = &(procHashTable[idx]);
    if (externalProcedure->program != NULL) {
        if (compat_stricmp(externalProcedure->name, identifier) == 0) {
            return externalProcedure;
        }
    }

    do {
        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        externalProcedure = &(procHashTable[idx]);
        if (externalProcedure->program != NULL) {
            if (compat_stricmp(externalProcedure->name, identifier) == 0) {
                return externalProcedure;
            }
        }
    } while (idx != start);

    return NULL;
}

// 0x439B18
static ExternalProcedure* findEmptyProc(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = hashName(identifier);
    unsigned int start = idx;

    ExternalProcedure* externalProcedure = &(procHashTable[idx]);
    if (externalProcedure->name[0] == '\0') {
        return externalProcedure;
    }

    do {
        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        externalProcedure = &(procHashTable[idx]);
        if (externalProcedure->name[0] == '\0') {
            return externalProcedure;
        }
    } while (idx != start);

    return NULL;
}

// 0x439BAC
static ExternalVariable* findVar(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = hashName(identifier);
    unsigned int start = idx;

    ExternalVariable* exportedVariable = &(varHashTable[idx]);
    if (compat_stricmp(exportedVariable->name, identifier) == 0) {
        return exportedVariable;
    }

    do {
        exportedVariable = &(varHashTable[idx]);
        if (exportedVariable->name[0] == '\0') {
            break;
        }

        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        exportedVariable = &(varHashTable[idx]);
        if (compat_stricmp(exportedVariable->name, identifier) == 0) {
            return exportedVariable;
        }
    } while (idx != start);

    return NULL;
}

// 0x439C8C
static ExternalVariable* findEmptyVar(const char* identifier)
{
    // NOTE: Uninline.
    unsigned int idx = hashName(identifier);
    unsigned int start = idx;

    ExternalVariable* exportedVariable = &(varHashTable[idx]);
    if (exportedVariable->name[0] == '\0') {
        return exportedVariable;
    }

    do {
        idx += 7;
        if (idx >= 1013) {
            idx -= 1013;
        }

        exportedVariable = &(varHashTable[idx]);
        if (exportedVariable->name[0] == '\0') {
            return exportedVariable;
        }
    } while (idx != start);

    return NULL;
}

// 0x439D7C
int exportStoreVariable(Program* program, const char* name, ProgramValue& programValue)
{
    ExternalVariable* exportedVariable = findVar(name);
    if (exportedVariable == NULL) {
        return 1;
    }

    if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        myfree(exportedVariable->stringValue, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 169
    }

    if ((programValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        if (program != NULL) {
            const char* stringValue = interpretGetString(program, programValue.opcode, programValue.integerValue);
            exportedVariable->value.opcode = VALUE_TYPE_DYNAMIC_STRING;

            exportedVariable->stringValue = (char*)mymalloc(strlen(stringValue) + 1, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 175
            strcpy(exportedVariable->stringValue, stringValue);
        }
    } else {
        exportedVariable->value = programValue;
    }

    return 0;
}

// 0x439ED4
int exportFetchVariable(Program* program, const char* name, ProgramValue& value)
{
    ExternalVariable* exportedVariable = findVar(name);
    if (exportedVariable == NULL) {
        return 1;
    }

    if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        value.opcode = exportedVariable->value.opcode;
        value.integerValue = interpretAddString(program, exportedVariable->stringValue);
    } else {
        value = exportedVariable->value;
    }

    return 0;
}

// 0x439FB8
int exportExportVariable(Program* program, const char* identifier)
{
    const char* programName = program->name;
    ExternalVariable* exportedVariable = findVar(identifier);

    if (exportedVariable != NULL) {
        if (compat_stricmp(exportedVariable->programName, programName) != 0) {
            return 1;
        }

        if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            myfree(exportedVariable->stringValue, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 234
        }
    } else {
        exportedVariable = findEmptyVar(identifier);
        if (exportedVariable == NULL) {
            return 1;
        }

        strncpy(exportedVariable->name, identifier, 31);

        exportedVariable->programName = (char*)mymalloc(strlen(programName) + 1, __FILE__, __LINE__); // // "..\\int\\EXPORT.C", 243
        strcpy(exportedVariable->programName, programName);
    }

    exportedVariable->value.opcode = VALUE_TYPE_INT;
    exportedVariable->value.integerValue = 0;

    return 0;
}

// 0x439FFC
static void exportRemoveProgramReferences(Program* program)
{
    for (int index = 0; index < 1013; index++) {
        ExternalProcedure* externalProcedure = &(procHashTable[index]);
        if (externalProcedure->program == program) {
            externalProcedure->name[0] = '\0';
            externalProcedure->program = NULL;
        }
    }
}

// 0x43A02C
void initExport()
{
    interpretRegisterProgramDeleteCallback(exportRemoveProgramReferences);
}

// 0x43A038
void exportClose()
{
    for (int index = 0; index < 1013; index++) {
        ExternalVariable* exportedVariable = &(varHashTable[index]);

        if (exportedVariable->name[0] != '\0') {
            myfree(exportedVariable->programName, __FILE__, __LINE__); // ..\\int\\EXPORT.C, 274
        }

        if (exportedVariable->value.opcode == VALUE_TYPE_DYNAMIC_STRING) {
            myfree(exportedVariable->stringValue, __FILE__, __LINE__); // ..\\int\\EXPORT.C, 276
        }
    }
}

// 0x43A08C
Program* exportFindProcedure(const char* identifier, int* addressPtr, int* argumentCountPtr)
{
    ExternalProcedure* externalProcedure = findProc(identifier);
    if (externalProcedure == NULL) {
        return NULL;
    }

    if (externalProcedure->program == NULL) {
        return NULL;
    }

    *addressPtr = externalProcedure->address;
    *argumentCountPtr = externalProcedure->argumentCount;

    return externalProcedure->program;
}

// 0x43A0B0
int exportExportProcedure(Program* program, const char* identifier, int address, int argumentCount)
{
    ExternalProcedure* externalProcedure = findProc(identifier);
    if (externalProcedure != NULL) {
        if (program != externalProcedure->program) {
            return 1;
        }
    } else {
        externalProcedure = findEmptyProc(identifier);
        if (externalProcedure == NULL) {
            return 1;
        }

        strncpy(externalProcedure->name, identifier, 31);
    }

    externalProcedure->argumentCount = argumentCount;
    externalProcedure->address = address;
    externalProcedure->program = program;

    return 0;
}

// 0x43A324
void exportClearAllVariables()
{
    for (int index = 0; index < 1013; index++) {
        ExternalVariable* exportedVariable = &(varHashTable[index]);
        if (exportedVariable->name[0] != '\0') {
            if ((exportedVariable->value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
                if (exportedVariable->stringValue != NULL) {
                    myfree(exportedVariable->stringValue, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 387
                }
            }

            if (exportedVariable->programName != NULL) {
                myfree(exportedVariable->programName, __FILE__, __LINE__); // "..\\int\\EXPORT.C", 393
                exportedVariable->programName = NULL;
            }

            exportedVariable->name[0] = '\0';
            exportedVariable->value.opcode = 0;
        }
    }
}

} // namespace fallout
