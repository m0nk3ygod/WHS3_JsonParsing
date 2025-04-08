#include <stdio.h>
#include <stdlib.h>
#include "json_c.c"

int main(void) {
    FILE * fp;
    fp = fopen("ast.json", "r");
    
    if (fp == NULL) printf("파일열기 실패\n");
    else printf("파일열기 성공\n");
    
    int c;
    int size=0;
    while((c = fgetc(fp)) != EOF) {
        size++;
    }

    rewind(fp);

    char* buf = (char*)malloc(size + 1);
    if(!buf) {
        printf("메모리 할당 실패\n");
        fclose(fp);
        return 1;
    }

    printf("%d\n", size);

    size_t readSize = fread(buf, 1, size, fp);
    buf[readSize] = '\0';

    fclose(fp);

    json_value root = json_create(buf);

    json_print(root);

    json_free(root);
    free(buf);

    return 0;

}