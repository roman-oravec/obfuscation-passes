#include <stdlib.h>

//extern void printArray(int arr[], int size);
extern void quickSort(int arr[], int low, int high);


int main(int argc, char *argv[]){
  int n = atoi(argv[1]);
  int *arr = malloc(n*sizeof(int));
  srand(0);
  for (int i = 0; i < n; i++) {
    arr[i] = rand();
  }
	quickSort(arr, 0, n - 1);
  free(arr);
  //printArray(arr,n);
  return 0;
}