/*
 * project_main.c
 *
 * Authors: Lotta Rantala and Venla Katainen
 *
 */

/* C Standard library */
#include <stdio.h>
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
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <driverlib/timer.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/buzzer.h"
#include "sensors/mpu9250.h"
#include "sensors/tmp007.h"

void musiikkiFunktioLiikunta(void);
void musiikkiFunktioEnergia(void);
void musiikkiFunktioVaroitus(void);
void musiikkiFunktioAktivointi(void);

/* Task */
#define STACKSIZE 4096
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

// Tilakone
enum state
{
    WAITING = 1, // DATA_READ
    DATA_READY,
    RUOKINTA_TILA,
    LIIKUNTA_TILA,
    ENERGIA_TILA,
    HOIVA_TILA,
    AKTIVOINTI_TILA,
    KARKAAMINEN
};
enum state programState = WAITING; // DATA_READ

// Sisainen tilakone
enum tila
{
    TILA_0 = 1,
    TILA_PAIKALLAAN,
    TILA_LIIKKUU,
    TILA_VALO,
    TILA_PIMEA,
    TILA_LAMPO
};
enum tila sisainenState = TILA_0;

// Global variables for pins
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle punainen_ledHandle;
static PIN_State punainen_ledState;
static PIN_Handle buzzer;
static PIN_State buzzerState;
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Handle powerButtonHandle;
static PIN_State powerButtonState;

// Global variables
double ambientLight = -1000.0;
double lampotila = 0.0;
int ravinto = 3;
int liikunta = 3;
int hoiva = 3;
int energia = 3;
int aktivoi = 3;

// Pin configs
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

