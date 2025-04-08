#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// "json_c.h" / "json_c.c" (질문에서 주신 라이브러리) 사용
#include "json_c.c"

/* ------------------------------------------------
 * 함수(선언 or 정의)의 시그니처 복원
 * - Decl 노드(함수 이름, 리턴타입, 파라미터) → 문자열로
 *   예) "int main(int argc, char *argv)" 등
 * ------------------------------------------------*/
static void rebuild_function_signature(json_value declNode, char *outbuf, size_t outsize) {
    // declNode: _nodetype == "Decl"
    //   - name: 함수명
    //   - type: { _nodetype: "FuncDecl", ... }
    //     - type: { _nodetype: "TypeDecl",  type: { _nodetype: "IdentifierType", names: [...] } }
    //     - args: ParamList or null
    // outbuf: 결과를 쓸 버퍼

    if (declNode.type != JSON_OBJECT) {
        snprintf(outbuf, outsize, "/* not a Decl node */");
        return;
    }

    // 1) 함수명
    char *fname = json_get_string(declNode, "name");
    if (!fname) {
        snprintf(outbuf, outsize, "/* name not found */");
        return;
    }

    // 2) 함수 타입 (FuncDecl)
    json_value typeObj = json_get(declNode, "type");
    if (typeObj.type != JSON_OBJECT) {
        snprintf(outbuf, outsize, "/* type is not object */");
        return;
    }
    char *typeNodeType = json_get_string(typeObj, "_nodetype");  // "FuncDecl"?
    if (!typeNodeType || strcmp(typeNodeType, "FuncDecl") != 0) {
        snprintf(outbuf, outsize, "/* not a FuncDecl: %s */", (typeNodeType ? typeNodeType : "null"));
        return;
    }

    // 3) 리턴타입
    //    typeObj -> "type" -> { _nodetype: "TypeDecl", type: { _nodetype: "IdentifierType", names: [...] } }
    json_value typeObj2 = json_get(typeObj, "type");
    if (typeObj2.type != JSON_OBJECT) {
        snprintf(outbuf, outsize, "/* no 'type' object? */");
        return;
    }
    // 여기서 _nodetype == "TypeDecl" 또는 "PtrDecl" 등등 있을 수 있음
    // 간단히 "TypeDecl"인 경우만 처리
    char *typeNode2 = json_get_string(typeObj2, "_nodetype"); // "TypeDecl"? "PtrDecl"?
    char retTypeBuf[256] = "(unknown_return_type)";

    if (typeNode2 && strcmp(typeNode2, "TypeDecl") == 0) {
        // typeObj2 -> "type" -> { _nodetype: "IdentifierType", names: [...] }
        json_value idtype = json_get(typeObj2, "type");
        if (idtype.type == JSON_OBJECT) {
            char *idnodetype = json_get_string(idtype, "_nodetype"); // "IdentifierType"
            if (idnodetype && strcmp(idnodetype, "IdentifierType") == 0) {
                // names 배열
                json_value namesArr = json_get(idtype, "names");
                if (namesArr.type == JSON_ARRAY) {
                    // 예: ["int"] or ["unsigned","long"] ...
                    int n = json_len(namesArr);
                    if (n > 0) {
                        // 간단히 첫 항목만 받아보자 (복합타입이면 "unsigned long" 식으로 붙일 수도)
                        char combined[256] = "";
                        combined[0] = '\0';
                        for (int i = 0; i < n; i++) {
                            char *oneName = json_get_string(namesArr, i);
                            if (oneName) {
                                if (i > 0) strcat(combined, " ");
                                strcat(combined, oneName);
                            }
                        }
                        snprintf(retTypeBuf, sizeof(retTypeBuf), "%s", combined);
                    }
                }
            }
        }
    }
    else if (typeNode2 && strcmp(typeNode2, "PtrDecl") == 0) {
        // 예: (예시: "void *malloc(int)")
        // typeObj2 -> "type" -> "TypeDecl" -> "type" -> "IdentifierType"
        // 좀 더 복잡하지만 간단히 "void*"로 처리
        json_value inner = json_get(typeObj2, "type"); // TypeDecl
        if (inner.type == JSON_OBJECT) {
            json_value innertype = json_get(inner, "type");
            if (innertype.type == JSON_OBJECT) {
                char *idnodetype = json_get_string(innertype, "_nodetype"); // "IdentifierType"
                if (idnodetype && strcmp(idnodetype, "IdentifierType") == 0) {
                    // names 배열
                    json_value namesArr = json_get(innertype, "names");
                    if (namesArr.type == JSON_ARRAY && json_len(namesArr) > 0) {
                        char combined[256] = "";
                        combined[0] = '\0';
                        for (int i = 0; i < json_len(namesArr); i++) {
                            char *oneName = json_get_string(namesArr, i);
                            if (oneName) {
                                if (i > 0) strcat(combined, " ");
                                strcat(combined, oneName);
                            }
                        }
                        // pointer 붙이기
                        snprintf(retTypeBuf, sizeof(retTypeBuf), "%s *", combined);
                    }
                }
            }
        }
    }

    // 4) 파라미터 리스트
    //    typeObj -> "args" -> { _nodetype: "ParamList", "params": [ ... ] } or null
    char paramBuf[256];
    paramBuf[0] = '\0';

    json_value argsObj = json_get(typeObj, "args");
    if (argsObj.type == JSON_OBJECT) {
        char *argsNodetype = json_get_string(argsObj, "_nodetype"); // "ParamList"?
        if (argsNodetype && strcmp(argsNodetype, "ParamList") == 0) {
            json_value paramsArr = json_get(argsObj, "params"); // 배열
            if (paramsArr.type == JSON_ARRAY) {
                int pcount = json_len(paramsArr);
                if (pcount == 0) {
                    // "void" 파라미터?
                    snprintf(paramBuf, sizeof(paramBuf), "(void)");
                } else {
                    // 실제 파라미터 목록을 만들어 보자
                    // 예: (int x, float y)
                    char tmp[256];
                    tmp[0] = '\0';
                    for (int i = 0; i < pcount; i++) {
                        // i번째 파라미터
                        json_value pnode = json_get(paramsArr, i);
                        // pnode._nodetype => "Typename" 또는 "Decl"
                        char *pnodetype = json_get_string(pnode, "_nodetype");
                        char paramType[64] = "(unknown)";
                        char paramName[64] = "";

                        if (pnodetype && strcmp(pnodetype, "Typename") == 0) {
                            // 익명 파라미터?
                            // pnode->"type"->"TypeDecl"->"type"->"IdentifierType"->"names"
                            // 변수명은 없는 경우가 많음
                            // ...
                            json_value typeDecl = json_get(pnode, "type");
                            // (이하 생략... 위와 비슷)
                            // 간단히 "int" 등 하나만 뽑는 식으로 처리
                            // ... 여기서는 예시로만 두겠습니다
                            snprintf(paramType, sizeof(paramType), "int /*?*/");
                            paramName[0] = '\0';
                        }
                        else if (pnodetype && strcmp(pnodetype, "Decl") == 0) {
                            // 예: { "_nodetype":"Decl", "name":"x", "type":{...} }
                            // type 안 보고 간단히 "int"라고 가정. 실제로는 위 리턴타입 로직과 동일하게 파싱해야 함
                            char *pname = json_get_string(pnode, "name");
                            if (pname) {
                                snprintf(paramName, sizeof(paramName), "%s", pname);
                            }
                            // 타입도 뽑아야 함 (위와 유사 로직)
                            // 여기서는 간단히 "???"
                            snprintf(paramType, sizeof(paramType), "???");
                        }
                        if (i > 0) strcat(tmp, ", ");
                        if (strlen(paramName) > 0)
                            sprintf(tmp + strlen(tmp), "%s %s", paramType, paramName);
                        else
                            sprintf(tmp + strlen(tmp), "%s", paramType);
                    }
                    snprintf(paramBuf, sizeof(paramBuf), "(%s)", tmp);
                }
            }
        }
    }
    if (paramBuf[0] == '\0') {
        // 파라미터가 없는 함수
        snprintf(paramBuf, sizeof(paramBuf), "(void)"); 
    }

    // 최종 signature
    snprintf(outbuf, outsize, "%s %s%s", retTypeBuf, fname, paramBuf);
}

