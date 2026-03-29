#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tokens.h"

extern int yylex(void);
extern FILE *yyin;
/* Flex functions for scanning from memory */
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);

YYSTYPE yylval;

long get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <iterations>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = (char *)malloc(fsize + 1);
    fread(source, 1, fsize, f);
    source[fsize] = '\0';
    fclose(f);

    int iterations = atoi(argv[2]);
    long tokens_count = 0;

    long start_time = get_time_ms();

    for (int i = 0; i < iterations; i++) {
        YY_BUFFER_STATE buffer = yy_scan_string(source);
        int tok;
        while ((tok = yylex()) != 0) {
            tokens_count++;
        }
        yy_delete_buffer(buffer);
    }

    long end_time = get_time_ms();
    
    printf("Flex (C) Parsed %ld tokens in %d iterations.\n", tokens_count, iterations);
    printf("Time taken: %ld ms\n", (end_time - start_time));

    free(source);
    return 0;
}
