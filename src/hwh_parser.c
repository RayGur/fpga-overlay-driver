#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <expat.h>

#include "hwh_parser.h"
#include "../include/config.h"

/* ─────────────────────────────────────────────
 * Internal parser context
 * ───────────────────────────────────────────── */

typedef struct {
    ip_map_t *map;
    int       error;
} parse_ctx_t;

/* SAX start-element handler — only cares about <MEMRANGE> */
static void start_handler(void *user_data, const XML_Char *el,
                          const XML_Char **attr)
{
    if (strcmp(el, "MEMRANGE") != 0)
        return;

    parse_ctx_t *ctx = (parse_ctx_t *)user_data;
    ip_map_t    *map = ctx->map;

    const char *instance  = NULL;
    const char *basevalue = NULL;
    const char *highvalue = NULL;
    const char *memtype   = NULL;

    for (int i = 0; attr[i] != NULL; i += 2) {
        if      (strcmp(attr[i], "INSTANCE")  == 0) instance  = attr[i + 1];
        else if (strcmp(attr[i], "BASEVALUE") == 0) basevalue = attr[i + 1];
        else if (strcmp(attr[i], "HIGHVALUE") == 0) highvalue = attr[i + 1];
        else if (strcmp(attr[i], "MEMTYPE")   == 0) memtype   = attr[i + 1];
    }

    /* must have all four fields */
    if (!instance || !basevalue || !highvalue || !memtype)
        return;

    /* only PL IP register interfaces */
    if (strcmp(memtype, "REGISTER") != 0)
        return;

    /* skip if this instance is already recorded (multiple slave interfaces) */
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->cores[i].name, instance) == 0)
            return;
    }

    if (map->count >= MAX_IP_CORES) {
        fprintf(stderr, "[hwh_parser] too many IP cores (max %d)\n", MAX_IP_CORES);
        ctx->error = -1;
        return;
    }

    ip_core_t *core = &map->cores[map->count];
    strncpy(core->name, instance, IP_NAME_LEN - 1);
    core->name[IP_NAME_LEN - 1] = '\0';
    core->base_addr = (fpga_addr_t)strtoull(basevalue, NULL, 16);
    core->high_addr = (fpga_addr_t)strtoull(highvalue, NULL, 16);
    map->count++;
}

/* ─────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────── */

int parse_hwh(const char *hwh_path, ip_map_t *map)
{
    memset(map, 0, sizeof(*map));

    FILE *fp = fopen(hwh_path, "r");
    if (!fp) {
        fprintf(stderr, "[hwh_parser] cannot open %s: %s\n",
                hwh_path, strerror(errno));
        return -1;
    }

    XML_Parser parser = XML_ParserCreate(NULL);
    if (!parser) {
        fprintf(stderr, "[hwh_parser] failed to create expat parser\n");
        fclose(fp);
        return -1;
    }

    parse_ctx_t ctx = { .map = map, .error = 0 };
    XML_SetUserData(parser, &ctx);
    XML_SetElementHandler(parser, start_handler, NULL);

    char   buf[4096];
    int    done = 0;

    while (!done) {
        size_t len = fread(buf, 1, sizeof(buf), fp);
        done = feof(fp);

        if (XML_Parse(parser, buf, (int)len, done) == XML_STATUS_ERROR) {
            fprintf(stderr, "[hwh_parser] XML error at line %lu: %s\n",
                    XML_GetCurrentLineNumber(parser),
                    XML_ErrorString(XML_GetErrorCode(parser)));
            XML_ParserFree(parser);
            fclose(fp);
            return -1;
        }
    }

    XML_ParserFree(parser);
    fclose(fp);
    return ctx.error;
}

const ip_core_t *find_ip_by_name(const ip_map_t *map, const char *name)
{
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->cores[i].name, name) == 0)
            return &map->cores[i];
    }
    return NULL;
}

void print_ip_map(const ip_map_t *map)
{
    printf("IP Cores (%d):\n", map->count);
    for (int i = 0; i < map->count; i++) {
        printf("  %-32s base=" FPGA_ADDR_FMT "  high=" FPGA_ADDR_FMT "\n",
               map->cores[i].name,
               map->cores[i].base_addr,
               map->cores[i].high_addr);
    }
}