/* ---------------------------------------
 * body(함수 본문)을 재귀적으로 순회하며
 *  - if문 개수 세기
 *  - 간단 복원 (예: { ... } )
 * ---------------------------------------*/
static int count_if = 0;

static void traverse_body(json_value node, int depth, FILE *outfp)
{
    // node가 OBJECT이거나 ARRAY라면 깊이 탐색
    if (node.type == JSON_OBJECT) {
        char *ntype = json_get_string(node, "_nodetype");
        if (ntype) {
            // if문 체크
            if (strcmp(ntype, "If") == 0) {
                count_if++;
            }
            // "Compound"이면 { ... } 로 감싸서 재귀
            if (strcmp(ntype, "Compound") == 0) {
                fprintf(outfp, "\n");
                for (int i=0; i<depth; i++) fprintf(outfp, "    "); // 들여쓰기
                fprintf(outfp, "{\n");
                // block_items 배열 순회
                json_value items = json_get(node, "block_items");
                if (items.type == JSON_ARRAY) {
                    int N = json_len(items);
                    for (int i=0; i<N; i++) {
                        json_value child = json_get(items, i);
                        traverse_body(child, depth+1, outfp);
                    }
                }
                for (int i=0; i<depth; i++) fprintf(outfp, "    ");
                fprintf(outfp, "}\n");
                return;
            }
            // "FuncCall", "Decl", "Return" 등등 처리 가능
            // 여기서는 Return만 조금 예시
            if (strcmp(ntype, "Return") == 0) {
                for (int i=0; i<depth; i++) fprintf(outfp, "    ");
                fprintf(outfp, "return ...;\n"); // 간단히
                return;
            }
        }
        // 그 외 노드에 대해서도 하위 field를 전부 순회
        // json_c.h 로는 "key" 목록을 한 번에 못 얻지만,
        // 적당히 우리가 "의미있는" 필드를 찾거나,
        // 혹은 전부 순회하며 OBJECT/ARRAY면 재귀하는 식
        // 여기서는 "의미있는" block_items 등만 예시로 재귀
        // 더 세밀한 복원은 많은 추가 구현이 필요.
    }
    else if (node.type == JSON_ARRAY) {
        int N = json_len(node);
        for (int i=0; i<N; i++) {
            json_value child = json_get(node, i);
            traverse_body(child, depth, outfp);
        }
    }
    // 그 외 (STRING, NUMBER...)는 여기선 무시
}

