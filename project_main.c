/* JTKJ Harjoitustyö 2022
    Tomi Pantsar
    Santeri Heikkinen
*/

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
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>
#include <driverlib/timer.h>

/* TI-RTOS Header files (PWM_Led_example)
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>
*/

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "sensors/tmp007.h"
#include "buzzer.h"

/* Task */
#define STACKSIZE 2048
char sensorTaskStack[STACKSIZE];
char uartTaskStack[STACKSIZE];
char commTaskStack[STACKSIZE];

// Tilakone Tamagotchin toiminnoille (ruokkiminen, leikkiminen, nukkuminen, varoitus, hyytyminen)
enum state
{
    WAITING = 1,
    DATA_READY,
    TOIMINTO_RUOKI,
    TOIMINTO_LEIKI,
    TOIMINTO_NUKU,
    TOIMINTO_VAROITUS,
    TOIMINTO_HYYTYMINEN
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum state programState = WAITING;

// Tilakone SensorTagin toiminnoille
enum stateSensor
{
    ODOTUSTILA = 1,
    STATE_PAIKALLAAN, // liikesensori
    STATE_LIIKE,      // liikesensori
    STATE_VALO,       // valosensori
    STATE_PIMEA,      // valosensori
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum stateSensor sensorTagState = ODOTUSTILA;

// Merkkijonomuuttujat tulostuksille
char debug_msg[100];

// Globaalit muuttujat Tamagotchin ominaisuuksille
double ambientLight = -1000.0;
int ravinto = 5;
int leikki = 5;
int energia = 5;

/* PINNIEN ALUSTUKSET JA MUUTTUJAT */

// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUConfig = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

static PIN_Handle MpuPinHandle;
static PIN_State MpuPinState;

// Painonappi
PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

static PIN_Handle buttonHandle;
static PIN_State buttonState;

// Vihreä ledi
PIN_Config LED0_ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Punainen ledi
PIN_Config LED1_ledConfig[] = {
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

static PIN_Handle LED0_Handle;
static PIN_State LED0_State;
static PIN_Handle LED1_Handle;
static PIN_State LED1_State;

// Buzzer config
PIN_Config buzzerConfig[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

static PIN_Handle buzzerHandle;
static PIN_State buzzerState;

/* Napinpainalluksen keskeytyksen käsittelijäfunktio */
void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    // JTKJ: Tehtava 1. Vilkuta jompaa kumpaa ledia

    // Vaihdetaan led-pinnin tilaa negaatiolla
    uint_t pinValue = PIN_getOutputValue(Board_LED0);
    pinValue = !pinValue;
    PIN_setOutputValue(LED0_Handle, Board_LED0, pinValue);
}

// Musiikki nukkumiselle
void buzzerSleep(UArg arg0, UArg arg1)
{
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(262); // c
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(262); // c
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(262); // c
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(330); // e
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // d
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // d
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // d
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(349); // f
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(330); // e
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(330); // e
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // d
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // d
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(262); // c
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerClose();
}

// Musiikki leikille
void buzzerPlay(UArg arg0, UArg arg1)
{
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
}

// Musiikki varoitukselle
void buzzerWarning(UArg arg0, UArg arg1)
{
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
}

// Musiikki hyytymiselle
void buzzerCongeal(UArg arg0, UArg arg1)
{
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
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

    // Vastaanotetaan 1 merkki kerrallaan input-muuttujaan
    UART_read(uart, &input, 1);

    // Lähetetään merkkijono takaisin
    sprintf(echo_msg, "uartTask: %lf luksia\n\r", ambientLight);
    UART_write(uart, echo_msg, strlen(echo_msg));

    while (1)
    {
        System_printf("uartTask");
        System_flush();

        if (programState == DATA_READY)
        {
            programState = WAITING;
        }

        else if (programState == WAITING)
        {
            programState = WAITING;
        }

        switch (programState)
        {
        case TOIMINTO_RUOKI:
            sprintf(echo_msg, "%s", "Syodaan!");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, echo_msg, strlen(echo_msg));
            StartReceive6LoWPAN();

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
            break;

        case TOIMINTO_LEIKI:
            sprintf(echo_msg, "%s", "Leikitaan!");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, echo_msg, strlen(echo_msg));
            StartReceive6LoWPAN();

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
            break;

        case TOIMINTO_NUKU:
            sprintf(echo_msg, "%s", "ZzzZzZ...");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, echo_msg, strlen(echo_msg));
            StartReceive6LoWPAN();

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
            break;

        case TOIMINTO_VAROITUS:
            sprintf(echo_msg, "%s", "SOS!");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, echo_msg, strlen(echo_msg));
            StartReceive6LoWPAN();

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
            break;

