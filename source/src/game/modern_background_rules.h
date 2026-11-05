#ifndef MODERN_BACKGROUND_RULES_H
#define MODERN_BACKGROUND_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BACKGROUND_RULE_TEXT 2048
#define BACKGROUND_RULE_IDS 16

typedef struct BackgroundRules {
    char text[BACKGROUND_RULE_TEXT];
    int ids[BACKGROUND_RULE_IDS];
    int count;
    bool inherit;
} BackgroundRules;

/* Native effect syntax; no scripts or file paths are accepted. */
bool background_rules_validate(const char* text, char* error, size_t size);
int background_rules_adjustment(const char* text, const char* key);
bool background_rules_step(char* text, size_t size, const char* key, int delta);
bool background_rules_prepare(BackgroundRules* rules, char* error, size_t size);
bool background_rules_available(const BackgroundRules* rules);
void background_rules_apply(int64_t obj, int base_effect, const BackgroundRules* rules);
void background_rules_remove(int64_t obj, const BackgroundRules* rules);
bool background_rules_preview_begin(int64_t obj, int base_effect, const BackgroundRules* old_rules,
    const char* text, bool inherit);
void background_rules_preview_end(int64_t obj);
bool background_rules_describe(int effect, char* text, size_t size);
bool background_rules_saved_objects_valid(void);

#endif
