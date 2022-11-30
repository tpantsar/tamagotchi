/* JTKJ Harjoitustyö 2022
    Tomi Pantsar
    Santeri Heikkinen
*/

/* C Standard library */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* XDCtools files */
#include <xdc/runtime/System.h>
#include <xdc/std.h>

/* BIOS Header files */
#include <driverlib/timer.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files (PWM_Led_example)
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>
*/

/* Board Header files */
#include "Board.h"
#include "buzzer.h"
#include "sensors/mpu9250.h"
#include "sensors/opt3001.h"
#include "sensors/tmp007.h"
#include "wireless/comm_lib.h"

void sendMsg(UART_Handle handle, char *msg, int length);

// Merkkijonot taustajärjestelmää varten
char msgEat[25] = "id:2230,EAT:2";
char msgPet[25] = "id:2230,PET:2";
char msgExercise[25] = "id:2230,EXERCISE:2";
char msgActivate[25] = "id:2230,ACTIVATE:2;2;2";
char msgWarning[45] = "id:2230,MSG1:Warning!";
char msgGameOver[25] = "id:2230,MSG1:Game Over!";

#define RXBUFLEN 80

/* Task */
#define STACKSIZE 4096
char sensorTaskStack[STACKSIZE];
char uartTaskStack[STACKSIZE];

uint8_t buffCount = 0;
char uartStr[RXBUFLEN];  // MSG1 taustajarjestelmasta
char uartStr2[RXBUFLEN]; // MSG2 taustajarjestelmasta

// Tilakone Tamagotchin toiminnoille
enum state {
    WAITING = 1,
    DATA_READY,
    EAT,      // ravinto
    PET,      // hoiva
    EXERCISE, // liikunta
    ACTIVATE, // EAT, PET, EXERCISE
    GAMEOVER  // Pelin paattyminen
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum state programState = WAITING;

// Tilakone SensorTagin toiminnoille
enum tila {
    TILA_0 = 1,
    TILA_PAIKALLAAN, // liikesensori
    TILA_LIIKKUU,    // liikesensori
    TILA_VALO,       // valosensori
    TILA_PIMEA,      // valosensori
};
// Globaali tilamuuttuja, alustetaan odotustilaan
enum tila sisainenState = TILA_0;

// Globaalit muuttujat Tamagotchin ominaisuuksille
double ambientLight = -1000.0;
int ravinto = 5;
int hoiva = 5;
int liikunta = 5;

/* PINNIEN MUUTTUJAT JA ALUSTUKSET */
static PIN_Handle MpuPinHandle;
static PIN_State MpuPinState;
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle LED0_Handle;
static PIN_State LED0_State;
static PIN_Handle LED1_Handle;
static PIN_State LED1_State;
static PIN_Handle buzzerHandle;
static PIN_State buzzerState;

// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUConfig = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

// Painonappi
PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Vihrea ledi
PIN_Config LED0_ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Punainen ledi
PIN_Config LED1_ledConfig[] = {
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

// Buzzer config
PIN_Config buzzerConfig[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

/* Napinpainalluksen keskeytyksen käsittelijäfunktio */
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    /*char debug[30];
    sprintf(debug, "button: %s\n", handle);
    System_printf(debug);
    System_flush();

    while (handle)
    {
        printf("\nnappi pohjassa");
    }*/
    if (ravinto < 10) {
        ravinto++;
        System_printf("\nEAT\n"); // Debug
        System_flush();
    } else {
        System_printf("\nMaha taynna!\n"); // Debug
        System_flush();
    }
    programState = EAT;
}

// Musiikki hoivalle
void buzzerPet(void) {
    /*
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
    */
}

// Musiikki liikunnalle
void buzzerExercise(void) {
    /*
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
    */
}

// Musiikki varoitukselle
void buzzerWarning(void) {
    /*
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
    */
}

// Musiikki pelin loppumiselle
void buzzerGameOver(void) {
    /*
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
    */
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1) {

    Task_sleep(100000 / Clock_tickPeriod);
    char echo_msg[50];

    UART_Handle uart;
    UART_Params uartParams;

    // UARTin alustus: 9600,8n1
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    // uartParams.readCallback = &uartFxn;
    uartParams.baudRate = 9600;            // nopeus 9600 baud
    uartParams.dataLength = UART_LEN_8;    // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE;   // 1

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening the UART");
    }

    while (1) {
        // System_printf("\nuartTask\n");
        // System_flush();

        if (programState == DATA_READY) {
            programState = WAITING;
        }

        else if (programState == WAITING) {
            programState = WAITING;
        }

        // EAT (ravinto, toimii painonapilla!)
        else if (programState == EAT) {
            sendMsg(uart, msgEat, strlen(msgEat)); // Testi

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
        }

        // PET (hoiva)
        else if (programState == PET) {
            sendMsg(uart, msgPet, strlen(msgPet)); // Testi

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
        }

        // EXERCISE (liikunta)
        else if (programState == EXERCISE) {
            sendMsg(uart, msgExercise, strlen(msgExercise)); // Testi

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
        }

        // ACTIVATE
        else if (programState == ACTIVATE) {
            sendMsg(uart, msgActivate, strlen(msgActivate)); // Testi

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);
            programState = WAITING;
        }

        // GAMEOVER
        else if (programState == GAMEOVER) {
            // Molemmat ledit päälle
            PIN_setOutputValue(LED0_Handle, Board_LED0, 0);
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);

            sendMsg(uart, msgGameOver, strlen(msgGameOver)); // Testi

            buzzerGameOver(); // musiikki
        }

