/**
 * @file param_cmd.c
 * @brief NSH command: param — list, get, set, save, reset
 *
 * Usage
 * -----
 *   param list              — print all parameters with current value
 *   param get <name>        — print one parameter
 *   param set <name> <val>  — write one parameter (not persisted until save)
 *   param save              — flush scratch to flash
 *   param reset             — restore factory defaults and flush to flash
 *
 * Registration (in your board init or appconfig):
 *   NSH_BUILTIN_APPS: param_cmd_main, "param", ...
 *   or via NSHLIB_CUSTOMCMDS / builtin registration.
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "param.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static void print_value(const param_desc_t *desc, const param_value_t *val)
{
    switch (desc->type) {
    case PARAM_TYPE_INT32: printf("%12"PRId32, val->i); break;
    case PARAM_TYPE_FLOAT: printf("%.6g",    val->f); break;
    case PARAM_TYPE_BOOL:  printf("%12s",      val->b ? "true" : "false"); break;
    }
}

static const char *type_str(param_type_t t)
{
    switch (t) {
    case PARAM_TYPE_INT32: return "int32";
    case PARAM_TYPE_FLOAT: return "float";
    case PARAM_TYPE_BOOL:  return "bool";
    default:               return "?";
    }
}

/* Parse a string into a param_value_t according to the descriptor type.
 * Returns 0 on success, -1 on parse error. */
static int parse_value(const param_desc_t *desc, const char *str,
                       param_value_t *val_out)
{
    char *end;
    switch (desc->type) {
    case PARAM_TYPE_INT32:
        val_out->i = (int32_t)strtol(str, &end, 0);
        if (end == str) return -1;
        break;
    case PARAM_TYPE_FLOAT:
        printf("Parsing float from '%s' is currently not supported\n", str);
        break;
    case PARAM_TYPE_BOOL:
        if (strcmp(str, "1") == 0 || strcasecmp(str, "true") == 0)
            val_out->b = true;
        else if (strcmp(str, "0") == 0 || strcasecmp(str, "false") == 0)
            val_out->b = false;
        else
            return -1;
        break;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Subcommands
 * ------------------------------------------------------------------------- */

static int cmd_list(void)
{
    size_t count = param_count();
    if (count == 0) {
        printf("No parameters registered.\n");
        return 0;
    }
    printf("\n");

    /* Header */
    printf("%-24s %-6s  %12s  [%12s .. %12s]  %12s\n",
           "NAME", "TYPE", "VALUE", "MIN", "MAX", "default");
    printf("%-24s %-6s  %12s  [%12s .. %12s]  %12s\n",
           "------------------------", "------",
           "------------", "------------", "------------", "------------");

    void *scratch;
    if (param_store_lock(&scratch) != PARAM_STORE_OK) {
        fprintf(stderr, "param: failed to acquire store lock\n");
        return -EIO;
    }

    for (size_t i = 0; i < count; ++i) {
        const param_desc_t *desc = param_desc_by_index(i);
        param_value_t val;
        param_get_locked(scratch, desc, &val);

        printf("%-24s %-6s  ", desc->name, type_str(desc->type));
        print_value(desc, &val);

        /* min/max */
        if (desc->type != PARAM_TYPE_BOOL) {
            param_value_t mn, mx;
            if (desc->type == PARAM_TYPE_INT32) {
                mn.i = (int32_t)desc->min; mx.i = (int32_t)desc->max;
            } else {
                mn.f = (float)desc->min;   mx.f = (float)desc->max;
            }
            printf("  [");
            print_value(desc, &mn);
            printf(" .. ");
            print_value(desc, &mx);
            printf("]");
        } else {
            printf("  [false .. true]");
        }

        /* default */
        param_value_t dflt;
        if (desc->type == PARAM_TYPE_INT32)
            dflt.i = (int32_t)desc->dflt;
        else if (desc->type == PARAM_TYPE_FLOAT)
            dflt.f = (float)desc->dflt;
        else
            dflt.b = (bool)desc->dflt;

        printf("  ");
        print_value(desc, &dflt);
        printf("\n");
    }

    param_store_unlock();
    return 0;
}

static int cmd_get(const char *name)
{
    const param_desc_t *desc = param_desc_by_name(name);
    if (!desc) {
        fprintf(stderr, "param: '%s' not found\n", name);
        return -ENOENT;
    }

    param_value_t val;
    if (param_get(desc, &val) != PARAM_OK) {
        fprintf(stderr, "param: get failed\n");
        return -EIO;
    }

    printf("%s = ", desc->name);
    print_value(desc, &val);
    printf(" (%s)\n", type_str(desc->type));
    return 0;
}

static int cmd_set(const char *name, const char *valstr)
{
    const param_desc_t *desc = param_desc_by_name(name);
    if (!desc) {
        fprintf(stderr, "param: '%s' not found\n", name);
        return -ENOENT;
    }

    param_value_t val;
    if (parse_value(desc, valstr, &val) < 0) {
        fprintf(stderr, "param: cannot parse '%s' as %s\n",
                valstr, type_str(desc->type));
        return -EINVAL;
    }

    param_value_t actual;
    if (param_set(desc, val, &actual) != PARAM_OK) {
        fprintf(stderr, "param: set failed\n");
        return -EIO;
    }

    printf("%s = ", desc->name);
    print_value(desc, &actual);
    printf(" (not saved — run 'param save')\n");
    return 0;
}

static void usage(void)
{
    printf("Usage:\n"
           "  param list\n"
           "  param get <name>\n"
           "  param set <name> <value>\n"
           "  param save\n"
           "  param reset\n");
}

/* -------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return -EINVAL;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "list") == 0) {
        return cmd_list();

    } else if (strcmp(sub, "get") == 0) {
        if (argc < 3) { usage(); return -EINVAL; }
        return cmd_get(argv[2]);

    } else if (strcmp(sub, "set") == 0) {
        if (argc < 4) { usage(); return -EINVAL; }
        return cmd_set(argv[2], argv[3]);

    } else if (strcmp(sub, "save") == 0) {
        if (param_store_save() != PARAM_STORE_OK) {
            fprintf(stderr, "param: save failed\n");
            return -EIO;
        }
        printf("Parameters saved to flash.\n");
        return 0;

    } else if (strcmp(sub, "reset") == 0) {
        if (param_store_reset() != PARAM_STORE_OK) {
            fprintf(stderr, "param: reset failed\n");
            return -EIO;
        }
        printf("Parameters reset to factory defaults and saved.\n");
        return 0;

    } else {
        fprintf(stderr, "param: unknown subcommand '%s'\n", sub);
        usage();
        return -EINVAL;
    }
}
