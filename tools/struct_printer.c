#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "../src/migi.h"
#include "../src/migi_string.h"
#include "../src/migi_lexer.h"
#include "../src/dynamic_array.h"
#include "../src/migi_temp.h"

// TODO: generate special case for:
// - unions [?] (undefined behaviour to access the wrong member)
// - tagged unions
// - enums
// - dereference pointers [?] (should be fine to only do 1 level at most)


typedef struct {
    String name;
    String type_name;
    bool is_non_primitive;
    const char *format;
} Member;

typedef struct {
    Member *data;
    size_t length;
    size_t capacity;
} Members;

typedef struct {
    String name;
    Members members;
    bool has_data_and_length;
} StructDef;


typedef struct {
    StructDef *data;
    size_t length;
    size_t capacity;
} StructDefs;


bool format_for_type(String type, const char **format, bool *is_char) {
    if (string_eq_any(type, (String[]){SV("int"), SV("byte"), SV("bool"), SV("short"), SV("signed") })) {
        *format = "%d";
    } else if (string_eq(type, SV("char"))) {
        is_char? *is_char = true: (void)0;
        *format = "\'%c\'";
    } else if (string_eq(type, SV("size_t"))) {
        *format = "%zu";
    } else if (string_eq_any(type, (String[]){SV("float"), SV("double")})) {
        *format = "%.3f";
    } else if (string_eq(type, SV("ptrdiff_t"))) {
        *format = "%td";
    } else if (string_eq(type, SV("void"))) {
        *format = "%p";
    } else if (string_eq(type, SV("long"))) {
        *format = "%ld";
    } else if (string_eq(type, SV("unsigned"))) {
        *format = "%u";
    } else {
        return false;
    }
    return true;
}


// TODO: parse static arrays and flexible array members
bool parse_member(Lexer *lexer, Member *member) {
    if (!match_token(lexer, Tok_Identifier)) return false;
    Token token = next_token(lexer);

    bool is_const = false;
    if (string_eq(token.string, SV("const"))) {
        is_const = true;
        if (!match_token(lexer, Tok_Identifier)) return false;
        if (!consume_token(lexer, &token)) return false;
    }

    bool is_char = false;
    if (!format_for_type(token.string, &member->format, &is_char)) {
        member->is_non_primitive = true;
    }
    member->type_name = token.string;

    if (match_token(lexer, Tok_Star)) {
        next_token(lexer);
        if (is_const && is_char) {
            member->format = "\\\"%s\\\"";
        } else {
            if (!match_token_str(lexer, Tok_Identifier, SV("data"))) {
                member->is_non_primitive = false;
                member->format = "%p";
            }
        }
    }

    if (!match_token(lexer, Tok_Identifier)) return false;
    member->name = next_token(lexer).string;
    return true;
}

// TODO: parse comma separated members
bool parse_struct_members(Lexer *lexer, StructDef *struct_def) {
    bool has_field_data     = false;
    bool has_field_length   = false;

    Token tok = {0};
    while (peek_token(lexer, &tok)) {
        if (tok.type == Tok_CloseBrace) break;

        Member member = {0};
        if (!parse_member(lexer, &member)) return false;
        if (string_eq(member.name, SV("data"))) {
            has_field_data = true;
        } else if (string_eq(member.name, SV("length"))) {
            has_field_length = true;
        } 
        array_add(&struct_def->members, member);
        if (!expect_token(lexer, Tok_Semicolon)) return false;
    }
    struct_def->has_data_and_length = has_field_data && has_field_length;

    if (tok.type == Tok_Eof) {
        fprintf(stderr, "error: expected identifier, but got end of file at: %zu\n",
                lexer->end - tok.string.length + 1);
        return false;
    }

    // skipping closing brace
    consume_token(lexer, &tok);
    return true;
}

void generate_string_printer(StringBuilder *sb) {
    // `level` is never used in `print_String,` but still present as a parameter
    // so that it can be called just like the other `_print_*` functions
    sb_push_cstr(sb, "static void _print_String(String var_name, int level) {\n");
    sb_push_cstr(sb, "    (void)level;\n");
    sb_push_cstr(sb, "    printf(\"\\\"%.*s\\\"\", SV_FMT(var_name));\n");
    sb_push_cstr(sb, "}\n");
}


void generate_member_printer(StringBuilder *sb, Member member, int indent_count, int max_name_length, bool is_slice) {
    // indent member sufficiently according to the level
    sb_pushf(sb, "    printf(\"%%*s\", (level + 1) * %d, \"\");\n", indent_count);

    // padding to align the `=` characters by the member with the longest name
    int padding = max_name_length - member.name.length + 1;


    // print as slice
    if (is_slice) {
        sb_pushf     (sb, "    printf(\".%.*s%*s= \");\n", SV_FMT(member.name), padding, " ");
        sb_pushf     (sb, "    printf(\"(%.*s[]){ \");\n", SV_FMT(member.type_name));
        sb_push_cstr (sb, "    for (size_t i = 0; i < var_name.length; i++) {\n");

        if (member.is_non_primitive) {
            sb_pushf(sb,     "        _print_%.*s(var_name.data[i], level + 1);\n", SV_FMT(member.type_name));
            sb_push_cstr(sb, "        printf(\", \");\n");
        } else {
            const char *format_str = NULL;
            format_for_type(member.type_name, &format_str, NULL);
            sb_pushf     (sb, "        printf(\"%s, \", var_name.data[i]);\n", format_str);
        }
        sb_push_cstr (sb, "    }\n");
        sb_push_cstr (sb, "    printf(\"},\\n\");\n");


    // call the respective `_print_*` for non-primitive types
    } else if (member.is_non_primitive) {
        sb_pushf(sb, "    printf(\".%.*s%*s= \");\n", SV_FMT(member.name), padding, " ");
        sb_pushf(sb, "    _print_%.*s(var_name.%.*s, level + 1);\n",
                SV_FMT(member.type_name), SV_FMT(member.name));
        sb_push_cstr(sb, "        printf(\"\\n\");\n");

    // otherwise use the respective format string
    } else {
        sb_pushf(sb, "    printf(\".%.*s%*s= %s,\\n\", var_name.%.*s);\n",
                SV_FMT(member.name), padding, " ", member.format, SV_FMT(member.name));
    }
}