        // Taski nukkumaan
        Task_sleep(2000000 / Clock_tickPeriod);
    }
}

void sensorTaskFxn(UArg arg0, UArg arg1) {
    // RTOS:n i2c-muuttujat ja alustus
    I2C_Handle i2c;
    I2C_Params i2cParams;
    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    // Merkkijonomuuttuja tulostuksille
    char debug_msg[100];

    // Kiihtyvyys- ja asentoanturin muuttujat
    float ax, ay, az, gx, gy, gz;

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // Alustetaan i2cMPU-väylä
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;

    // Note the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUConfig;

    // MPU power on
    PIN_setOutputValue(MpuPinHandle, Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU setup and calibration
    Task_sleep(100000 / Clock_tickPeriod);
    mpu9250_setup(&i2cMPU);
    I2C_close(i2cMPU);

    // OPT3001 open i2c
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    // OPT3001 setup and calibration
    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);
    I2C_close(i2c);

    while (1) {
        sprintf(debug_msg, "\n\nprogramState: %d\n", programState);
        System_printf(debug_msg);
        System_flush();

        if (programState == WAITING) {
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

        if (programState == DATA_READY) {
            sisainenState = TILA_0;
        }

        // sensorTag on liikkeessä
        if (sisainenState == TILA_0 && ax < -0.5) {
            sisainenState = TILA_LIIKKUU;
        }

        // sensorTag on paikallaan
        if ((sisainenState == TILA_0) && (ax < 5) && (ay < 10) && (az < 5)) {
            sisainenState = TILA_PAIKALLAAN;
        }

        // EXERCISE
        if (sisainenState == TILA_LIIKKUU && liikunta < 10) {
            liikunta++;
            sprintf(debug_msg, "%s", "\nEXERCISE");
            System_printf(debug_msg);
            System_flush();
            buzzerExercise(); // musiikki
            programState = EXERCISE;
        }

        // sensorTag on pimeässä
        if (sisainenState == TILA_PAIKALLAAN && ambientLight < 15) {
            sisainenState = TILA_PIMEA;
        }

        // sensorTag on valoisassa
        if (sisainenState == TILA_PAIKALLAAN && ambientLight > 150) {
            sisainenState = TILA_VALO;
        }

        // PET
        if (sisainenState == TILA_PIMEA && hoiva < 10) {
            // hoiva++;
            sprintf(debug_msg, "%s", "\nPET");
            System_printf(debug_msg);
            System_flush();
            buzzerPet(); // musiikki
            programState = PET;
            sisainenState = TILA_0;
        }

        // ACTIVATE
        if (sisainenState == TILA_VALO) {
            sprintf(debug_msg, "%s", "\nACTIVATE");
            System_printf(debug_msg);
            System_flush();
            if (ravinto < 10) {
                ravinto++;
            }
            if (hoiva < 10) {
                hoiva++;
            }
            if (liikunta < 10) {
                liikunta++;
            }
            programState = ACTIVATE;
        }

        // WARNING
        if (ravinto == 2 || hoiva == 2 || liikunta == 2) {
            sprintf(debug_msg, "%s", "WARNING\n");
            System_printf(debug_msg);
            System_flush();

            // Punainen ledi päälle
            PIN_setOutputValue(LED1_Handle, Board_LED1, 1);

            buzzerWarning(); // musiikki

            // Punainen ledi pois
            PIN_setOutputValue(LED1_Handle, Board_LED1, 0);

            sisainenState = TILA_0;
        }

        // GAMEOVER
        if (ravinto == 0 || hoiva == 0 || liikunta == 0) {
            sprintf(debug_msg, "%s", "GAMEOVER\n");
            System_printf(debug_msg);
            System_flush();

            programState = GAMEOVER;
            sisainenState = TILA_0;
        }

        // Tulostaa tamagotchin ominaisuudet
        debugFxn();

        // Taski nukkumaan
        Task_sleep(2000000 / Clock_tickPeriod);
    }
    // PIN_setOutputValue(MpuPinHandle, Board_MPU_POWER, Board_MPU_POWER_OFF);
}

// Tulostaa kaikki tamagotchin ominaisuudet konsoliin
void debugFxn(void) {
    System_printf("\nravinto: %d", ravinto);
    System_printf("\nhoiva: %d", hoiva);
    System_printf("\nliikunta: %d", liikunta);
}

/*
// Taskifunktio
void serialTask(UArg arg0, UArg arg1) {

    char input;
    char echo_msg[30];

    // UART-kirjaston asetukset
    UART_Handle uart;
    UART_Params uartParams;

    // Alustetaan sarjaliikenne
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;            // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8;    // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE;   // 1

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening the UART");
    }

    // Ikuinen elämä
    while (1) {
        // Vastaanotetaan 1 merkki kerrallaan input-muuttujaan
        UART_read(uart, &input, 1);

        // Lähetetään merkkijono takaisin
        sprintf(echo_msg, "Received: %c\n", input);
        UART_write(uart, echo_msg, strlen(echo_msg));

        // Kohteliaasti nukkumaan sekunniksi
        Task_sleep(1000000L / Clock_tickPeriod);
    }
}*/

void uartFxn(UART_Handle handle, void *rxBuf, size_t len) {
    if (buffCount < RXBUFLEN) {
        strncat(uartStr, rxBuf, RXBUFLEN);
        buffCount++;
    } else if (buffCount < (RXBUFLEN * 2)) {
        strncat(uartStr2, rxBuf, RXBUFLEN);
        buffCount++;
    }
    UART_read(handle, rxBuf, 1);
}

// Viestitys uartilla
void sendMsg(UART_Handle handle, char *msg, int length) {
    UART_write(handle, msg, length + 1);
}

int main(void) {
    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();
    Board_initUART();

    // Tiedonsiirto alustetaan
    Init6LoWPAN();

    // Open MPU power pin
    MpuPinHandle = PIN_open(&MpuPinState, MpuPinConfig);
    if (MpuPinHandle == NULL) {
        System_abort("Pin open failed!");
    }

    // Open buzzer pin
    buzzerHandle = PIN_open(&buzzerState, buzzerConfig);
    if (buzzerHandle == NULL) {
        System_abort("Pin open failed!");
    }

    // Painonappi käyttöön ohjelmassa
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle) {
        System_abort("Error initializing button pins\n");
    }

    // Ledi käyttöön ohjelmassa
    LED0_Handle = PIN_open(&LED0_State, LED0_ledConfig);
    if (!LED0_Handle) {
        System_abort("Error initializing LED pins\n");
    }

    // Asetetaan painonapille keskeytyksen käsittelijä (funktio buttonFxn)
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
        System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
