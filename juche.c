#define MAX_ARGS 64
#define MAX_INPUTS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define T_IN "\x1"
#define T_OUT "\x2"

typedef struct {
        const char* command;
        const char* output;
        const char* args[MAX_ARGS];
        size_t args_cursor;
        const char* inputs[MAX_INPUTS];
        size_t inputs_cursor;
} Step;

Step* stepInit(const char* command, const char* output);
void stepArg(Step* step, const char* argument);
void stepInput(Step* step, const char* input);
void stepDepend(Step* step, Step* dependency);
void stepBuild(Step* step);

Step* stepInit(const char* command, const char* output)
{
        Step* step = calloc(1, sizeof(Step));
        step->command = command;
        step->output = output;
        return step;
}

void stepArg(Step* step, const char* argument)
{
        step->args[step->args_cursor++] = argument;
}

void stepInput(Step* step, const char* input)
{
        step->inputs[step->inputs_cursor++] = input;
}

void stepDepend(Step* step, Step* dependency)
{
        (void)step;
        stepBuild(dependency);
}

uint64_t getTimestamp(const char *path) {
        struct stat attr;
        stat(path, &attr);
        return attr.st_mtime;
}

static void printInputs(Step* step)
{
        for (size_t i = 0; i < step->inputs_cursor;) {
                printf("%s", step->inputs[i++]);
                if (i < step->inputs_cursor) {
                        printf(" ");
                }
        }
}

static void parseArg(Step* step, const char* arg)
{
        size_t len = strlen(arg);
        for (size_t i = 0; i < len; ++i) {
                char c = arg[i];

                if (c == T_IN[0]) {
                        printInputs(step);
                } else if (c == T_OUT[0]) {
                        printf("%s", step->output); 
                } else {
                        printf("%c", c);
                }
        }
        printf(" ");
}

void stepBuild(Step* step)
{
        uint64_t output_ts = getTimestamp(step->output);
        bool should_rebuild = false;
        
        for (size_t i = 0; i < step->inputs_cursor; ++i) {
                uint64_t ts = getTimestamp(step->inputs[i]);
                should_rebuild = should_rebuild ? 1 : ts > output_ts;
        }

        if (!should_rebuild) return;

        printf("%s ", step->command);

        for (size_t i = 0; i < step->args_cursor; ++i) {
                parseArg(step, step->args[i]);
        }

        printf("\n");
}
