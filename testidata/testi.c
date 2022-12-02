#include <stdio.h>
#include <string.h>

#define RXBUFLEN 80

void buzzerWarning(void) {
    printf("\nbuzzerWarning!\n");
}

int main(void) {
    char dest[16];
    char ourId[10] = "2230,BEEP";
    char uartStr[80] = "2230,BEEP:Severe warning about my wellbeing";

    strncpy(dest, uartStr, 9); // dest = 2230,BEEP

    printf("dest: %s\n", dest);
    printf("ourId: %s\n", ourId);

    if (strcmp(ourId, dest) == 0) {
        buzzerWarning(); // varoitusmusiikki
    } else if (strcmp(ourId, dest) != 0) {
        printf("error");
    }
}