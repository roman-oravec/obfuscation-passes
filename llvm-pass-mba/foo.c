#include <stdio.h>

int main() {
	int a = 5;
	int b = 6;
	if (a>b){
		a -= 3;
		b += 3;
		return 7;
	} else {
		a += 3;
		b -= 3;
		return 8;
	}
}