void generate_struct_printer(StringBuilder *sb, StructDef struct_def, int indent_count) {
    // generate print function with indentation level
    sb_pushf(sb, "static void _print_%.*s(%.*s var_name, int level) {\n",
            SV_FMT(struct_def.name), SV_FMT(struct_def.name));

    // indent member sufficiently according to the level
    sb_pushf(sb, "    printf(\"(%%s){\\n\", \"%.*s\");\n", SV_FMT(struct_def.name));

    size_t members_length = struct_def.members.length;
    size_t max_name_length = 0;
    for (size_t i = 0; i < members_length; i++) {
        max_name_length = max(max_name_length, struct_def.members.data[i].name.length);
    }

    // TODO: save the index of data and length so that this loop is not required here
    size_t member_data_i = 0;
    if (struct_def.has_data_and_length) {
        for (size_t i = 0; i < members_length; i++) {
            if (string_eq(struct_def.members.data[i].name, SV("data"))) {
                member_data_i = i;
            }
        }
    }

    for (size_t i = 0; i < members_length; i++) {
        Member member = struct_def.members.data[i];
        generate_member_printer(sb, member, indent_count, max_name_length,
                struct_def.has_data_and_length && i == member_data_i);
    }

    // also indent final closing brace
    sb_pushf(sb, "    printf(\"%%*s\", level*%d, \"\");\n", indent_count);
    sb_push_cstr(sb, "    printf(\"}\");\n");
    sb_push_cstr(sb, "}\n");

    sb_pushf(sb, "static void print_%.*s(%.*s var_name) {\n",
            SV_FMT(struct_def.name), SV_FMT(struct_def.name));
    sb_pushf    (sb, "    _print_%.*s(var_name, 0);\n", SV_FMT(struct_def.name));
    sb_push_cstr(sb, "    printf(\"\\n\");\n");
    sb_push_cstr(sb, "}\n");
}

#define DEFAULT_INDENT_LEVEL 2
#define DEFAULT_OUTPUT_DIR "./gen"

int main(int argc, char *argv[]) {
    shift_args(argc, argv);
    if (argc == 0) {
        fprintf(stderr, "error: no filename provided\n");
        return 1;
    }
    String input_file = string_from_cstr(shift_args(argc, argv));

    // TODO: check if the folder exists, and if not create it
    const char *output_dir = DEFAULT_OUTPUT_DIR;
    if (argc != 0) {
        output_dir = shift_args(argc, argv);
    }

    StringBuilder reader = {0};
    read_file(&reader, input_file);
    String file_data = sb_to_string(&reader);

    Lexer lexer = {.string = file_data};
    Token tok = {0};
    StructDefs structs = {0};
    while (peek_token(&lexer, &tok)) {
        bool skip = true;
        if (match_token_str(&lexer, Tok_Identifier, SV("typedef"))) {
            next_token(&lexer);
            if (!expect_token_str(&lexer, Tok_Identifier, SV("struct"))) continue;
            skip = false;
        } else if (match_token_str(&lexer, Tok_Identifier, SV("struct"))) {
            next_token(&lexer);
            skip = false;
        } else {
            next_token(&lexer);
        }
        if (!skip) {
            StructDef struct_def = {0};
            if (match_token(&lexer, Tok_Identifier)) {
                struct_def.name = next_token(&lexer).string;
            }

            if (!expect_token(&lexer, Tok_OpenBrace)) continue;
            if (!parse_struct_members(&lexer, &struct_def)) continue;

            // prefer the name at the end for typedef struct declarations
            if (match_token(&lexer, Tok_Identifier)) {
                struct_def.name = next_token(&lexer).string;
            }
            if (struct_def.name.length == 0) continue;
            if (!expect_token(&lexer, Tok_Semicolon)) continue;
            array_add(&structs, struct_def);
        }
    }

    StringBuilder writer = {0};

    generate_string_printer(&writer);
    String filename_string = temp_format("%s/String_printer.gen.c", output_dir);
    write_file(&writer, filename_string);
    printf("Generated printer for `String`: `%.*s`\n", SV_FMT(filename_string));
    sb_reset(&writer);

    array_foreach(&structs, StructDef, struct_def) {
        generate_struct_printer(&writer, *struct_def, DEFAULT_INDENT_LEVEL);
        String filename_struct = temp_format("%s/%.*s_printer.gen.c", output_dir, SV_FMT(struct_def->name));
        write_file(&writer, filename_struct);
        printf("Generated printer for `%.*s`: `%.*s`\n",
                SV_FMT(struct_def->name), SV_FMT(filename_struct));

        sb_reset(&writer);
    }
}