PIN_Config punainen_ledConfig[] = {
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

PIN_Config buzzerConfig[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, /* Buzzer initially off          */
    PIN_TERMINATE};

PIN_Config powerButtonConfig[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

PIN_Config powerButtonWakeConfig[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE};

void powerFxn(PIN_Handle handle, PIN_Id pinId)
{
    // Odotetana hetki ihan varalta..
    Task_sleep(100000 / Clock_tickPeriod);

    // Taikamenot
    PIN_close(powerButtonHandle);
    PINCC26XX_setWakeup(powerButtonWakeConfig);
    Power_shutdown(NULL, 0);
}

void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    // kun painetaan nappia, tamagotchia ruokitaan
    if (ravinto < 10)
    {
        ravinto += 1;
    }
    programState = RUOKINTA_TILA;
}

void musiikkiFunktioLiikunta(void)
{
    // kun tamagotchi liikkuu, soi tama musiikki
    buzzerOpen(buzzer);
    buzzerSetFrequency(262); // c
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // d
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(330); // e
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(349); // f
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(1000000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(1000000 / Clock_tickPeriod);
    buzzerClose();
}

void musiikkiFunktioEnergia(void)
{
    // kun tamagotchi nukkuu soi tama musiikki
    buzzerOpen(buzzer);
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

void musiikkiFunktioVaroitus(void)
{
    // kun jokin arvo on alle 2, tamagotchi lahettaa taman varoitus aanen
    buzzerOpen(buzzer);
    buzzerSetFrequency(277);
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(185);
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerClose();
}

void musiikkiFunktioAktivointi(void)
{
    // kun tamagotchia aktivoidaan soi tama musiikki
    buzzerOpen(buzzer);
    buzzerSetFrequency(494); // h
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(494); // h
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(250000 / Clock_tickPeriod);
    buzzerSetFrequency(494); // h
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // a
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerSetFrequency(392); // g
    Task_sleep(500000 / Clock_tickPeriod);
    buzzerClose();
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1)
{
    Task_sleep(100000 / Clock_tickPeriod);
    char viesti[8];

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

        else if (programState == RUOKINTA_TILA)
        {
            // tamagotchi syo
            sprintf(viesti, "%s", "ping");
            Send6LoWPAN(IEEE80154_SERVER_ADDR, viesti, strlen(viesti));
            StartReceive6LoWPAN();

            PIN_setOutputValue(ledHandle, Board_LED0, 1);
            programState = WAITING;
        }

        else if (programState == LIIKUNTA_TILA)
        {
            // tamagotchi liikkuu
            programState = WAITING;
        }

        else if (programState == HOIVA_TILA)
        {
            // punainen led syttyy hoivatilan merkiksi
            PIN_setOutputValue(punainen_ledHandle, Board_LED1, 1);
            programState = WAITING;
        }

        else if (programState == ENERGIA_TILA)
        {
            // tamagotchi nukkuu jonka jalkeen data read tilaan
            programState = WAITING;
        }

        else if (programState == AKTIVOINTI_TILA)
        {
            // tamagotchia aktivoidaan jonka jalkeen siirrytaan data read tilaan
            System_printf("Aktivointitilassa whilessa");
            System_flush();
            programState = WAITING;
        }

        else if (programState == KARKAAMINEN)
        {
            // tamagotchi karkaa
            PIN_setOutputValue(punainen_ledHandle, Board_LED1, 1);
            PIN_setOutputValue(ledHandle, Board_LED0, 1);
        }

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

void sensorTaskFxn(UArg arg0, UArg arg1)
{
    // valosensori
    I2C_Handle i2c;
    I2C_Params i2cParams;
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // liikesensori
    float ax, ay, az, gx, gy, gz;

    I2C_Handle i2cMPU; // liikesensorin oma i2c vayla
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;

    // Note the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // tulostettavien merkkijonojen alustus
    char merkkijono[50];
    char merkkijono_liike[100];
    char toinen_merkkijono[100];
    char hoiva_merkkijono[20];
    char energia_merkkijono[20];
    char lampotilamerkkijono[20];

    // MPU setup and calibration
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();
    I2C_close(i2cMPU);

    // valosensorin i2c vaylan avaus
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // OPT3001 setup and calibration
    System_printf("OPT3001: Setup and calibration...\n");
    System_flush();
    opt3001_setup(&i2c);
    System_printf("OPT3001: Setup and calibration OK\n");
    System_flush();
    I2C_close(i2c);

    // lampotilasensorin i2c vaylan avaus
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // TMP007 setup and calibration
    System_printf("TMP007: Setup and calibration...\n");
    System_flush();
    tmp007_setup(&i2c);
    System_printf("TMP007: Setup and calibration OK\n");
    System_flush();
    I2C_close(i2c);

    while (1)
    {
        sprintf(merkkijono, "%d\n", programState);
        System_printf(merkkijono);
        System_flush();

        if (programState == WAITING)
        {
            PIN_setOutputValue(punainen_ledHandle, Board_LED1, 0);
            PIN_setOutputValue(ledHandle, Board_LED0, 0);

            // avataan valosensorin i2c vayla
            i2c = I2C_open(Board_I2C, &i2cParams);

            // kerataan dataa valosensorilta
            ambientLight = opt3001_get_data(&i2c);

            // suljetaan valosensorin i2c vayla
            I2C_close(i2c);
            sprintf(merkkijono, "%f\n", ambientLight);
            System_printf(merkkijono);
            System_flush();

            // avataan lampotilasensorin i2c vayla
            i2c = I2C_open(Board_I2C, &i2cParams);

            // kerataan dataa lampotilasensorilta
            lampotila = tmp007_get_data(&i2c);

            // suljetaan lampotilasensorin i2c vayla
            I2C_close(i2c);

            // avataan liikesensorin i2c vayla
            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);

            // kerataan dataa liikesensorilta
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

            // suljetaan liikesensorin i2c vayla
            I2C_close(i2cMPU);

            sprintf(merkkijono_liike, "%lf, %lf, %lf, %lf, %lf, %lf\n", ax, ay, az, gx, gy, gz);
            System_printf(merkkijono_liike);
            System_flush();

            sprintf(lampotilamerkkijono, "%lf\n", lampotila);
            System_printf(lampotilamerkkijono);
            System_flush();

            // asetetaan tilakone tilaan data_ready, kun sensoreiden mittausdata on saatu
            programState = DATA_READY;
        }

        if (programState == DATA_READY)
        {
            // asetetaan sisainen tilakone alkutilaa, kun data on valmis
            sisainenState = TILA_0;
        }

        if (ax < -0.5 && sisainenState == TILA_0)
        {
            // sensor tag on liikkeessa, joten vaihdetaan tilaa
            sisainenState = TILA_LIIKKUU;
        }

        if (sisainenState == TILA_0 && ax < 5 && ay < 10 && az < 5)
        {
            // sensor tag ei liiku
            sisainenState = TILA_PAIKALLAAN;
        }

        if (liikunta < 10 && sisainenState == TILA_LIIKKUU)
        {
            // tunnistettu, että tamagotchi on liikuntatilassa
            liikunta += 1;
            sprintf(toinen_merkkijono, "liikuntaa: %d\n", liikunta);
            System_printf(toinen_merkkijono);
            System_flush();
            musiikkiFunktioLiikunta();
            programState = LIIKUNTA_TILA;
        }

        if (sisainenState == TILA_PAIKALLAAN && lampotila > 35.0)
        {
            // sensor tagilla mitattu lampotila on haluttu
            sisainenState = TILA_LAMPO;
        }

        if (sisainenState == TILA_LAMPO && hoiva < 10)
        {
            // tamagotchia hoivataan
            hoiva += 1;
            sprintf(hoiva_merkkijono, "hoiva: %d\n", hoiva);
            System_printf(hoiva_merkkijono);
            System_flush();
            programState = HOIVA_TILA;
        }

        if (ambientLight < 15 && sisainenState == TILA_PAIKALLAAN)
        {
            // sensor tag on pimeassa
            sisainenState = TILA_PIMEA;
        }

        if (sisainenState == TILA_PIMEA && energia < 10)
        {
            // tamagotchi nukkuu
            energia += 1;
            System_printf("asetetaan energiatila\n");
            sprintf(energia_merkkijono, "energia: %d\n", energia);
            System_printf(energia_merkkijono);
            System_flush();
            musiikkiFunktioEnergia();
            programState = ENERGIA_TILA;
            sisainenState = TILA_0;
        }

        if (sisainenState == TILA_PAIKALLAAN && ambientLight > 100)
        {
            // sensor tag on valoisassa
            sisainenState = TILA_VALO;
        }

        if (sisainenState == TILA_VALO && aktivoi < 10)
        {
            // tamagotchia aktivoidaan
            musiikkiFunktioAktivointi();
            System_printf("aktivointi tila\n");
            System_flush();
            programState = AKTIVOINTI_TILA;

            if (hoiva < 10)
            {
                hoiva++;
            }

            if (ravinto < 10)
            {
                ravinto++;
            }

            if (liikunta < 10)
            {
                liikunta++;
            }
        }

        if (energia == 0 || ravinto == 0 || hoiva == 0 || liikunta == 0)
        {
            // tamagotchi karkaa
            programState = KARKAAMINEN;
            sisainenState = TILA_0;
        }

        if (energia == 2 || ravinto == 2 || hoiva == 2 || liikunta == 2)
        {
            // tamagotchi varoittaa karkaamisesta
            musiikkiFunktioVaroitus();
            sisainenState = TILA_0;
        }

        Task_sleep(1000000 / Clock_tickPeriod);
    }

    // liikesensori pois paalta
    PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_OFF);
}

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
    Init6LoWPAN();

    // otetaan i2c vayla kayttoon
    Board_initI2C();

    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL)
    {
        System_abort("Pin open failed!");
    }

    // otetaan painonappi ja led kayttoon
    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle)
    {
        System_abort("Error initializing Led pin\n");
    }

    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle)
    {
        System_abort("Error initializing button pin\n");
    }

    // virtakytkimen kayttoonotto
    powerButtonHandle = PIN_open(&powerButtonState, powerButtonConfig);
    if (!powerButtonHandle)
    {
        System_abort("Error initializing power button\n");
    }
    if (PIN_registerIntCb(powerButtonHandle, &powerFxn) != 0)
    {
        System_abort("Error registering power button callback");
    }

    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0)
    {
        System_abort("Error registering button callback function\n");
    }

    buzzer = PIN_open(&buzzerState, buzzerConfig);
    if (buzzer == NULL)
    {
        System_abort("Pin open failed!");
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

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority = 1;
    commTaskHandle = Task_create(commTask, &commTaskParams, NULL);
    if (commTaskHandle == NULL)
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