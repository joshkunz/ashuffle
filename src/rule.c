#define _GNU_SOURCE

#include <mpd/client.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "list.h"
#include "rule.h"

struct rule_field {
    enum mpd_tag_type tag;
    char * value;
};

int rule_init(struct song_rule * rule, 
              enum rule_type type) {
    /* set the type */
    rule->type = type;

    /* allocate the field list */
    list_init(&rule->matchers);
    return 0;
}

int rule_add_criteria(struct song_rule * rule,
                      const char * field,
                      const char * expected_value) {
    struct rule_field matcher;
    /* try and parse out the tag to match on */
    matcher.tag = mpd_tag_name_iparse(field);
    if (matcher.tag == MPD_TAG_UNKNOWN) {
        return -1;
    }

    /* copy the string to match on */
    matcher.value = strdup(expected_value);
    /* add our matcher to the array */
    list_push(&rule->matchers, 
              node_from(&matcher, sizeof(struct rule_field)));
    return 0;
}

bool rule_match(struct song_rule * rule, 
                const struct mpd_song * song) {
    struct rule_field * current_matcher = NULL;
    const char * tag_value = NULL;
    for (unsigned i = 0; i < rule->matchers.length; i++) {
        current_matcher = list_at(&rule->matchers, i);
        /* get the first result for this tag */
        tag_value = mpd_song_get_tag(song, current_matcher->tag, 0);
        /* if the tag doesn't exist, we can't match on it. */
        if (tag_value == NULL) { continue; }
        /* if our match value is at least a substring of the tag's
         * value, we have a match. e.g. de matches 'De La Soul'.
         * If the output of strstr is NULL we don't have a substring
         * match. */
        if (strcasestr(tag_value, current_matcher->value) == NULL) {
            continue;
        }

        /* On exclusion matches, if any tag check succeeds, we have
         * a failed match. */
        if (rule->type == RULE_EXCLUDE) {
            return false;
        }
    }
    /* If we've passed all the tests, we have a match */
    return true;
}

int rule_free(struct song_rule * rule) {
    struct rule_field * field;
    for (unsigned i = 0; i < rule->matchers.length; i++) {
        field = list_at(&rule->matchers, i);
        free(field->value);
    }
    list_free(&rule->matchers);
    return 0;
}
