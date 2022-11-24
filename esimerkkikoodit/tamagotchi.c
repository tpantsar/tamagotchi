/*
JTKJ Harjoitustyö 2022 - Tamagotchi

Tekijät:
    Aleksi Nurminen  - 1.2.246.562.24.46646649698
    Rohit Pandey     - 1.2.246.562.24.75970177179
    Pyry Rannanmäki  - 1.2.246.562.24.42410276534
*/

// Otsikkotiedostot alla

/* C Standard library */
#include <stdio.h>
#include <string.h>
#include <math.h>

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

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "buzzer.h"

// Itse koodi alkaa

/* Task Stacks */
#define STACKSIZE 1024
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char analyzeTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];
Char msgTaskStack[STACKSIZE];

/* Other Variables */

enum state
{
    WAITING = 1,
    READ_SENSOR,
    ANALYZE_DATA,
    SEND_MSG,
    ANALYZE_MSG
};
enum state programState = WAITING;

typedef struct AccelData
{
    float x;
    float y;
    float z;
} Acc;

#define SENSORDATAPOINTS 10

enum comType
{
    COMTYPE_UART = 1,
    COMTYPE_LOWPAN
};

enum comType COMTYPE = COMTYPE_LOWPAN; // Do we use uart or 6LoWPAN for sending and receiving messages?

Acc accelData[SENSORDATAPOINTS] = {};
unsigned char latestIndex = SENSORDATAPOINTS - 1; // Sensorin lukema päivitetään uusimpaan. Käänteinen järjestys. Helpottaa lukemista.

char writeMsg[20];

char readBuffer[80];

/* RTOS Variables */

static PIN_Handle buttonHandle; // Button RTOS Variables
static PIN_State buttonState;

// Power button RTOS Variables
static PIN_Handle powerButtonHandle;
static PIN_State powerButtonState;

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;

// Buzzer RTOS Variables
static PIN_Handle buzzerHandle;
static PIN_State buzzerState;

// Buzzer clock variables
Clock_Handle buzzerClkHandle;
Clock_Params buzzerClkParams;

/* Pin Configs */

