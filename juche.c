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

typedef struct {
        const char* path;
        bool is_fake;
} Input;

typedef struct {
        const char* command;
        const char* output;
        struct juche_list args;
        struct juche_list inputs;
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

Step* stepInit(const char* command, const char* output) {
        Step* step = malloc(sizeof(Step));
        step->command = command;
        step->output = output;
        listInit(&step->args, sizeof(char*));
        listInit(&step->inputs, sizeof(Input));
        return step;
}

void stepArg(Step* step, const char* argument) {
        listPush(&step->args, &argument);
}

void stepInput(Step* step, const char* path) {
        Input inp = {path, false};
        listPush(&step->inputs, &inp);
}

void stepFakeInput(Step* step, const char* path) {
        Input inp = {path, true};
        listPush(&step->inputs, &inp);
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
        for (size_t i = 0; i < step->inputs.count;) {
                Input input = ((Input*)step->inputs.items)[i++];
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
        
        for (size_t i = 0; i < step->inputs.count; ++i) {
                Input inp = ((Input*)step->inputs.items)[i];
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
