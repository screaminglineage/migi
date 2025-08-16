#include <stddef.h>
#include <stdio.h>
#include "../src/migi.h"
#include "../src/migi_string.h"
#include "../src/migi_lexer.h"
#include "../src/dynamic_array.h"

// TODO: generate special case for:
// - dynamic arrays (data, length, and capacity members) 
// - slices (data and length members)
// array_print() already exists and can be used for both of the above
// - unions [?] (undefined behaviour to access the wrong member)
// - tagged unions
// - enums

typedef struct {
    String name;
    bool is_non_primitive;
    union {
        const char *format;  // only valid when is_non_primitive is false
        String type_name;    // used if type is non-primitive
    };
} Member;

typedef struct {
    Member *data;
    size_t length;
    size_t capacity;
} Members;

typedef struct {
    String name;
    Members members;
} StructDef;


typedef struct {
    StructDef *data;
    size_t length;
    size_t capacity;
} StructDefs;


// TODO: parse qualifiers (static, const, etc.)
// TODO: parse pointers and array types
bool parse_member(Lexer *lexer, Member *member) {
    Token token = {0};
    if (!consume_token(lexer, &token)) return false;
    if (token.type != TOK_IDENTIFIER) return false;

    if (string_eq_any(token.string, migi_slice(StringSlice, (String[]){SV("int"), SV("signed"), SV("short"), SV("bool")}))) {
        member->format = "%d";
    } else if (string_eq(token.string, SV("char"))) {
        member->format = "%c";
    } else if (string_eq(token.string, SV("size_t"))) {
        member->format = "%zu";
    } else if (string_eq(token.string, SV("ptrdiff_t"))) {
        member->format = "%td";
    } else if (string_eq_any(token.string, migi_slice(StringSlice, (String[]){SV("float"), SV("double")}))) {
        member->format = "%.3f";
    } else if (string_eq(token.string, SV("long"))) {
        member->format = "%ld";
    } else if (string_eq(token.string, SV("unsigned"))) {
        member->format = "%u";
    } else if (string_eq(token.string, SV("const"))) {
        if (!expect_token_str(lexer, TOK_IDENTIFIER, SV("char"))) return false;
        if (!expect_token(lexer, TOK_STAR)) return false;
        member->format = "\\\"%s\\\"";
    } else {
        member->is_non_primitive = true;
        member->type_name = token.string;
    }

    if (!match_token(lexer, TOK_IDENTIFIER)) return false;
    member->name = next_token(lexer).string;
    return true;
}


bool parse_struct(Lexer *lexer, StructDef *struct_def) {
    Token tok = {0};
    while (peek_token(lexer, &tok)) {
        if (tok.type == TOK_CLOSE_BRACE) break;

        Member member = {0};
        if (parse_member(lexer, &member)) {
            array_add(&struct_def->members, member);
        } else {
            if (!consume_token(lexer, &tok)) return false;
        }
        if (!expect_token(lexer, TOK_SEMICOLON)) return false;
    }

    if (tok.type == TOK_EOF) {
        fprintf(stderr, "error: expected identifier, but got end of file at: %zu\n",
                lexer->end - tok.string.length + 1);
        return false;
    }

    // skipping closing brace
    consume_token(lexer, &tok);
    if (!match_token(lexer, TOK_IDENTIFIER)) return false;
    struct_def->name = next_token(lexer).string;

    if (!expect_token(lexer, TOK_SEMICOLON)) return false;
    return true;
}

void generate_string_printer(StringBuilder *sb) {
    // `level` is never used in `print_String,` but still present as a parameter
    // so that it can be called just like the other `_print_*` functions
    sb_push_cstr(sb, "static void _print_String(String var_name, int level) {\n");
    sb_push_cstr(sb, "    (void)level;\n");
    sb_push_cstr(sb, "    printf(\"\\\"%.*s\\\",\\n\", SV_FMT(var_name));\n");
    sb_push_cstr(sb, "}\n");
}

void generate_member_printer(StringBuilder *sb, Member member, int indent_count, int max_name_length) {
    // indent member sufficiently according to the level
    sb_pushf(sb, "    printf(\"%%*s\", (level + 1) * %d, \"\");\n", indent_count);

    // call the respective `_print_*` for non-primitive types
    if (member.is_non_primitive) {
        sb_pushf(sb, "    printf(\".%.*s = \");\n", SV_FMT(member.name));
        sb_pushf(sb, "    _print_%.*s(var_name.%.*s, level + 1);\n",
                SV_FMT(member.type_name), SV_FMT(member.name));

    // otherwise use the respective format string, while maintaining alignment
    } else {
        int padding = max_name_length - member.name.length + 1;
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

    for (size_t i = 0; i < members_length - 1; i++) {
        Member member = struct_def.members.data[i];
        generate_member_printer(sb, member, indent_count, max_name_length);
    }
    Member last_member = struct_def.members.data[members_length - 1];
    generate_member_printer(sb, last_member, indent_count, max_name_length);

    // also indent final closing brace
    sb_pushf(sb, "    printf(\"%%*s\", level*%d, \"\");\n", indent_count);
    sb_push_cstr(sb, "    printf(\"}\\n\");\n");
    sb_push_cstr(sb, "}\n");

    sb_pushf(sb, "static void print_%.*s(%.*s var_name) {\n",
            SV_FMT(struct_def.name), SV_FMT(struct_def.name));
    sb_pushf(sb, "    _print_%.*s(var_name, 0);\n", SV_FMT(struct_def.name));
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
    while (tok.type != TOK_EOF) {
        if (!consume_token(&lexer, &tok)) {
            return 1;
        }

        if (tok.type == TOK_IDENTIFIER) {
            if (!string_eq(SV("typedef"), tok.string)) continue;
            if (!expect_token_str(&lexer, TOK_IDENTIFIER, SV("struct"))) continue;
            if (!expect_token(&lexer, TOK_OPEN_BRACE)) continue;
            StructDef struct_def = {0};
            if (!parse_struct(&lexer, &struct_def)) continue;
            array_add(&structs, struct_def);
        }
    }

    StringBuilder writer = {0};
    StringBuilder filename = {0};

    generate_string_printer(&writer);
    sb_pushf(&filename, "%s/String_printer.gen.c", output_dir);
    write_file(&writer, sb_to_string(&filename));
    printf("Generated printer for `String`: `%.*s`\n", SV_FMT(sb_to_string(&filename)));
    sb_reset(&writer);
    sb_reset(&filename);

    array_foreach(&structs, StructDef, struct_def) {
        generate_struct_printer(&writer, *struct_def, DEFAULT_INDENT_LEVEL);
        sb_pushf(&filename, "%s/%.*s_printer.gen.c", output_dir, SV_FMT(struct_def->name));
        write_file(&writer, sb_to_string(&filename));
        printf("Generated printer for `%.*s`: `%.*s`\n",
                SV_FMT(struct_def->name), SV_FMT(sb_to_string(&filename)));

        sb_reset(&writer);
        sb_reset(&filename);
    }
}
