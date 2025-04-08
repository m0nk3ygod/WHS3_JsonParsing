#include <stdio.h>
#include <string.h>
#include "json_c.c"  

typedef struct {
    char name[64];
    char return_type[32];
    int param_count;
    char param_types[10][32];
    char param_names[10][32];
} function_info;

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
            if (strcmp(typenodetype, "FuncDecl") != 0) continue;

            function_info *f = &funcs[func_count];

            
            char* fname = json_get_string(json_get(decl, "name"));
            if (fname)
                strncpy(f->name, fname, sizeof(f->name));

            
            json_value return_type = json_get(type, "type", "type", "names", 0);
            if (json_get_type(return_type) == JSON_STRING) {
                strncpy(f->return_type, json_get_string(return_type), sizeof(f->return_type));
            }

            
            json_value params = json_get(type, "args", "params");
            if (json_get_type(params) == JSON_ARRAY) {
                int plen = json_len(params);
                f->param_count = plen;
                for (int j = 0; j < plen; j++) {
                    json_value param = json_get(params, j);
                    json_value typenames = json_get(param, "type", "type", "names");
                    json_value pname = json_get(param, "name");

                    if (json_get_type(typenames) == JSON_ARRAY && json_len(typenames) > 0)
                        strncpy(f->param_types[j], json_get_string(json_get(typenames, 0)), 32);
                    if (json_get_type(pname) == JSON_STRING)
                        strncpy(f->param_names[j], json_get_string(pname), 32);
                    else
                        strncpy(f->param_names[j], "(unnamed)", 32);
                }
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
        printf("\n\n");
    }

    json_free(root);
    return 0;
}
