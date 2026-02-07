#include "migi.h"
#include "string_builder.h"

void html_begin(StringBuilder *html) {
    sb_push(html,
        S("<!DOCTYPE html>\n"
          "<html lang=\"en\">\n"
          "    <head>\n"
          "       <meta charset=\"UTF-8\">\n"
          "       <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
          "       <title>MDML</title>\n"
          "       <link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/prismjs/themes/prism.css\">\n"
          "    </head>\n"
          "    <body>\n"
          "        <script src=\"https://cdn.jsdelivr.net/npm/prismjs/prism.js\"></script>\n"
          "        <script src=\"https://cdn.jsdelivr.net/npm/prismjs/components/prism-c.min.js\"></script>"));
}

void html_end(StringBuilder *html) {
    sb_push(html,
        S("    </body>\n"
          "</html>\n"));
}

Str escape_html(Arena *a, Str str) {
    Str escaped = {0};

    while (str.length > 0) {
        switch (str.data[0]) {
            case '&': {
                escaped = str_cat(a, escaped, S("&amp;"));
            } break;
            case '<': {
                escaped = str_cat(a, escaped, S("&lt;"));
            } break;
            case '\\': {
                str = str_skip(str, 1);
                if (str.length == 0) break;

                switch (str.data[0]) {
                    case '\\':
                    case '`':
                    case '*':
                    case '_':
                    case '[':
                    case ']':
                    case '(':
                    case ')':
                    case '>':
                    case '#':
                    case '+':
                    case '-':
                    case '.':
                    case '!':
                    case '|': {
                        escaped = str_cat(a, escaped, str_take(str, 1));
                    } break;
                    default: {
                        escaped = str_cat(a, escaped, S("\\"));
                        escaped = str_cat(a, escaped, str_take(str, 1));
                    } break;
                }
            } break;

            default: {
                escaped = str_cat(a, escaped, str_take(str, 1));
            } break;
        }
        str = str_skip(str, 1);
    }

    return escaped;
}

void html_push_text(StringBuilder *html, Str str);

Str parse_link(StringBuilder *html, Str str, bool image) {
    Str link = str;

    size_t link_text_end = str_find(link, S("]"));
    if (link_text_end == link.length) {
        migi_log(Log_Warning, "Unterminated '['");
        sb_push(html, str_take(str, link_text_end));
        return str_skip(link, link_text_end);
    }

    Str link_text = str_slice(link, 1, link_text_end);
    link = str_skip(link, link_text_end + 1);

    if (!str_starts_with(link, S("("))) {
        sb_push(html, str_take(str, link_text_end + 1));
        return link;
    }

    size_t link_url_end = str_find(link, S(")"));
    if (link_url_end == link.length) {
        migi_log(Log_Warning, "Unterminated '('");
        sb_push(html, str_take(str, link_text_end + 1));
        sb_push(html, str_take(link, link_url_end));
        return str_skip(link, link_url_end);
    }

    Str link_url = str_slice(link, 1, link_url_end);
    link = str_skip(link, link_url_end + 1);

    if (image) {
        sb_pushf(html, "<img src=\"%.*s\" alt=\"%.*s\">", SArg(link_url), SArg(link_text));
    } else {
        sb_pushf(html, "<a href=\"%.*s\">", SArg(link_url));
        html_push_text(html, link_text);
        sb_push(html, S("</a>"));
    }

    return link;
}

