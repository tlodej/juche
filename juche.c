/*
 * MIT License
 * 
 * Copyright (c) 2025 Tymoteusz Łodej
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define MAX_ARGS 64
#define MAX_INPUTS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// Will be replaced with output file in argument string
#define T_OUT "\x1"
// This one with space-separated input files
#define T_IN "\x2"

typedef struct {
        const char* path;
        bool is_fake;
} Input;

typedef struct {
        const char* command;
        const char* output;
        const char* args[MAX_ARGS];
        size_t args_cursor;
        Input inputs[MAX_INPUTS];
        size_t inputs_cursor;
} Step;

Step* stepInit(const char* command, const char* output);

void stepArg(Step* step, const char* argument);

void stepInput(Step* step, const char* path);

// Fake input file. Will not be included in build,
// but will cause a rebuild if modified.
void stepFakeInput(Step* step, const char* path);

void stepDepend(Step* step, Step* dependency);

// Prints finalized build command to stdout.
void stepBuild(Step* step);

/*
 * Implementation
 */

Step* stepInit(const char* command, const char* output) {
        Step* step = calloc(1, sizeof(Step));
        step->command = command;
        step->output = output;
        return step;
}

void stepArg(Step* step, const char* argument) {
        step->args[step->args_cursor++] = argument;
}

void stepInput(Step* step, const char* path) {
        step->inputs[step->inputs_cursor++] = (Input){path, false};
}

void stepFakeInput(Step* step, const char* path) {
        step->inputs[step->inputs_cursor++] = (Input){path, true};
}

void stepDepend(Step* step, Step* dependency) {
        (void)step;
        stepBuild(dependency);
}

static uint64_t getTimestamp(const char *path) {
        struct stat attr;
        stat(path, &attr);
        return attr.st_mtime;
}

static void printInputs(Step* step) {
        for (size_t i = 0; i < step->inputs_cursor;) {
                Input input = step->inputs[i++];
                if (input.is_fake) continue;
                printf("%s ", input.path);
        }
}

static void parseArg(Step* step, const char* arg) {
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

void stepBuild(Step* step) {
        uint64_t output_ts = getTimestamp(step->output);
        bool should_rebuild = false;
        
        for (size_t i = 0; i < step->inputs_cursor; ++i) {
                uint64_t ts = getTimestamp(step->inputs[i].path);
                should_rebuild = should_rebuild ? 1 : ts > output_ts;
        }

        if (!should_rebuild) return;

        printf("%s ", step->command);

        for (size_t i = 0; i < step->args_cursor; ++i) {
                parseArg(step, step->args[i]);
        }

        printf("\n");
}
