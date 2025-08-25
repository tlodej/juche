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

struct juche_list {
        void* items;
        size_t count;
        size_t capacity;
        size_t item_size;
};

void listInit(struct juche_list* list, size_t item_size);
void listFree(struct juche_list* list);
void listPush(struct juche_list* list, void* item);

struct juche_input {
        const char* path;
        bool is_fake;
};

struct juche_step {
        const char* command;
        const char* output;
        struct juche_list args;
        struct juche_list deps;
        struct juche_list inputs;
};

void stepInit(struct juche_step* step, const char* cmd, const char* output);

/*
 * Adds any number of arguments in any format.
 */
void stepArg(struct juche_step* step, const char* argument);

/*
 * Adds input file.
 */
void stepInput(struct juche_step* step, const char* path);

/*
 * Adds fake input file. Program will not be compiled with it,
 * but modifying it will cause a rebuild.
 */
void stepFakeInput(struct juche_step* step, const char* path);

/*
 * Compiles `dependency` when compiling step.
 */
void stepDepend(struct juche_step* step, struct juche_step* dependency);

/*
 * Prints build command to stdout.
 */
void stepBuild(struct juche_step* step);

void listInit(struct juche_list* list, size_t item_size) {
        memset(list, 0, sizeof(*list));
        list->item_size = item_size;
}

void listFree(struct juche_list* list) {
        free(list->items);
        memset(list, 0, sizeof(*list));
}

void listPush(struct juche_list* list, void* item) {
        if (list->count >= list->capacity) {
                list->capacity *= 2;

                if (list->capacity == 0) {
                        list->capacity = 32;
                }

                size_t new_len = list->capacity * list->item_size;
                list->items = realloc(list->items, new_len);
        }

        uint8_t* items = list->items;
        size_t offset = list->count++ * list->item_size;
        memcpy(items + offset, item, list->item_size);
}

void stepInit(struct juche_step* step, const char* cmd, const char* output) {
        step->command = cmd;
        step->output = output;
        listInit(&step->args, sizeof(char*));
        listInit(&step->deps, sizeof(struct juche_step*));
        listInit(&step->inputs, sizeof(struct juche_input));
}

void stepArg(struct juche_step* step, const char* argument) {
        listPush(&step->args, &argument);
}

void stepInput(struct juche_step* step, const char* path) {
        struct juche_input inp = {path, false};
        listPush(&step->inputs, &inp);
}

void stepFakeInput(struct juche_step* step, const char* path) {
        struct juche_input inp = {path, true};
        listPush(&step->inputs, &inp);
}

void stepDepend(struct juche_step* step, struct juche_step* dependency) {
        listPush(&step->deps, dependency);
}

static uint64_t getTimestamp(const char *path) {
        struct stat attr;
        stat(path, &attr);
        return attr.st_mtime;
}

static void printInputs(struct juche_step* step) {
        for (size_t i = 0; i < step->inputs.count;) {
                struct juche_input input = ((struct juche_input*)step->inputs.items)[i++];
                if (input.is_fake) continue;
                printf("%s ", input.path);
        }
}

static void parseArg(struct juche_step* step, const char* arg) {
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

void stepBuild(struct juche_step* step) {
        for (size_t i = 0; i < step->deps.count; ++i) {
                struct juche_step* step = ((struct juche_step**)step->deps.items)[i];
                stepBuild(step);
        }
        uint64_t output_ts = getTimestamp(step->output);
        bool should_rebuild = false;
        
        for (size_t i = 0; i < step->inputs.count; ++i) {
                struct juche_input inp = ((struct juche_input*)step->inputs.items)[i];
                uint64_t ts = getTimestamp(inp.path);
                should_rebuild = should_rebuild ? 1 : ts > output_ts;
        }

        if (!should_rebuild) return;

        printf("%s ", step->command);

        for (size_t i = 0; i < step->args.count; ++i) {
                const char* arg = ((char**)step->args.items)[i];
                parseArg(step, arg);
        }

        printf("\n");
}