// parse inline elements into html
void html_push_text(StringBuilder *html, Str str) {
    bool parsing_strong = false;
    bool parsing_em     = false;

    while (str.length > 0) {
        if (str_starts_with(str, S("`"))) {
            sb_push(html, S("<code>"));
            str = str_skip(str, 1);

            int64_t code_end = str_find(str, S("`"));
            sb_push(html, str_take(str, code_end));

            str = str_skip(str, code_end + 1);
            sb_push(html, S("</code>"));

            continue;
        }

        // parse regular link
        if (str_starts_with(str, S("["))) {
            str = parse_link(html, str, false);
            continue;
        }

        // parse image link
        if (str_starts_with(str, S("!"))) {
            Str bang = str_take(str, 1);
            str = str_skip(str, 1);
            if (str_starts_with(str, S("["))) {
                str = parse_link(html, str, true);
            } else {
                sb_push(html, bang);
            }
            continue;
        }

        // In the case of parsing a closing `***`, the tag outputted first
        // (<strong>), instead closes the tag outputted second (<\em>)
        // to ensure proper nesting order.
        // This results in `<strong><em>[TEXT]</em></strong>`
        // and not `<strong><em>[TEXT]</strong></em` which is incorrect

        if (str_starts_with(str, S("**")) || str_starts_with(str, S("__"))) {
            if (parsing_strong && parsing_em) {
                sb_push(html, S("</em>"));
            } else if (parsing_strong) {
                sb_push(html, S("</strong>"));
                parsing_strong = false;
            } else {
                sb_push(html, S("<strong>"));
                parsing_strong = true;
            }
            str = str_skip(str, 2);
        }

        if (str_starts_with(str, S("*")) || str_starts_with(str, S("_"))) {
            if (parsing_em && parsing_strong) {
                sb_push(html, S("</strong>"));
                parsing_em = false;
                parsing_strong = false;
            } else if (parsing_em) {
                sb_push(html, S("</em>"));
                parsing_em = false;
            } else {
                sb_push(html, S("<em>"));
                parsing_em = true;
            }
            str = str_skip(str, 1);
        }


        int64_t markup_end = str_find_ex(str, S("*_`[!"), Find_AsChars);
        sb_push(html, str_take(str, markup_end));
        str = str_skip(str, markup_end);
    }

    if (parsing_strong) {
        migi_log(Log_Warning, "Unclosed bold delimiter");
    }
    if (parsing_em) {
        migi_log(Log_Warning, "Unclosed italics delimiter");
    }
}

#define HTML_INDENT 4

// TODO: maybe call escape_html in this function itself
void html_push_tag_text(StringBuilder *html, int indent_level, Str tag, Str text) {
    sb_pushf(html, "%*s", HTML_INDENT * indent_level, "");
    sb_pushf(html, "<%.*s>", SArg(tag));
    html_push_text(html, text);
    sb_pushf(html, "</%.*s>\n", SArg(tag));
}

void html_push_text_raw(StringBuilder *html, int indent_level, Str text) {
    sb_pushf(html, "%*s", HTML_INDENT * indent_level, "");
    sb_pushf(html, "%.*s", SArg(text));
}

typedef struct {
  Str class;
  bool closing;
  bool no_newline;
} PushTagOpt;

void html_push_tag_opt(StringBuilder *html, int indent_level, Str tag, PushTagOpt opt) {
    const char *newline_str = opt.no_newline? "": "\n";

    if (!opt.closing) {
        sb_pushf(html, "%*s", HTML_INDENT * indent_level, "");
        sb_pushf(html, "<%.*s class=\"%.*s\">%s", SArg(tag), SArg(opt.class), newline_str);
    } else {
        sb_pushf(html, "%s%*s", newline_str, HTML_INDENT * indent_level, "");
        sb_pushf(html, "</%.*s>%s", SArg(tag), newline_str);
    }
}

#define html_push_tag(html, indent_level, tag, ...) \
    html_push_tag_opt((html), (indent_level), (tag), (PushTagOpt){ __VA_ARGS__ })

bool line_is_ul(Str line) {
    return str_starts_with(line, S("- "))
        || str_starts_with(line, S("+ "))
        || str_starts_with(line, S("* "));
}

