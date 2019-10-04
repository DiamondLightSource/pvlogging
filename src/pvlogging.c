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
#include <limits.h>

#define db_accessHFORdb_accessC     // Needed to get correct DBF_ values
#include <dbAccess.h>
#include <dbFldTypes.h>
#include <iocsh.h>
#include <db_access.h>
#include <asTrapWrite.h>
#include <gpHash.h>
#include <epicsString.h>
#include <epicsExport.h>
#include <registryFunction.h>
#include <epicsVersion.h>

/* The interface to the caput event callback has changed as of EPICS 3.15, and
 * we need to compile as appropriate. */
#define BASE_3_15 (EPICS_VERSION * 100 + EPICS_REVISION >= 315)
#if BASE_3_15
#include <dbChannel.h>
#endif



/* This function can be called to load a list of PVs to blacklist. */
extern void load_logging_blacklist(const char *blacklist);
/* This function can be used to enable or disable logging. */
extern void set_logging_enable(bool enabled);
/* Used to set the maximum length of logged arrays. */
extern void set_max_array_length(int max_length);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                            IOC PV put logging                             */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool logging_enabled = true;
static struct gphPvt *blacklist;
static int max_array_length = INT_MAX;


/* Searches the blacklist for the requested PV.  If present we won't log it. */
static bool check_black_list(struct asTrapWriteMessage *pmessage, int after)
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


/* Alas dbGetField is rather rubbish at formatting floating point numbers, so we
 * do that ourselves, but the rest formats ok. */
static void format_field(dbAddr *dbaddr, dbr_string_t *value)
{
#define FORMAT(type, format) \
    do { \
        int i; \
        type *raw = (type *) dbaddr->pfield; \
        for (i = 0; i < length; i ++) \
            sprintf(value[i], format, raw[i]); \
    } while (0)

    long length = dbaddr->no_elements;
    switch (dbaddr->field_type)
    {
        case DBF_FLOAT:
            FORMAT(dbr_float_t, "%.7g");
            break;
        case DBF_DOUBLE:
            FORMAT(dbr_double_t, "%.15g");
            break;
        default:
            dbGetField(dbaddr, DBR_STRING, value, NULL, &length, NULL);
            break;
    }
#undef FORMAT
}

static void print_value(dbr_string_t *value, int length)
{
    if (length == 1)
        printf("%s", value[0]);
    else
    {
        int i = 0;
        printf("[");
        for (; i < length  &&  i < max_array_length; i ++)
        {
            if (i > 0)  printf(", ");
            printf("%s", value[i]);
        }
        if (length > max_array_length + 1)
            printf(", ...");
        if (length > max_array_length)
            printf(", %s", value[length-1]);
        printf("]");
    }
}


/* Called both before and after each CA put. */
static void epics_pv_put_hook(asTrapWriteMessage *pmessage, int after)
{
    if (logging_enabled  &&  !check_black_list(pmessage, after))
    {
#if BASE_3_15
        struct dbChannel *pchan = pmessage->serverSpecific;
        dbAddr *dbaddr = &pchan->addr;
#else
        dbAddr *dbaddr = pmessage->serverSpecific;
#endif
        int length = (int) dbaddr->no_elements;
        dbr_string_t *value = calloc((size_t) length, sizeof(dbr_string_t));
        format_field(dbaddr, value);

        if (after)
        {
            /* Log the message after the event. */
            dbr_string_t *old_value = (dbr_string_t *) pmessage->userPvt;
            printf("%s@%s %s.%s ",
                pmessage->userid, pmessage->hostid,
                dbaddr->precord->name, dbaddr->pfldDes->name);
            print_value(old_value, length);
            printf(" -> ");
            print_value(value, length);
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
        size_t len = strlen(line);
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

void set_max_array_length(int max_length)
{
    max_array_length = max_length;
}


static void initialise(void)
{
    asTrapWriteRegisterListener(epics_pv_put_hook);
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

static const iocshArg max_array_len_Arg0 = { "max_length", iocshArgInt };
static const iocshArg *const max_array_len_Args[] = { &max_array_len_Arg0 };
static iocshFuncDef set_max_array_length_def = {
    "set_max_array_length", 1, max_array_len_Args
};

static void call_load_logging_blacklist(const iocshArgBuf *args)
{
    load_logging_blacklist(args[0].sval);
}

static void call_set_logging_enable(const iocshArgBuf *args)
{
    set_logging_enable(args[0].ival);
}

static void call_set_max_array_length(const iocshArgBuf *args)
{
    set_max_array_length(args[0].ival);
}


static void InstallPvPutHook(void)
{
    initialise();
    iocshRegister(&load_logging_blacklist_def, call_load_logging_blacklist);
    iocshRegister(&set_logging_enable_def, call_set_logging_enable);
    iocshRegister(&set_max_array_length_def, call_set_max_array_length);
}

epicsExportRegistrar(InstallPvPutHook);