        case TOIMINTO_HYYTYMINEN:
            sprintf(echo_msg, "%s", "RIP");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, echo_msg, strlen(echo_msg));
            StartReceive6LoWPAN();

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
            break;

        default: // esim. WAITING tai DATA_READY
            break;
        }

        // Once per second
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

void sensorTaskFxn(UArg arg0, UArg arg1)
{
    // RTOS:n i2c-muuttujat ja alustus
    I2C_Handle i2c;
    I2C_Params i2cParams;
    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // Alustetaan i2cMPU-väylä
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;

    // Note the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUConfig;

    // Kiihtyvyys- ja liikeanturin muuttujat
    float ax, ay, az, gx, gy, gz;

    // MPU power on
    PIN_setOutputValue(MpuPinHandle, Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL)
    {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU setup and calibration
    Task_sleep(100000 / Clock_tickPeriod);
    mpu9250_setup(&i2cMPU);
    I2C_close(i2cMPU);

    // OPT3001 and TMP007 open i2c
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // OPT3001 setup and calibration
    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);
    I2C_close(i2c);

    while (1)
    {
        sprintf(debug_msg, "programState: %d\n", programState);
        System_printf(debug_msg);
        System_flush();

        if (programState == WAITING) // if (programState == DATA_READ)
        {
            PIN_setOutputValue(LED0_Handle, Board_LED0, 0);
            PIN_setOutputValue(LED1_Handle, Board_LED1, 0);

            // OPT3001 open i2c
            i2c = I2C_open(Board_I2C_TMP, &i2cParams);

            // OPT3001 ask data
            ambientLight = opt3001_get_data(&i2c);

            // Sleep 100ms
            Task_sleep(100000 / Clock_tickPeriod);

            // OPT3001 close i2c
            I2C_close(i2c);

            // MPU open i2c
            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);

            // MPU ask data
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

            // Sleep 100ms
            Task_sleep(100000 / Clock_tickPeriod);

            // MPU close i2c
            I2C_close(i2cMPU);

            // Print OPT3001 values (ambientLight)
            sprintf(debug_msg, "%.2lf luksia\n", ambientLight);
            System_printf(debug_msg);
            System_flush();

            // Print MPU values (motion)
            sprintf(debug_msg, "%.2lf, %.2lf, %.2lf, %.2lf, %.2lf, %.2lf\n", ax, ay, az, gx, gy, gz);
            System_printf(debug_msg);
            System_flush();

            programState = DATA_READY;
        }

        if (programState == DATA_READY)
        {
            printf("KALA2!!\n");
            sensorTagState = ODOTUSTILA;
        }

        // Once per second
        Task_sleep(1000000L / Clock_tickPeriod);
        printf("KALA!!\n");
    }
}

// Langaton viestintä
void commTask(UArg arg0, UArg arg1)
{
    char payload[16]; // viestipuskuri
    uint16_t senderAddr;

    // Radio alustetaan vastaanottotilaan
    int32_t result = StartReceive6LoWPAN();
    if (result != true)
    {
        System_abort("Wireless receive start failed");
    }

    // Vastaanotetaan viestejä loopissa
    while (true)
    {
        // jos true, viesti odottaa
        if (GetRXFlag())
        {
            // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
            memset(payload, 0, 16);
            // Luetaan viesti puskuriin payload
            Receive6LoWPAN(&senderAddr, payload, 16);
            // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
            System_printf(payload);
            System_flush();
        }
    }
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
    Board_initI2C();
    Board_initUART();
    Init6LoWPAN();

    // Open MPU power pin
    MpuPinHandle = PIN_open(&MpuPinState, MpuPinConfig);
    if (MpuPinHandle == NULL)
    {
        System_abort("Pin open failed!");
    }

    // Open buzzer pin
    buzzerHandle = PIN_open(&buzzerState, buzzerConfig);
    if (buzzerHandle == NULL)
    {
        System_abort("Pin open failed!");
    }

    // Painonappi käyttöön ohjelmassa
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle)
    {
        System_abort("Error initializing button pins\n");
    }

    // Ledi käyttöön ohjelmassa
    LED0_Handle = PIN_open(&LED0_State, LED0_ledConfig);
    if (!LED0_Handle)
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

    /* Communication Task */
    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority = 1; // Important to set the priority to 1
    commTaskHandle = Task_create(commTask, &commTaskParams, NULL);
    if (commTask == NULL)
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
