/* C Standard library */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/buzzer.h"
#include "sensors/mpu9250.h"
#include "sensors/tmp007.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// JTKJ: Tehtava 3. Tilakoneen esittely
enum state
{
    WAITING = 1,
    DATA_READY
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum state programState = WAITING;

// Merkkijonomuuttujat tulostuksille
char debug_msg[100];

// JTKJ: Tehtava 3. Valoisuuden globaali muuttuja
double ambientLight = -1000.0;

// JTKJ: Tehtava 1. Lisaa painonappien RTOS-muuttujat ja alustus
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

// Pinnien alustukset, molemmille pinneille oma konfiguraatio
// Vakio BOARD_BUTTON_0 vastaa toista painonappia
PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Vakio Board_LED0 vastaa toista lediä
PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Napinpainalluksen keskeytyksen käsittelijäfunktio
void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    // JTKJ: Tehtava 1. Vilkuta jompaa kumpaa ledia

    // Vaihdetaan led-pinnin tilaa negaatiolla
    uint_t pinValue = PIN_getOutputValue(Board_LED0);
    pinValue = !pinValue;
    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1)
{
    char input;
    char echo_msg[30];

    // UART-kirjaston asetukset
    UART_Handle uart;
    UART_Params uartParams;

    // JTKJ: Tehtava 4. Lisaa UARTin alustus: 9600,8n1
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;            // nopeus 9600 baud
    uartParams.dataLength = UART_LEN_8;    // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE;   // 1

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL)
    {
        System_abort("Error opening the UART");
    }

    while (1)
    {
        // JTKJ: Tehtava 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //       Muista tilamuutos

        if (programState == DATA_READY)
        {
            sprintf(debug_msg, "uartTask: %f luksia\n", ambientLight);
            System_printf(debug_msg);
            System_flush();
            programState = WAITING;
        }

        // JTKJ: Tehtava 4. Laheta sama merkkijono UARTilla

        // Vastaanotetaan 1 merkki kerrallaan input-muuttujaan
        // UART_read(uart, &input, 1);

        // Lähetetään merkkijono takaisin
        sprintf(echo_msg, "uartTask: %lf luksia\n\r", opt3001_get_data(&i2c));
        UART_write(uart, echo_msg, strlen(echo_msg));

        // Kohteliaasti nukkumaan sekunniksi
        Task_sleep(1000000L / Clock_tickPeriod);

        // Just for sanity check for exercise, you can comment this out
        System_printf("uartTask\n");

        / System_flush();

        /* Lopuksi sarjaliikenneyhteys pitää sulkea UART_Close-kutsulla,
        mutta esimerkissä sitä ei ole, koska toimimme ikuisessa silmukassa.*/
    }
}

void sensorTaskFxn(UArg arg0, UArg arg1)
{
    // RTOS:n i2c-muuttujat ja alustus
    I2C_Handle i2c;
    I2C_Params i2cParams;
    // Muuttuja i2c-viestirakenteelle
    I2C_Transaction i2cMessage;

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    float ax, ay, az, gx, gy, gz;

    I2C_Handle i2cMPU;
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;

    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL)
    {
        System_abort("Error Initializing I2CMPU\n");
    }

    // JTKJ: Tehtava 2. Avaa valosensorin i2c-vayla taskin kayttoon
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // JTKJ: Tehtava 2. Alusta sensori OPT3001 setup-funktiolla.
    //       Laita ennen funktiokutsua eteen 100ms viive (Task_sleep)
    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);

    while (1)
    {
        // JTKJ: Tehtava 2. Lue sensorilta dataa ja tulosta se Debug-ikkunaan merkkijonona
        double lux = opt3001_get_data(&i2c);
        char lux_str[50];
        sprintf(lux_str, "sensorTask: %lf luksia\n", lux);
        System_printf("%s\n", lux_str);
        System_flush();

        // JTKJ: Tehtava 3. Tallenna mittausarvo globaaliin muuttujaan. Muista tilamuutos
        if (programState == WAITING)
        {
            ambientLight = lux;
            // System_flush();
            programState = DATA_READY;
        }

        // Just for sanity check for exercise, you can comment this out
        // System_printf("sensorTask\n");
        System_flush();

        // Once per second
        Task_sleep(1000000 / Clock_tickPeriod);
    }
    I2C_close(i2c); // Suljetaan i2c-vayla
}

int main(void)
{
    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;
    Task_Handle commTaskHandle;
    Task_Params commTaskParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // JTKJ: Tehtava 2. Ota i2c-vayla kayttoon ohjelmassa
    Board_initI2C();

    // JTKJ: Tehtava 4. Ota UART kayttoon ohjelmassa
    Board_initUART();

    // JTKJ: Tehtava 1. Ota painonappi ja ledi ohjelman kayttoon
    //       Muista rekisteroida keskeytyksen kasittelija painonapille

    // Painonappi käyttöön ohjelmassa
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle)
    {
        System_abort("Error initializing button pins\n");
    }

    // Ledi käyttöön ohjelmassa
    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle)
    {
        System_abort("Error initializing LED pins\n");
    }

    // Asetetaan painonapille keskeytyksen käsittelijä (funktio buttonFxn)
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0)
    {
        System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);

    if (uartTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