PIN_Config buzzerConfig[] = { // Buzzer pin configuration
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

// Button pin configuration
PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

// Power button pin configurations
PIN_Config powerButtonConfig[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};
PIN_Config powerButtonWakeConfig[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE};

// MPU power pin configuration
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

// SFX muuttujat
enum toneType
{
    STATIC = 1,
    RISING,
    FALLING
};
enum toneType currentType = STATIC;
int currentPitchModifierValue;
int currentTimeModifierValue;
int buzzerStartTime;
int buzzerDuration;
int buzzerFrequency;

/* Non-Task Functions */

//Äänet ja niihin liittyvä
void playTone(int frequency, int duration, enum toneType type, int pitchModifier, int timeModifier)
{
    currentType = type;
    currentPitchModifierValue = pitchModifier;
    currentTimeModifierValue = timeModifier;
    buzzerStartTime = Clock_getTicks();
    buzzerFrequency = frequency;
    buzzerDuration = duration;

    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(buzzerFrequency);

    switch (currentType)
    {

    case STATIC:
        Clock_setTimeout(buzzerClkHandle, buzzerDuration);
        break;

    case RISING:
        Clock_setTimeout(buzzerClkHandle, currentTimeModifierValue);
        break;

    case FALLING:
        Clock_setTimeout(buzzerClkHandle, currentTimeModifierValue);
        break;

    default:
        Clock_setTimeout(buzzerClkHandle, buzzerDuration);
    }

    Clock_start(buzzerClkHandle);
}

void buzzerClockFxn(UArg arg0)
{
    if (Clock_getTicks() < buzzerStartTime + (buzzerDuration))
    {
        switch (currentType)
        {

        case STATIC:
            buzzerClose();
            Clock_stop(buzzerClkHandle);
            break;

        case RISING:
            buzzerFrequency = buzzerFrequency + currentPitchModifierValue;
            buzzerSetFrequency(buzzerFrequency);
            Clock_setTimeout(buzzerClkHandle, currentTimeModifierValue);
            Clock_start(buzzerClkHandle);
            break;

        case FALLING:
            buzzerFrequency = buzzerFrequency - currentPitchModifierValue;
            buzzerSetFrequency(buzzerFrequency);
            Clock_setTimeout(buzzerClkHandle, currentTimeModifierValue);
            Clock_start(buzzerClkHandle);
            break;

        default:
            buzzerClose();
            Clock_stop(buzzerClkHandle);
        }
    }
    else
    {
        buzzerClose();
    }
}

void button1Fxn(PIN_Handle handle, PIN_Id pinId)
{
    if (COMTYPE == COMTYPE_UART)
    {
        COMTYPE = COMTYPE_LOWPAN;
        playTone(600, 250000 / Clock_tickPeriod, STATIC, 0, 0);
        System_printf("Switched communication type to 6LoWPAN\n");
        System_flush();
    }
    else
    {
        COMTYPE = COMTYPE_UART;
        playTone(400, 250000 / Clock_tickPeriod, STATIC, 0, 0);
        System_printf("Switched communication type to uart\n");
        System_flush();
    }
}

void powerFxn(PIN_Handle handle, PIN_Id pinId)
{
    System_printf("Going to sleep\n");
    System_flush();

    // Varmistus odottelu
    Task_sleep(500000 / Clock_tickPeriod);

    // Magiikkaa
    PIN_close(powerButtonHandle);
    PINCC26XX_setWakeup(powerButtonWakeConfig);
    Power_shutdown(0, 0);
}

void clkFxn(UArg arg0)
{
    if (programState == WAITING)
    {
        programState = READ_SENSOR;
    }
}

void uartReadFxn(UART_Handle handle, void *rxBuf, size_t len)
{
    programState = ANALYZE_MSG;
    UART_read(handle, rxBuf, 80);
}

// Tamagotchin tila
void analyzeMsg()
{
    if (strstr(readBuffer, "Food") != NULL)
    {
        playTone(20, 500000 / Clock_tickPeriod, STATIC, 0, 0);
        System_printf("PERKELE ETTÄ ON NÄLÄKÄ!\n"); // Alunperin tarkistukseen, mutta sitten saatiinkin nerokas idea...
        System_flush();
    }
    if (strstr(readBuffer, "Scratch") != NULL)
    {
        playTone(1000, 500000 / Clock_tickPeriod, FALLING, 250, 250000 / Clock_tickPeriod);
        System_printf("FUCKING HELL\n"); // Alunperin tarkistukseen, mutta sitten saatiinkin nerokas idea...
        System_flush();
    }
    if (strstr(readBuffer, "Severe") != NULL)
    {
        playTone(300, 1000000 / Clock_tickPeriod, FALLING, 100, 500000 / Clock_tickPeriod);
        System_printf("FAT AS HELL\n"); // Alunperin tarkistukseen, mutta sitten saatiinkin nerokas idea...
        System_flush();
    }
    if (strstr(readBuffer, "Too late") != NULL)
    {
        playTone(600, 1250000 / Clock_tickPeriod, FALLING, 100, 250000 / Clock_tickPeriod);
        System_printf("RIP\n"); // Alunperin tarkistukseen, mutta sitten saatiinkin nerokas idea...
        System_flush();
    }
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1)
{

    // UART-kirjaston asetukset
    UART_Handle uart;
    UART_Params uartParams;

    // Alustetaan sarjaliikenne
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readCallback = &uartReadFxn;
    uartParams.baudRate = 9600;            // nopeus 9600baudia
    uartParams.dataLength = UART_LEN_8;    // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE;   // 1

    System_printf("Opening uart\n");
    System_flush();

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL)
    {
        System_abort("Error opening the UART");
    }

    UART_read(uart, readBuffer, 80);

    System_printf("Opened uart\n");
    System_flush();

    while (1)
    {
        if (programState == SEND_MSG)
        {
            char final_msg[30];
            sprintf(final_msg, "id:236,%s\0", writeMsg);
            /*System_printf("Sending message: %s\n", final_msg);
            System_flush();*/
            if (COMTYPE == COMTYPE_UART)
            {
                UART_write(uart, final_msg, strlen(final_msg) + 1);
            }
            if (COMTYPE == COMTYPE_LOWPAN)
            {
                Send6LoWPAN(IEEE80154_SERVER_ADDR, final_msg, strlen(final_msg));
                StartReceive6LoWPAN();
            }
            memset(writeMsg, 0, 20);
            programState = WAITING;
            Task_sleep(200000 / Clock_tickPeriod);
        }
        Task_sleep(100000 / Clock_tickPeriod);
    }
}
void lowpanReadFxn(UArg arg0, UArg arg1)
{
    uint16_t senderAddr;
    int32_t result = StartReceive6LoWPAN();
    if (result != true)
    {
        System_abort("Wireless receive start failed");
    }
    while (1)
    {
        if (GetRXFlag() && COMTYPE == COMTYPE_LOWPAN)
        {
            memset(readBuffer, 0, 80); // Empty the read buffer
            Receive6LoWPAN(&senderAddr, readBuffer, 80);
            if (senderAddr == IEEE80154_SERVER_ADDR)
            { // If the message was received from the correct address, save the message and analyze it for relevant information
                programState = ANALYZE_MSG;
            }
        }
    }
}
void msgTaskFxn(UArg arg0, UArg arg1)
{
    while (1)
    {
        if (programState == ANALYZE_MSG)
        {
            if (COMTYPE == COMTYPE_UART && strstr(readBuffer, "236") != NULL)
            {
                analyzeMsg();
            }
            else if (COMTYPE == COMTYPE_LOWPAN)
            {
                analyzeMsg();
            }
            memset(readBuffer, 0, strlen(readBuffer));
            programState = WAITING;
        }
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

void sensorTaskFxn(UArg arg0, UArg arg1)
{

    // MPU9250 käyttöönotto

    float ax, ay, az, gx, gy, gz;

    I2C_Handle i2cMPU; // i2c-käyttöliittymä MPU sensorille
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    // Eri konffis alhaalla
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU käynnistyminen
    PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_ON);

    // 100ms viive käynnistymiselle
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU avaa i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL)
    {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU sensorin setuppi ja kalibraatio
    System_printf("MPU9250: Setup and calibration\n");
    System_flush();

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration done\n");
    System_flush();

    playTone(300, 750000 / Clock_tickPeriod, RISING, 100, 100000 / Clock_tickPeriod);

    // TASK LOOPPI

    while (1)
    {

        if (programState == READ_SENSOR)
        {

            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

            if (latestIndex <= 0)
            {
                latestIndex = SENSORDATAPOINTS - 1;
            }
            else
            {
                latestIndex--;
            }

            accelData[latestIndex].x = ax;
            accelData[latestIndex].y = ay;
            accelData[latestIndex].z = az;

            programState = ANALYZE_DATA;
        }

        Task_sleep(50000 / Clock_tickPeriod);
    }

    I2C_close(i2cMPU);
}

void analyzeTaskFxn(UArg arg0, UArg arg1)
{

    while (1)
    {

        char messageCoolDown = 0;
        if (programState == ANALYZE_DATA)
        {
            if (acs(accelData[latestIndex].x) > 0.5f) // Haluaa liikettä
            {
                if (acs(accelData[latestIndex].x - accelData[(latestIndex + 1) % SENSORDATAPOINTS].x) > 0.7f || acs(accelData[(latestIndex + 1) % SENSORDATAPOINTS].x - accelData[(latestIndex + 2) % SENSORDATAPOINTS].x) > 0.7f)
                {
                    if (acs(accelData[(latestIndex + 2) % SENSORDATAPOINTS].x - accelData[(latestIndex + 3) % SENSORDATAPOINTS].x) > 0.7f || acs(accelData[(latestIndex + 3) % SENSORDATAPOINTS].x - accelData[(latestIndex + 4) % SENSORDATAPOINTS].x) > 0.7f)
                    {
                        messageCoolDown = 1;
                        strcpy(writeMsg, "EXERCISE:1");
                    }
                }
            }
            else if (acs(accelData[(latestIndex + 1) % SENSORDATAPOINTS].y) > 0.7f)
            {
                messageCoolDown = 1;
                strcpy(writeMsg, "EAT:1");
            }
            else if (acs(accelData[(latestIndex + 3) % SENSORDATAPOINTS].z + 1.0f) > 0.7f)
            {
                if (acs(accelData[(latestIndex + 4) % SENSORDATAPOINTS].z + 1.0f) < 0.6f || acs(accelData[(latestIndex + 5) % SENSORDATAPOINTS].z + 1.0f) < 0.6f)
                {
                    messageCoolDown = 1;
                    strcpy(writeMsg, "PET:1");
                }
            }
            programState = WAITING;
        }

        if (messageCoolDown == 0)
        {
            Task_sleep(100000 / Clock_tickPeriod);
        }
        else
        {
            programState = SEND_MSG;
            playTone(750, 250000 / Clock_tickPeriod, RISING, 250, 125000 / Clock_tickPeriod);
            Task_sleep(1000000 / Clock_tickPeriod);
        }
    }
}

/* Main Function */

int main(void)
{

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;
    Task_Handle analyzeTaskHandle;
    Task_Params analyzeTaskParams;
    Task_Handle commTaskHandle;
    Task_Params commTaskParams;
    Task_Handle msgTaskHandle;
    Task_Params msgTaskParams;

    // Clock variables
    Clock_Handle clkHandle;
    Clock_Params clkParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // Initialize i2c bus

    Board_initI2C();

    // Initialize UART

    // Otetaan sarjaportti käyttöön ohjelmassa
    Board_initUART();

    // Open power button pin
    powerButtonHandle = PIN_open(&powerButtonState, powerButtonConfig);
    if (!powerButtonHandle)
    {
        System_abort("Error initializing power button\n");
    }
    if (PIN_registerIntCb(powerButtonHandle, &powerFxn) != 0)
    {
        System_abort("Error registering power button callback");
    }

    buzzerHandle = PIN_open(&buzzerState, buzzerConfig);
    if (!buzzerHandle)
    {
        System_abort("Error initializing buzzer pins\n");
    }

    // Open the pins
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle)
    {
        System_abort("Error initializing button pins\n");
    }

    // Painonappi aborttia varten
    // funktio button1Fxn
    if (PIN_registerIntCb(buttonHandle, &button1Fxn) != 0)
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

    Task_Params_init(&analyzeTaskParams);
    analyzeTaskParams.stackSize = STACKSIZE;
    analyzeTaskParams.stack = &analyzeTaskStack;
    analyzeTaskParams.priority = 2;
    analyzeTaskHandle = Task_create(analyzeTaskFxn, &analyzeTaskParams, NULL);
    if (analyzeTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority = 1;
    commTaskHandle = Task_create(lowpanReadFxn, &commTaskParams, NULL);
    if (commTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    Task_Params_init(&msgTaskParams);
    msgTaskParams.stackSize = STACKSIZE;
    msgTaskParams.stack = &msgTaskStack;
    msgTaskParams.priority = 2;
    msgTaskHandle = Task_create(msgTaskFxn, &msgTaskParams, NULL);
    if (msgTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    // Initialize Clock
    Clock_Params_init(&clkParams);
    clkParams.period = 100000 / Clock_tickPeriod;
    clkParams.startFlag = TRUE;

    clkHandle = Clock_create((Clock_FuncPtr)clkFxn, 500000 / Clock_tickPeriod, &clkParams, NULL);
    if (clkHandle == NULL)
    {
        System_abort("Clock create failed");
    }

    // Initialize Buzzer Clock
    Clock_Params_init(&buzzerClkParams);
    buzzerClkParams.startFlag = false;

    buzzerClkHandle = Clock_create((Clock_FuncPtr)buzzerClockFxn, 0, &buzzerClkParams, NULL);
    if (buzzerClkHandle == NULL)
    {
        System_abort("Clock create failed");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
