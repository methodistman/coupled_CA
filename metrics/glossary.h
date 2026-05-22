#ifndef GLOSSARY_H
#define GLOSSARY_H

#include <stdint.h>

#define GLOSSARY_NAME_LEN    64
#define GLOSSARY_DESC_LEN    256
#define GLOSSARY_RULE_LEN    32
#define GLOSSARY_MAX_RULES   8

typedef struct {
    char name[GLOSSARY_NAME_LEN];
    char desc[GLOSSARY_DESC_LEN];
    int n_rules;
    char rule_names[GLOSSARY_MAX_RULES][GLOSSARY_RULE_LEN];
    int interval;
    char mode[GLOSSARY_RULE_LEN];
    double emergence_score;
    double mean_density;
    double mean_activity;
    double mean_entropy;
    int    sample_steps;
} GlossaryEntry;

typedef struct {
    GlossaryEntry *entries;
    int n_entries;
    int capacity;
} GlossaryDB;

GlossaryDB *glossary_create(int capacity_hint);
void glossary_destroy(GlossaryDB *db);

/* Add or update an entry. Returns index, or -1 on error. */
int glossary_add(GlossaryDB *db, const GlossaryEntry *entry);

/* Export database to a human-readable Markdown file. */
void glossary_export_md(const GlossaryDB *db, const char *filepath);

#endif
