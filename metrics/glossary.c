#include "glossary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GlossaryDB *glossary_create(int capacity_hint) {
    int cap = ((capacity_hint + 7) / 8) * 8;
    if (cap < 8) cap = 8;
    GlossaryDB *db = calloc(1, sizeof(GlossaryDB));
    if (!db) return NULL;
    db->capacity = cap;
    db->entries = calloc(cap, sizeof(GlossaryEntry));
    if (!db->entries) { free(db); return NULL; }
    return db;
}

void glossary_destroy(GlossaryDB *db) {
    if (!db) return;
    free(db->entries);
    free(db);
}

int glossary_add(GlossaryDB *db, const GlossaryEntry *entry) {
    if (!db || !entry) return -1;
    if (db->n_entries >= db->capacity) {
        int new_cap = db->capacity * 2;
        GlossaryEntry *tmp = realloc(db->entries, new_cap * sizeof(GlossaryEntry));
        if (!tmp) return -1;
        db->entries = tmp;
        db->capacity = new_cap;
    }
    db->entries[db->n_entries] = *entry;
    return db->n_entries++;
}

void glossary_export_md(const GlossaryDB *db, const char *filepath) {
    if (!db || !filepath) return;
    FILE *f = fopen(filepath, "w");
    if (!f) return;

    fprintf(f, "# CA Rule Schedule Glossary\n\n");
    fprintf(f, "_Auto-generated from runtime discovery experiments._\n\n");
    fprintf(f, "| Total entries | %d |\n", db->n_entries);
    fprintf(f, "|---|---|\n\n");

    for (int i = 0; i < db->n_entries; i++) {
        const GlossaryEntry *e = &db->entries[i];
        fprintf(f, "## %s\n\n", e->name);
        fprintf(f, "| Field | Value |\n");
        fprintf(f, "|-------|-------|\n");
        fprintf(f, "| **Description** | %s |\n", e->desc);
        fprintf(f, "| **Mode** | %s |\n", e->mode);
        fprintf(f, "| **Interval** | %d |\n", e->interval);
        fprintf(f, "| **Rules** | ");
        for (int r = 0; r < e->n_rules; r++) {
            if (r) fprintf(f, ", ");
            fprintf(f, "%s", e->rule_names[r]);
        }
        fprintf(f, " |\n");
        fprintf(f, "| **Emergence Score** | %.4f |\n", e->emergence_score);
        fprintf(f, "| **Mean Density** | %.4f |\n", e->mean_density);
        fprintf(f, "| **Mean Activity** | %.4f |\n", e->mean_activity);
        fprintf(f, "| **Mean Entropy** | %.4f |\n", e->mean_entropy);
        fprintf(f, "| **Sample Steps** | %d |\n", e->sample_steps);
        fprintf(f, "\n");
    }
    fclose(f);
}
