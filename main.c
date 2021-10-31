#include <stdio.h>
#include "bitmap.h"

int main(void){
    Bmp test = read_bmp("basic_parity_error.bmp");
    printf("%d", test);
    // write_bmp(test, "test.bmp");
    return 1;
}
