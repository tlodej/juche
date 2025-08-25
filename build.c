#include "juche.c"

#define BUILD "build/"
#define CC "gcc"
#define FLAGS "-Wall -Wextra -Werror -pedantic-errors -std=c99 -ggdb"

int main(void) {
        struct juche_step script;
        stepInit(&script, CC, BUILD"juche");
        stepArg(&script, T_IN);
        stepArg(&script, "-o "T_OUT);
        stepArg(&script, FLAGS);
        stepInput(&script, "build.c");
        stepAutoDeps(&script);
        stepBuild(&script);
}
