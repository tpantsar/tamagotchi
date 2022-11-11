/* C Standard library */
#include <stdio.h>

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

// #include "wireless/comm_lib.h"
#include "sensors/opt3001.h"

#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// JTKJ: Tehtava 3. Tilakoneen esittely
// JTKJ: Exercise 3. Definition of the state machine
enum state
{
    WAITING = 1,
    DATA_READY
};
enum state programState = WAITING;

// JTKJ: Tehtava 3. Valoisuuden globaali muuttuja
// JTKJ: Exercise 3. Global variable for ambient light
double ambientLight = -1000.0;

// JTKJ: Tehtava 1. Lisaa painonappien RTOS-muuttujat ja alustus
// JTKJ: Exercise 1. Add pins RTOS-variables and configuration here
// RTOS:n globaalit muuttujat pinnien käyttöön
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
    // JTKJ: Exercise 1. Blink either led of the device

    // Vaihdetaan led-pinnin tilaa negaatiolla
    uint_t pinValue = PIN_getOutputValue(Board_LED0);
    pinValue = !pinValue;
    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1)
{
    // JTKJ: Tehtava 4. Lisaa UARTin alustus: 9600,8n1
    // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1

    while (1)
    {
        // JTKJ: Tehtava 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Print out sensor data as string to debug window if the state is correct
        //       Remember to modify state

        // JTKJ: Tehtava 4. Laheta sama merkkijono UARTilla
        // JTKJ: Exercise 4. Send the same sensor data string with UART

        // Just for sanity check for exercise, you can comment this out
        System_printf("uartTask\n");
        System_flush();

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

void sensorTaskFxn(UArg arg0, UArg arg1)
{
    char debug_msg[50];

    // RTOS:n i2c-muuttujat ja alustus
    I2C_Handle i2c;
    I2C_Params i2cParams;
    // Muuttuja i2c-viestirakenteelle
    I2C_Transaction i2cMessage;

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // JTKJ: Tehtava 2. Avaa i2c-vayla taskin kayttoon
    // JTKJ: Exercise 2. Open the i2c bus
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // JTKJ: Tehtava 2. Alusta sensori OPT3001 setup-funktiolla.
    //       Laita ennen funktiokutsua eteen 100ms viive (Task_sleep)
    // JTKJ: Exercise 2. Setup the OPT3001 sensor for use
    //       Before calling the setup function, insert 100ms delay with Task_sleep
    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);

    // https://lovelace.oulu.fi/tietokonej%C3%A4rjestelm%C3%A4t/tietokonej%C3%A4rjestelm%C3%A4t-syksy-2022/sarjaliikenne/#i2c-v%C3%A4yl%C3%A4
    while (1)
    {
        /*
        sprintf(debug_msg, "%d\n", programState);
        System_printf(debug_msg);
        System_flush();
        */

        // JTKJ: Tehtava 2. Lue sensorilta dataa ja tulosta se Debug-ikkunaan merkkijonona
        // JTKJ: Exercise 2. Read sensor data and print it to the Debug window as string
        ambientLight = opt3001_get_data(&i2c);

        // Valoisuusarvo tiedoksi konsoli-ikkunaan
        sprintf(debug_msg, "%f\n", ambientLight);
        System_printf(debug_msg);
        System_flush();

        // JTKJ: Tehtava 3. Tallenna mittausarvo globaaliin muuttujaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Save the sensor value into the global variable
        //       Remember to modify state

        // Just for sanity check for exercise, you can comment this out
        System_printf("sensorTask\n");
        System_flush();

        // Taski nukkumaan!
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

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // JTKJ: Tehtava 2. Ota i2c-vayla kayttoon ohjelmassa
    // JTKJ: Exercise 2. Initialize i2c bus
    Board_initI2C();

    // JTKJ: Tehtava 4. Ota UART kayttoon ohjelmassa
    // JTKJ: Exercise 4. Initialize UART

    // JTKJ: Tehtava 1. Ota painonappi ja ledi ohjelman kayttoon
    //       Muista rekisteroida keskeytyksen kasittelija painonapille
    // JTKJ: Exercise 1. Open the button and led pins
    //       Remember to register the above interrupt handler for button

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
