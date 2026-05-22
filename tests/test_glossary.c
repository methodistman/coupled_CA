#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../metrics/glossary.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", msg); failures++; } \
    else fprintf(stderr, "  PASS: %s\n", msg); \
} while (0)

int main(void) {
    GlossaryDB *db = glossary_create(4);
    CHECK(db != NULL, "glossary_create returns non-NULL");
    CHECK(db->n_entries == 0, "initial n_entries is 0");
    CHECK(db->capacity >= 4, "capacity >= requested hint");

    /* Add a single entry */
    GlossaryEntry e1;
    memset(&e1, 0, sizeof(e1));
    snprintf(e1.name, GLOSSARY_NAME_LEN, "test_cycle");
    snprintf(e1.desc, GLOSSARY_DESC_LEN, "Test cycle schedule");
    snprintf(e1.mode, GLOSSARY_RULE_LEN, "cycle");
    e1.n_rules = 2;
    snprintf(e1.rule_names[0], GLOSSARY_RULE_LEN, "Conway's Life");
    snprintf(e1.rule_names[1], GLOSSARY_RULE_LEN, "HighLife");
    e1.interval = 5;
    e1.emergence_score = 0.75;
    e1.mean_density = 0.3;
    e1.mean_activity = 0.5;
    e1.mean_entropy = 0.8;
    e1.sample_steps = 200;

    int idx = glossary_add(db, &e1);
    CHECK(idx == 0, "first glossary_add returns index 0");
    CHECK(db->n_entries == 1, "n_entries incremented to 1");

    /* Verify stored entry */
    CHECK(strcmp(db->entries[0].name, "test_cycle") == 0, "stored name matches");
    CHECK(db->entries[0].n_rules == 2, "stored n_rules is 2");
    CHECK(db->entries[0].interval == 5, "stored interval is 5");
    CHECK(db->entries[0].emergence_score == 0.75, "stored emergence_score matches");

    /* Add entries beyond initial capacity to test resize */
    for (int i = 0; i < 10; i++) {
        GlossaryEntry e;
        memset(&e, 0, sizeof(e));
        snprintf(e.name, GLOSSARY_NAME_LEN, "entry_%d", i);
        snprintf(e.desc, GLOSSARY_DESC_LEN, "Bulk entry %d", i);
        snprintf(e.mode, GLOSSARY_RULE_LEN, "random");
        e.n_rules = 1;
        snprintf(e.rule_names[0], GLOSSARY_RULE_LEN, "Rule110");
        int ridx = glossary_add(db, &e);
        CHECK(ridx >= 0, "glossary_add during resize succeeds");
    }
    CHECK(db->n_entries == 11, "n_entries is 11 after bulk add");

    /* Export to Markdown and verify file exists */
    glossary_export_md(db, "/tmp/test_glossary_out.md");
    FILE *f = fopen("/tmp/test_glossary_out.md", "r");
    CHECK(f != NULL, "glossary_export_md creates file");
    if (f) {
        char line[512];
        int found_header = 0, found_entry = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "# CA Rule Schedule Glossary")) found_header = 1;
            if (strstr(line, "## test_cycle")) found_entry = 1;
        }
        fclose(f);
        CHECK(found_header, "exported file contains glossary header");
        CHECK(found_entry, "exported file contains test_cycle entry");
        remove("/tmp/test_glossary_out.md");
    }

    /* NULL safety */
    glossary_add(NULL, &e1);
    glossary_add(db, NULL);
    glossary_export_md(NULL, "/dev/null");
    glossary_export_md(db, NULL);
    glossary_destroy(NULL);

    glossary_destroy(db);
    CHECK(1, "glossary_destroy does not crash");

    fprintf(stderr, "%d errors\n", failures);
    return failures ? 1 : 0;
}
