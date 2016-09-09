#ifdef _WIN32
/* No stdbool on Windows. */
typedef int bool;
#define false 0
#define true 1
#else
#include <stdbool.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define db_accessHFORdb_accessC     // Needed to get correct DBF_ values
#include <dbAccess.h>
#include <dbFldTypes.h>
#include <iocsh.h>
#include <asTrapWrite.h>
#include <gpHash.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <registryFunction.h>


/* This function can be called to load a list of PVs to blacklist. */
extern void load_logging_blacklist(const char *blacklist);

/* This function can be used to enable or disable logging. */
extern void set_logging_enable(bool enabled);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                            IOC PV put logging                             */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool logging_enabled = true;
static struct gphPvt *blacklist;


struct formatted
{
    long length;
    epicsOldString values[0];
};

static struct formatted *FormatValue(struct dbAddr *dbaddr)
{
    struct formatted *formatted =
        malloc(sizeof(struct formatted) +
            dbaddr->no_elements * sizeof(epicsOldString));

    /* Start by using dbGetField() to format everything.  This will also update
     * the length, which is why we do this first. */
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

    switch (dbaddr->field_type)
    {
        case DBF_FLOAT:
            FORMAT(epicsFloat32, "%.7g");
            break;
        case DBF_DOUBLE:
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

/* Searches the blacklist for the requested PV.  If present we won't log it. */
static bool CheckBlacklist(struct asTrapWriteMessage *pmessage, int after)
{
    if (after)
        /* On second pass can just check for presence of userPvt field. */
        return pmessage->userPvt == NULL;
    else
    {
        /* On first pass check for PV in blacklist file. */
        dbAddr *dbaddr = pmessage->serverSpecific;
        const char *record_name = dbaddr->precord->name;
        return gphFind(blacklist, record_name, NULL) != NULL;
    }
}

/* Called both before and after each CA put. */
static void EpicsPvPutHook(struct asTrapWriteMessage *pmessage, int after)
{
    if (logging_enabled  &&  !CheckBlacklist(pmessage, after))
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
    else if (after  &&  pmessage->userPvt != NULL)
        /* Before bailing out ensure we handle any residual value.  This can
         * happen if the blacklist or logging enable state is updated while a CA
         * put is in progress */
        free(pmessage->userPvt);
}


void load_logging_blacklist(const char *blacklist_file)
{
    char line[1024];
    FILE *file = fopen(blacklist_file, "r");
    if (file == NULL)
    {
        printf("Unable to open blacklist file \"%s\"\n", blacklist_file);
        return;
    }

    printf("Loading PV log blacklist from %s\n", blacklist_file);
    printf("Ignoring:");
    while (fgets(line, sizeof(line), file))
    {
        int len = strlen(line);
        if (len > 1  &&  line[len - 1] == '\n')
        {
            char *space = strchr(line, ' ');
            line[len - 1] = '\0';
            /* Allow comments after first space in line. */
            if (space != NULL)
                *space = '\0';
            if (line[0] != '\0')
            {
                printf(" %s", line);
                gphAdd(blacklist, epicsStrDup(line), NULL);
            }
        }
    }
    fclose(file);
    printf("\n");
}

void set_logging_enable(bool enabled)
{
    logging_enabled = enabled;
}


static void Initialise(void)
{
    asTrapWriteRegisterListener(EpicsPvPutHook);
    gphInitPvt(&blacklist, 256);    // Finger in the air table size
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                       Registration and Shell Interface                    */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static const iocshArg blacklistArg0 = { "blacklist", iocshArgString };
static const iocshArg *const blacklistArgs[] = { &blacklistArg0 };
static iocshFuncDef load_logging_blacklist_def = {
    "load_logging_blacklist", 1, blacklistArgs
};

static const iocshArg enableArg0 = { "enabled", iocshArgInt };
static const iocshArg *const enableArgs[] = { &enableArg0 };
static iocshFuncDef set_logging_enable_def = {
    "set_logging_enable", 1, enableArgs
};

static void call_load_logging_blacklist(const iocshArgBuf *args)
{
    load_logging_blacklist(args[0].sval);
}

static void call_set_logging_enable(const iocshArgBuf *args)
{
    set_logging_enable(args[0].ival);
}


static void InstallPvPutHook(void)
{
    Initialise();
    iocshRegister(&load_logging_blacklist_def, call_load_logging_blacklist);
    iocshRegister(&set_logging_enable_def, call_set_logging_enable);
}

epicsExportRegistrar(InstallPvPutHook);
