#include <mpd/client.h>
#include <stdbool.h>
#include "list.h"

#ifndef ASHUFFLE_RULE_H
#define ASHUFFLE_RULE_H

enum rule_type { RULE_EXCLUDE };

struct song_rule {
    enum rule_type type;
    struct list matchers;
};

/* Initialize a rule */
void rule_init(struct song_rule * rule, enum rule_type);

/* Add some criteria for this rule to match on */
int rule_add_criteria(struct song_rule * rule, 
                      const char * field,
                      const char * expected_value);

/* Returns true if this song is a positive match
 * in the case of 'include' rules and true for
 * negative matches in the case of 'exclude' rules. 
 * False otherwise. */
bool rule_match(struct song_rule *rule, 
                const struct mpd_song * song);

/* Free the memory used to store this rule */
void rule_free(struct song_rule * rule);

#endif