/* ---------------------------------------
 * 메인 함수
 * ---------------------------------------*/
int main(void) {
    // 1) ast.json 읽기
    FILE *fp = fopen("ast.json", "r");
    if (!fp) {
        fprintf(stderr, "Fail to open ast.json\n");
        return 1;
    }

    // 파일 사이즈 구하기
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = (char *)malloc(fsize + 1);
    if (!buffer) {
        fclose(fp);
        return 1;
    }
    size_t rsz = fread(buffer, 1, fsize, fp);
    buffer[rsz] = '\0';
    fclose(fp);

    // 2) JSON 파싱
    json_value root = json_create(buffer);
    if (root.type != JSON_OBJECT) {
        fprintf(stderr, "Root is not an object. _nodetype?\n");
        free(buffer);
        json_free(root);
        return 1;
    }

    // 3) "ext" 배열을 가져옴
    json_value extArr = json_get(root, "ext");
    if (extArr.type != JSON_ARRAY) {
        fprintf(stderr, "'ext' is not an array.\n");
        free(buffer);
        json_free(root);
        return 1;
    }
    int extCount = json_len(extArr);

    // 4) 순회하며 함수(또는 선언)를 찾는다
    int funcDefCount = 0;
    int declCount    = 0;
    for (int i = 0; i < extCount; i++) {
        json_value node = json_get(extArr, i);
        // node: { "_nodetype": "Decl" or "FuncDef", ... }
        if (node.type != JSON_OBJECT) {
            continue;
        }
        char *nodetype = json_get_string(node, "_nodetype");
        if (!nodetype) continue;

        if (strcmp(nodetype, "FuncDef") == 0) {
            // 함수 정의!
            funcDefCount++;
            // node -> "decl" => 함수 선언부(이름, 타입, 파라미터)
            json_value declNode = json_get(node, "decl");
            if (declNode.type == JSON_OBJECT) {
                char signbuf[512];
                rebuild_function_signature(declNode, signbuf, sizeof(signbuf));
                printf("\n[FuncDef] %s\n", signbuf);

                // if 갯수 세기 + body 복원
                count_if = 0;
                json_value body = json_get(node, "body");
                // 간단히 출력
                FILE *outfp = stdout;
                traverse_body(body, 1, outfp);
                printf(" -> IF 문 개수: %d\n", count_if);
            }
        }
        else if (strcmp(nodetype, "Decl") == 0) {
            // 함수 선언 or 변수 선언?
            // 구분하려면 node->"type"->"_nodetype" == "FuncDecl" 확인
            json_value type = json_get(node, "type");
            if (type.type == JSON_OBJECT) {
                char *typeN = json_get_string(type, "_nodetype");
                if (typeN && strcmp(typeN, "FuncDecl") == 0) {
                    declCount++;
                    char signbuf[512];
                    rebuild_function_signature(node, signbuf, sizeof(signbuf));
                    printf("\n[FuncDecl] %s;\n", signbuf);
                }
            }
        }
    }

    printf("\n총 FuncDef 개수: %d\n", funcDefCount);
    printf("총 FuncDecl(선언) 개수: %d\n", declCount);

    // 5) 정리
    json_free(root);
    free(buffer);
    return 0;
}