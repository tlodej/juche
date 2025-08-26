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
#include <unistd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

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
void* listGet(struct juche_list* list, size_t index);

struct juche_input {
        const char* path;
        bool include;
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
 * Adds dependency file. Program will not be compiled with it,
 * but modifying it will cause a rebuild.
 */
void stepDepend(struct juche_step* step, const char* path);

/*
 * Builds `req` along with `step`.
 */
void stepRequire(struct juche_step* step, struct juche_step* req);

/*
 * Looks for #include of local files in inputs and adds those as dependencies.
 */
void stepAutoDeps(struct juche_step* step);

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

void* listGet(struct juche_list* list, size_t index) {
        return (uint8_t*)list->items + (index * list->item_size);
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
        struct juche_input inp = {path, true};
        listPush(&step->inputs, &inp);
}

void stepDepend(struct juche_step* step, const char* path) {
        struct juche_input inp = {path, false};
        listPush(&step->inputs, &inp);
}

void stepRequire(struct juche_step* step, struct juche_step* req) {
        listPush(&step->deps, req);
}

static char* _parseInclude(const char* src, size_t start) {
        if (strncmp(src + start, "include", strlen("include")) != 0) {
                return NULL;
        }
        const char* name = src + start + strlen("include") + 1;

        if (name[0] != '"') {
                return NULL;
        }

        size_t length = 0;

        while ((name + length)[1] != '"') {
                length++;
        }

        char* path = malloc(length + 1);
        path[length] = 0;
        memcpy(path, name + 1, length);
        return path;
}

static void _findDeps(struct juche_step* step, const char* src, size_t len) {
        bool start_of_line = true;

        for (size_t i = 0; i < len; ++i) {
                char c = src[i];

                if (c == '\n') {
                        start_of_line = true;
                } else if (c == '#' && start_of_line) {
                        const char* path = _parseInclude(src, i + 1);

                        if (path != NULL) {
                                stepDepend(step, path);
                        }

                        start_of_line = false;
                } else {
                        start_of_line = false;
                }
        }
}

void stepAutoDeps(struct juche_step* step) {
        for (size_t i = 0; i < step->inputs.count;) {
                struct juche_input* input = listGet(&step->inputs, i++);

                FILE* in = fopen(input->path, "r");
                fseek(in, 0, SEEK_END);
                size_t len = ftell(in);
                fseek(in, 0, SEEK_SET);
                char* src = malloc(len);
                fread(src, 1, len, in);
                fclose(in);

                _findDeps(step, src, len - 1);

                free(src);
        }
}

static uint64_t _getTimestamp(const char *path) {
        struct stat attr;
        stat(path, &attr);

        return (uint64_t)attr.st_mtime;
}

static void _parseArg(struct juche_step* step, struct juche_list* list, const
                char* arg) {
        size_t len = strlen(arg);
        char* clone = malloc(len + 1);
        memcpy(clone, arg, len + 1);

        size_t counter = 0;

        for (size_t i = 0; i <= len; ++i) {
                char c = arg[i];

                if (c == T_IN[0]) {
                        for (size_t i = 0; i < step->inputs.count;) {
                                struct juche_input* input =
                                        listGet(&step->inputs, i++);
                                if (!input->include) continue;
                                listPush(list, &input->path);
                        }
                        counter = 0;
                } else if (c == T_OUT[0]) {
                        listPush(list, &step->output);
                        counter = 0;
                } else if (c == ' ') {
                        clone[i] = 0;

                        if (counter > 0) {
                                char* a = clone + (i - counter);
                                listPush(list, &a);
                        }

                        counter = 0;
                } else if (c == '\0') {
                        if (counter > 0) {
                                char* a = clone + (i - counter);
                                listPush(list, &a);
                        }
                } else {
                        counter++;
                }
        }
}

static void _buildReqs(struct juche_step* step) {
        for (size_t i = 0; i < step->deps.count; ++i) {
                struct juche_step** req = listGet(&step->deps, i);
                stepBuild(*req);
        }
}

static bool _shouldRebuild(struct juche_step* step) {
        uint64_t output_ts = _getTimestamp(step->output);
        
        for (size_t i = 0; i < step->inputs.count; ++i) {
                struct juche_input* inp = listGet(&step->inputs, i);
                uint64_t ts = _getTimestamp(inp->path);

                if (ts > output_ts) {
                        return true;
                }
        }

        return false;
}

void stepBuild(struct juche_step* step) {
        _buildReqs(step);
        
        if (!_shouldRebuild(step)) {
                return;
        }

        struct juche_list cmd_args;
        listInit(&cmd_args, sizeof(char*));

        listPush(&cmd_args, &step->command);

        for (size_t i = 0; i < step->args.count; ++i) {
                char** arg = listGet(&step->args, i);
                _parseArg(step, &cmd_args, *arg);
        }

        for (size_t i = 0; i < cmd_args.count; ++i) {
                char** text = listGet(&cmd_args, i);
                printf("%s ", *text);
        }
        printf("\n");

        pid_t pid = fork();
        if (pid == 0) {
                execvp(step->command, cmd_args.items);
                exit(1);
        } else if (pid > 0) {
                wait(NULL);
        }
}
