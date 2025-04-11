#include <stdio.h>
#include <string.h>
#include "json_c.c"

typedef struct {
    char name[64];
    char return_type[64];
    int param_count;
    char param_types[10][64];
    char param_names[10][64];
    int if_count;
} function_info;

void extract_type_string(json_value type_node, char* out_type) {
    int pointer_depth = 0;
    json_value current = type_node;

    while (json_get_type(current) == JSON_OBJECT) {
        char* nodetype = json_get_string(json_get(current, "_nodetype"));
        if (!nodetype || strcmp(nodetype, "PtrDecl") != 0) break;
        pointer_depth++;
        current = json_get(current, "type");
    }

    json_value names = json_get(current, "type", "names");
    if (json_get_type(names) == JSON_ARRAY && json_len(names) > 0) {
        strncpy(out_type, "", 64);
        strncat(out_type, json_get_string(json_get(names, 0)), 64);
        for (int i = 0; i < pointer_depth; i++) strncat(out_type, "*", 64);
    } else {
        strncpy(out_type, "(unknown)", 64);
    }
}

int count_if_statements(json_value body) {
    if (json_get_type(body) != JSON_OBJECT) return 0;

    int count = 0;
    char* nodetype = json_get_string(json_get(body, "_nodetype"));
    if (!nodetype) return 0;

    if (strcmp(nodetype, "If") == 0) count++;

    if (strcmp(nodetype, "Compound") == 0) {
        json_value items = json_get(body, "block_items");
        if (json_get_type(items) == JSON_ARRAY) {
            for (int i = 0; i < json_len(items); i++) {
                count += count_if_statements(json_get(items, i));
            }
        }
    } else {
        for (int i = 0; i <= json_get_last_index(body); i++) {
            json_value child = json_get_from_object((json_object*)body.value, ((json_object*)body.value)->keys[i]);
            if (json_get_type(child) == JSON_OBJECT || json_get_type(child) == JSON_ARRAY)
                count += count_if_statements(child);
        }
    }

    return count;
}

int main() {
    json_value root = json_read("ast.json");
    if (json_get_type(root) != JSON_OBJECT) {
        fprintf(stderr, "JSON 최상단이 객체가 아님\n");
        return 1;
    }

    json_value ext = json_get(root, "ext");
    if (json_get_type(ext) != JSON_ARRAY) {
        fprintf(stderr, "'ext'가 배열이 아님\n");
        return 1;
    }

    int func_count = 0;
    function_info funcs[100] = {0};

    for (int i = 0; i < json_len(ext); i++) {
        json_value node = json_get(ext, i);
        char* nodetype = json_get_string(json_get(node, "_nodetype"));

        if (strcmp(nodetype, "Decl") == 0 || strcmp(nodetype, "FuncDef") == 0) {
            json_value decl = node;
            if (strcmp(nodetype, "FuncDef") == 0)
                decl = json_get(node, "decl");

            json_value type = json_get(decl, "type");
            char* typenodetype = json_get_string(json_get(type, "_nodetype"));
            if (!typenodetype || strcmp(typenodetype, "FuncDecl") != 0) continue;

            function_info *f = &funcs[func_count];
            memset(f, 0, sizeof(function_info));

            char* fname = json_get_string(json_get(decl, "name"));
            if (fname) strncpy(f->name, fname, sizeof(f->name));

            // 리턴 타입 추출
            json_value return_type_node = json_get(type, "type");
            extract_type_string(return_type_node, f->return_type);

            // 파라미터 처리
            json_value params = json_get(type, "args", "params");
            if (json_get_type(params) == JSON_ARRAY) {
                int plen = json_len(params);
                f->param_count = plen;
                for (int j = 0; j < plen; j++) {
                    json_value param = json_get(params, j);
                    json_value param_type_node = json_get(param, "type");
                    extract_type_string(param_type_node, f->param_types[j]);

                    json_value pname = json_get(param, "name");
                    if (json_get_type(pname) == JSON_STRING)
                        strncpy(f->param_names[j], json_get_string(pname), 32);
                    else
                        strncpy(f->param_names[j], "(unnamed)", 32);
                }
            }

            if (strcmp(nodetype, "FuncDef") == 0) {
                json_value body = json_get(node, "body");
                f->if_count = count_if_statements(body);
            }

            func_count++;
        }
    }

    printf("총 함수 수: %d\n\n", func_count);
    for (int i = 0; i < func_count; i++) {
        function_info *f = &funcs[i];
        printf("함수명: %s\n", f->name);
        printf("리턴타입: %s\n", f->return_type);
        printf("파라미터 (%d개): ", f->param_count);
        for (int j = 0; j < f->param_count; j++) {
            printf("%s %s", f->param_types[j], f->param_names[j]);
            if (j != f->param_count - 1) printf(", ");
        }
        printf("\nif문 개수: %d\n", f->if_count);
        printf("\n");
    }

    json_free(root);
    return 0;
}