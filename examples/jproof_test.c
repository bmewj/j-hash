#include "../lib/sha256.h"
#include "../src/jproof.h"
#include <string.h>
#include <stdio.h>

#define BUFFER_SIZE 4096

int main(int argc, const char** argv) {

    if (argc != 6) {
        printf(
            "J-PROOF C TEST UTILITY\n"
            "\n"
            "   ./jproof generate [file-in] [tree-in] [region-in-point] [region-out-point]\n"
            "   Generates a J-proof given a file, its merkle tree, and two byte offsets for the\n"
            "   subregion that needs a proof.\n"
        );
        return 0;
    }
    
    FILE* file_in;
    size_t file_size, range_in_point, range_out_point;
    {
        file_in = fopen(argv[2], "rb");
        if (file_in == NULL) {
            printf("Couldn't read file '%s'\n", argv[2]);
            return 1;
        }
        fseek(file_in, 0, SEEK_END);
        file_size = ftell(file_in);
        fseek(file_in, 0, SEEK_SET);
    }
    sscanf(argv[4], "%ld", &range_in_point);
    sscanf(argv[5], "%ld", &range_out_point);

    JPROOF_VALUE val;
    JHASH_VALUE correct;

    {
        JPROOF_GENERATE_CTX ctx;
        jproof_generate_init(&ctx, file_size, range_in_point, range_out_point);
        val = ctx.value;

        unsigned char* ptr = val.payload;

        fseek(file_in, ctx.request.head_in_point, SEEK_SET);
        jproof_generate_write_head_from_file(&ctx, file_in);

        fseek(file_in, ctx.request.tail_in_point, SEEK_SET);
        jproof_generate_write_tail_from_file(&ctx, file_in);

        FILE* tree_in = fopen(argv[3], "rb");
        if (tree_in == NULL) {
            printf("Couldn't read file '%s'\n", argv[2]);
            return 1;
        }

        jproof_generate_write_hashes_from_file(&ctx, tree_in);

        fseek(tree_in, -32, SEEK_END);
        int did = fread(correct.payload, 1, 32, tree_in);
        correct.length = file_size;
    }

    char* res_string = jproof_encode(&val);

    printf("JPROOF for file '%s' (%ld-%ld) = %s\n\n", argv[2], range_in_point, range_out_point, res_string);
    
    JPROOF_VALUE test_val;
    jproof_decode(res_string, &test_val);

    {
        JPROOF_VERIFY_CTX ctx;
        jproof_verify_init(&ctx, &val);

        fseek(file_in, range_in_point, SEEK_SET);

        unsigned char buffer[BUFFER_SIZE];
        size_t remaining = range_out_point - range_in_point;
        size_t length;
        while (remaining > 0) {
            size_t length = fread(buffer, 1, remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE, file_in);
            if (length == 0) break;
            remaining -= length;
            jproof_verify_update(&ctx, buffer, length);
        }

        JHASH_VALUE result;
        int err = jproof_verify_final(&ctx, &result);
        if (err) {
            printf("JPROOF verification failed\n\n");
        } else if (memcmp(correct.payload, result.payload, 32) == 0) {
            printf("JPROOF verification tested, checks out! %s\n\n", jhash_encode(&result));
        } else {
            printf("JPROOF verification tested, DOESN'T match:\n");
            printf("  result  = %s\n", jhash_encode(&result));
            printf("  correct = %s\n\n", jhash_encode(&correct));
        }

    }

    fclose(file_in);

    return 0;
}
