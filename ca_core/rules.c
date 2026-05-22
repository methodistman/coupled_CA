#include "rules.h"
#include <string.h>

const CARule RULE_TABLE[] = {
    {"Conway's Life",      {0,0,0,1,0,0,0,0,0}, {0,0,1,1,0,0,0,0,0}, 0, 0},
    {"HighLife",           {0,0,0,1,0,0,1,0,0}, {0,0,1,1,0,0,0,0,0}, 0, 0},
    {"Day & Night",        {0,0,0,1,0,0,1,1,1}, {0,0,0,1,1,0,1,1,1}, 0, 0},
    {"Seeds",              {0,0,1,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0}, 0, 0},
    {"Diamoeba",           {0,0,0,1,0,1,1,1,1}, {0,0,0,0,0,1,1,1,0}, 0, 0},
    {"Anneal",             {0,0,0,0,1,0,1,1,1}, {0,0,0,1,1,0,1,0,0}, 0, 0},
    {"Life Without Death", {0,0,0,1,0,0,0,0,0}, {1,1,1,1,1,1,1,1,1}, 0, 0},
    {"Morley",             {0,0,0,1,0,0,1,0,0}, {0,0,1,0,1,1,0,0,0}, 0, 0},
    {"Rule 110",           {0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0}, 1, 110},
};

const int NUM_RULES = (int)(sizeof(RULE_TABLE) / sizeof(RULE_TABLE[0]));

const CARule *rules_lookup(const char *name) {
    for (int i = 0; i < NUM_RULES; i++)
        if (strcmp(RULE_TABLE[i].name, name) == 0)
            return &RULE_TABLE[i];
    return NULL;
}

int rules_index_by_name(const char *name) {
    for (int i = 0; i < NUM_RULES; i++)
        if (strcmp(RULE_TABLE[i].name, name) == 0)
            return i;
    return -1;
}
