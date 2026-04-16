#include <stdio.h>
int main(int argc, char ** argv){
    void ** items = malloc(sizeof(void**) * 2);
    items[0] = (char*) malloc(sizeof("ana"));
    strcpy(items[0], "ana");
    items[1] = (int*) malloc(4);
    items[1][0] = 1;
    int i;
    for(i=0;i<2;i++){
        for(j=0;j<sizeof(items[i]);j++){
            if(i==0){
                printf("%c",items[i][j]);
            }
            else printf("%d", items[i][j]);
        }
        printf("\n");
    }
}