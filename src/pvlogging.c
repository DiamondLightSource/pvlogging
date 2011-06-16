#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbAccess.h>       // review
#include <iocsh.h>
#include <asTrapWrite.h>
#include <epicsExport.h>
#include <registryFunction.h>



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                            IOC PV put logging                             */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct formatted
{
    long length;
    epicsOldString values[1];
};

static struct formatted * FormatValue(struct dbAddr *dbaddr)
{
    struct formatted *formatted =
        malloc(sizeof(struct formatted) +
            dbaddr->no_elements * sizeof(epicsOldString));

    /* Start by using dbGetField() to format everything.  This will also update
     * the length. */
    formatted->length = dbaddr->no_elements;
    dbGetField(
        dbaddr, DBR_STRING, formatted->values, NULL, &formatted->length, NULL);

    /* Alas dbGetField is rather rubbish at formatting floating point numbers,
     * so if that's what we've got redo everything ourselves. */
#define FORMAT(type, format) \
    do { \
        type *raw = (type *) dbaddr->pfield; \
        int i; \
        for (i = 0; i < formatted->length; i ++) \
            sprintf(formatted->values[i], format, raw[i]); \
    } while (0)

    switch (dbaddr->dbr_field_type)
    {
        case 2:     // Should be DBR_FLOAT, but broken!
            FORMAT(epicsFloat32, "%.7g");
            break;
        case 6:     // Should be DBR_DOUBLE
            FORMAT(epicsFloat64, "%.15g");
            break;
    }
#undef FORMAT
    return formatted;
}

static void PrintValue(struct formatted *formatted)
{
    if (formatted->length == 1)
        printf("%s", formatted->values[0]);
    else
    {
        int i;
        printf("[");
        for (i = 0; i < formatted->length; i ++)
        {
            if (i > 0)  printf(", ");
            printf("%s", formatted->values[i]);
        }
        printf("]");
    }
}

static void EpicsPvPutHook(struct asTrapWriteMessage *pmessage, int after)
{
    struct dbAddr *dbaddr = pmessage->serverSpecific;
    struct formatted *value = FormatValue(dbaddr);

    if (after)
    {
        /* Log the message after the event. */
        struct formatted *old_value = pmessage->userPvt;
        printf("%s@%s %s.%s ",
            pmessage->userid, pmessage->hostid,
            dbaddr->precord->name, dbaddr->pfldDes->name);
        PrintValue(old_value);
        printf(" -> ");
        PrintValue(value);
        printf("\n");

        free(old_value);
        free(value);
    }
    else
        /* Just save the old value for logging after. */
        pmessage->userPvt = value;
}

static void InstallPvPutHook(const iocshArgBuf *args)
{
    asTrapWriteRegisterListener(EpicsPvPutHook);
}

static const iocshFuncDef InstallPvPutHookDef = { "InstallPvPutHook", 0, NULL };

static void RegisterPvPutHook(void)
{
    iocshRegister(&InstallPvPutHookDef, InstallPvPutHook);
}

epicsExportRegistrar(RegisterPvPutHook);
