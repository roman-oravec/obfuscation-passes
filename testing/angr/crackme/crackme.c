#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void pwd(int a, int b, char **pass){
  char c = (char) a*b;
  char p[4] = {'O', c, 'F', '\0'};
  *pass = p;
  return;
}


int main(int argc, char** argv) {
if (argc < 2){
  return 1;
}
char *password; 
pwd(argc, 33, &password); 
if (strcmp(argv[1], password)) {
        printf("Wrong password\n");
        return 1;
    } else {
        printf("Correct\n");
        return 0;
    }
}