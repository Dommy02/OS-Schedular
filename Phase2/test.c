#include"headers.h"
int main() {
    int num;

    printf("Enter a hexadecimal number (e.g., 0x10): ");
    scanf("%x", &num);  // %x reads hexadecimal input

    printf("Decimal value = %d\n", num);

    printf("%x\n", num);
    printf("Hexadecimal value = 0x%X\n", num);

    return 0;
}