bool line_is_ol(Str line, int *digits_at_start) {
    int count = 0;
    Str tmp = line;
    while (tmp.length > 0 && between(tmp.data[0], '0', '9')) {
        count++;
        tmp = str_skip(tmp, 1);
    }
    *digits_at_start = count;
    return count > 0 && str_starts_with(str_skip(line, count), S(". "));
}

// TODO: add support for automatic compilation of the code blocks
// TODO: merge ul and ol parsing into a single function
// TODO: add blockquotes parsing
void html_render_md(StringBuilder *html, Str md) {
    Temp tmp = arena_temp();

    int ul_level         = 0;
    int ol_level         = 0;
    bool parsing_paragraph  = false;
    bool parsing_code_block = false;
    int64_t last_ul_indent = 0;
    int64_t last_ol_indent = 0;

    // <body> already is at level 1
    int html_indent = 2;

    strcut_foreach(md, S("\n"), 0, cut) {
        arena_rewind(tmp);

        bool parse_as_plain_text = false;
        Str line = cut.split;
        // TODO: this is a bit weird (trimming only `\r` to support windows new lines while the splitting is done by `\n`)
        line = str_skip_chars(line, S("\r"), SkipWhile_Reverse);

        // close ongoing elements when an empty line is encountered
        if (line.length == 0) {
            if (parsing_paragraph) {
                parsing_paragraph = false;
                html_push_tag(html, html_indent, S("p"), .closing=true);
            }
            if (ol_level > 0) {
                while (ol_level > 0) {
                    ol_level -= 1;
                    html_indent -= 1;
                    html_push_tag(html, html_indent, S("ol"), .closing=true);
                }
            }
            if (ul_level > 0) {
                while (ul_level > 0) {
                    ul_level -= 1;
                    html_indent -= 1;
                    html_push_tag(html, html_indent, S("ul"), .closing=true);
                }
            }
            continue;
        }

        if (parsing_code_block) {
            if (str_starts_with(line, S("```"))) {
                parsing_code_block = false;

                html_push_tag(html, html_indent + 1, S("code"), .closing=true, .no_newline=true);
                html_push_tag(html, html_indent, S("pre"), .closing=true);
            } else {
                line = escape_html(tmp.arena, line);
                html_push_text_raw(html, 0, line);
                sb_push(html, S("\n"));
            }
            continue;
        }

        if (ul_level > 0) {
            if (line_is_ul(line)) {
                line = str_skip(line, 2);
                line = escape_html(tmp.arena, str_trim_left(line));
                html_push_tag_text(html, html_indent, S("li"), line);
                continue;
            }

            Str trimmed = str_trim_left(line);
            if (line_is_ul(trimmed)) {
                int64_t indent_amount = line.length - trimmed.length;

                trimmed = str_skip(trimmed, 2);
                trimmed = escape_html(tmp.arena, trimmed);

                if (indent_amount > last_ul_indent) {
                    html_push_tag(html, html_indent, S("ul"));
                    ul_level += 1;
                    html_indent += 1;
                } else if (indent_amount < last_ul_indent) {
                    html_push_tag(html, html_indent, S("ul"), .closing=true);
                    ul_level -= 1;
                    html_indent -= 1;
                }
                last_ul_indent = indent_amount;

                html_push_tag_text(html, html_indent, S("li"), trimmed);
                continue;
            }

            ul_level -= 1;
            html_indent -= 1;
            html_push_tag(html, html_indent, S("ul"), .closing=true);
        }

        if (ol_level > 0) {
            int digits_at_start = 0;

            if (line_is_ol(line, &digits_at_start)) {
                line = str_skip(line, digits_at_start + 2);
                line = escape_html(tmp.arena, str_trim_left(line));
                html_push_tag_text(html, html_indent, S("li"), line);
                continue;
            }

            Str trimmed = str_trim_left(line);
            if (line_is_ol(trimmed, &digits_at_start)) {
                int64_t indent_amount = line.length - trimmed.length;

                trimmed = str_skip(trimmed, digits_at_start + 2);
                trimmed = escape_html(tmp.arena, trimmed);

                if (indent_amount > last_ol_indent) {
                    html_push_tag(html, html_indent, S("ol"));
                    ol_level += 1;
                    html_indent += 1;
                } else if (indent_amount < last_ol_indent) {
                    html_push_tag(html, html_indent, S("ol"), .closing=true);
                    ol_level -= 1;
                    html_indent -= 1;
                }
                last_ol_indent = indent_amount;

                html_push_tag_text(html, html_indent, S("li"), trimmed);
                continue;
            }

            ol_level -= 1;
            html_indent -= 1;
            html_push_tag(html, html_indent, S("ol"), .closing=true);
        }


        if (str_starts_with(line, S("#"))) {
            int header_count = 0;
            while (str_starts_with(line, S("#"))) {
                line = str_skip(line, 1);
                header_count++;
            }
            header_count = clamp_top(header_count, 6);

            // handle `#heading` as just regular text
            bool space_before_heading = str_starts_with(line, S(" "));

            if (space_before_heading) {
                line = escape_html(tmp.arena, str_trim_left(line));
                Str header_tag = stringf(tmp.arena, "h%d", header_count);
                html_push_tag_text(html, html_indent, header_tag, line);
            } else {
                parse_as_plain_text = true;
            }
        } else if (str_starts_with(line, S("```"))) {
            line = str_skip(line, 3);
            Str lang = str_take(line, 1);

            html_push_tag(html, html_indent, S("pre"));
            html_push_tag(html, html_indent + 1, S("code"), 
                         .class=stringf(tmp.arena, "language-%.*s", SArg(lang)));
            parsing_code_block = true;

        } else if (line_is_ul(line)) {
            line = str_skip(line, 2);
            line = escape_html(tmp.arena, str_trim_left(line));

            html_push_tag(html, html_indent, S("ul"));

            ul_level += 1;
            html_indent += 1;
            html_push_tag_text(html, html_indent, S("li"), line);

        } else if (between(line.data[0], '0', '9')) {
            int digits_at_start = 0;
            if (line_is_ol(line, &digits_at_start)) {
                line = str_skip(line, digits_at_start + 2);
                line = escape_html(tmp.arena, str_trim_left(line));

                html_push_tag(html, html_indent, S("ol"));

                ol_level += 1;
                html_indent += 1;

                html_push_tag_text(html, html_indent, S("li"), line);
            } else {
                parse_as_plain_text = true;
            }
        } else if (str_eq(line, S("---")) || str_eq(line, S("***")) || str_eq(line, S("___"))) {
            html_push_tag(html, html_indent, S("hr"));
        } else {
            parse_as_plain_text = true;
        }

        if (parse_as_plain_text) {
            line = escape_html(tmp.arena, str_trim_left(line));
            if (line.length > 0) {
                if (!parsing_paragraph) {
                    parsing_paragraph = true;
                    html_push_tag(html, html_indent, S("p"), .no_newline=true);
                } else {
                    // individual lines of markdown are separated with a single space
                    sb_push(html, S(" "));
                }
                html_push_text(html, line);

                // double space at end of line translates to a linebreak
                if (str_ends_with(line, S("  "))) {
                    html_push_tag(html, html_indent, S("br"), .no_newline=true);
                }
            }
        }
    }

    if (parsing_code_block) {
        migi_log(Log_Warning, "Unterminated '```'");
    }

    arena_temp_release(tmp);
}


int main() {
    Str input_file = S("test.md");
    Str output_file = S("index.html");

    Arena *a = arena_init();

    Str md = str_from_file(a, input_file);

    StringBuilder html = sb_init();
    html_begin(&html);
    html_render_md(&html, md);
    html_end(&html);
    if (!sb_to_file(&html, output_file)) return 1;

    printf("Generated: %.*s\n",SArg(output_file));
    return 0;
}
