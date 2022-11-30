/* JTKJ 2022 Harjoitustyö TAMAGOTCHI
 * Heikki Marjala
 * Arrtu Niemensivu
 * Jere Viitasalo
 */

/* C Standard library */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* XDCtools files */
#include <xdc/runtime/System.h>
#include <xdc/std.h>

/* BIOS Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>

/* Board Header files */
#include "Board.h"
#include "sensors/buzzer.h"
#include "sensors/mpu9250.h"
#include "sensors/opt3001.h"
#include "sensors/tmp007.h"
#include "wireless/address.h"
#include "wireless/comm_lib.h"

/* mylib */
#include "mylib/config.h"
#include "mylib/display_helper.h"
#include "mylib/mylib.h"

#define STACKSIZE05K 512
#define STACKSIZE1K 1024
#define STACKSIZE2K 2048

#define SAMPLES 100
#define MS 1000
#define MEAS_SLEEP 10
#define RXBUFLEN 80

#define DAY 100.0
#define COLD 30.0

// BUZZER CONFIG
static PIN_Handle buzzerHandle;

PIN_Config Buzzer[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

// COMM TASK STRUCT
Task_Struct commStruct;
char commStack[STACKSIZE2K];

double sensordata[SAMPLES][3];

char menuTaskStack[STACKSIZE2K];
char sensorTaskStack[STACKSIZE2K];
char buzzTaskStack[STACKSIZE05K];
char musicTaskStack[STACKSIZE1K];
char displayTaskStack[STACKSIZE2K];

double last_lux;
double last_temp;
uint8_t tempUpdateFlag = 0;
uint8_t luxUpdateFlag = 0;

enum command {
    PET = 0,
    EXERCISE,
    EAT,
    ACTIVATE
};
enum state {
    STOP = 0,
    WAITING,
    READ_DATA
};
enum state programState = STOP;

/* FLAGS */
uint8_t petNeedFlags[6] = {0};
uint8_t playBuzzFlag = 0;
uint8_t playMusicFlag = 0;
uint8_t newAlertFlag = 0;

uint8_t buzzPetCmd = 0;
uint8_t buzzExerCmd = 0;
uint8_t buzzEatCmd = 0;

uint8_t buzzPetAlert = 0;
uint8_t buzzExerAlert = 0;
uint8_t buzzEatAlert = 0;

uint8_t buzzPetFull = 0;
uint8_t buzzExerFull = 0;
uint8_t buzzEatFull = 0;

/* function prototypes */
float timeDelta(uint32_t t0, uint32_t t1);

static PIN_Handle hMpuPin;

// I2CMPU 9250 pin config
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

// Button 0 pin handle
static PIN_Handle button0Handle;
static PIN_State button0State;

// Button 0 pin config
PIN_Config button0Config[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

// Button 1 pin handle
static PIN_Handle button1Handle;
static PIN_State button1State;

// Button 1 pin config
PIN_Config button1Config[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

void displayTask(UArg arg0, UArg arg1) {
    Display_Params params;
    Display_Params_init(&params);
    params.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Handle displayHandle = Display_open(Display_Type_LCD, &params);

    if (displayHandle == NULL) {
        System_abort("Error initializing LCD Screen\n");
    }

    struct displayText mainMenu = createMainMenu();
    // struct displayText gameMenu = createGameMenu();
    // struct displayText winMenu  = createWinMenu();
    // struct displayText lostMenu = createLostMenu();
    // helpMenu = createHelpMenu();
    // scoreMenu = createScoreMenu();
    // quitMenu = createQuitMenu();

    struct displayText *currentMenu = &mainMenu;
    uint8_t i;
    for (i = 0; i < 12; i++)
        Display_print0(displayHandle, i, 0, currentMenu->row[i]);
    uint8_t cursorPosition = 0;
    while (1) {
        updateMenu(currentMenu, cursorPosition++);
        for (i = 0; i < 12; i++)
            Display_print0(displayHandle, i, 0, currentMenu->row[i]);
        Task_sleep(5000 * MS / Clock_tickPeriod);
    }
}

void buzzer(uint16_t freq, uint16_t count, uint16_t step) {
    uint16_t i = 0;
    for (i; i < count; i++) {
        buzzerOpen(buzzerHandle);
        buzzerSetFrequency(freq);
        Task_sleep(step * MS / Clock_tickPeriod);
        buzzerClose();
        Task_sleep(step * MS / Clock_tickPeriod);
    }
}

void sendCmd(uint8_t cmd, uint8_t amount) {
    uint8_t *msg = cmdStr(cmd, amount);
    if (msg == NULL) {
        return;
    }

    Send6LoWPAN(IEEE80154_SERVER_ADDR, msg, strlen(msg));
    // set radio back to receiving mode
    StartReceive6LoWPAN();
    if (msg != NULL) {
        free(msg);
    }
}

void sendCustomMsg(uint8_t cmd, uint8_t status) {
    uint8_t *msg = customMsg(cmd, status);
    if (msg == NULL) {
        return;
    }

    Send6LoWPAN(IEEE80154_SERVER_ADDR, msg, strlen(msg));
    // set radio back to receiving mode
    StartReceive6LoWPAN();
    if (msg != NULL) {
        free(msg);
    }
}

void updatePetNeedFlags(uint8_t actions) {
    if (actions & 0x1) {
        buzzPetAlert = 1;
        ++petNeedFlags[0];
        System_printf("NEW ALERT:PET!\n");
        System_flush();
    }
    if (actions & 0x2) {
        buzzExerAlert = 1;
        ++petNeedFlags[1];
        System_printf("NEW ALERT:EXERCISE!\n");
        System_flush();
    }
    if (actions & 0x4) {
        buzzEatAlert = 1;
        ++petNeedFlags[2];
        System_printf("NEW ALERT:EAT!\n");
        System_flush();
    }
    if (actions & 0x8) {
        buzzPetFull = 1;
        ++petNeedFlags[3];
    }
    if (actions & 0x10) {
        buzzExerFull = 1;
        ++petNeedFlags[4];
    }
    if (actions & 0x20) {
        buzzEatFull = 1;
        ++petNeedFlags[5];
    }
    if (buzzPetFull || buzzExerFull || buzzEatFull) {
        playMusicFlag = 1;
        buzzPetFull = 0;
        buzzExerFull = 0;
        buzzEatFull = 0;
    }
}

void menuTaskFxn(UArg arg0, UArg arg1) {
    while (1) {
        if ((programState == WAITING) || (programState == READ_DATA)) {
            if (luxUpdateFlag) {
                luxUpdateFlag = 0;
                if (last_lux > DAY) {
                    sendCustomMsg(0, 0);
                } else {
                    sendCustomMsg(0, 1);
                }
            }
            if (tempUpdateFlag) {
                tempUpdateFlag = 0;
                if (last_temp > COLD) {
                    sendCustomMsg(1, 2);
                } else {
                    sendCustomMsg(1, 3);
                }
            }
        }
        Task_sleep(5000 * MS / Clock_tickPeriod);
    }
}

/* SENSOR TASK */
void sensorTaskFxn(UArg arg0, UArg arg1) {
    float ax, ay, az, gx, gy, gz;
    double Z_TRESHOLD = 0.25;
    double ACC_TRESHOLD = 0.15;
    double temp = 0;
    double lux = 0;

    /* MPU 9250 INIT */
    // I2CMPU parameters and handle
    I2C_Handle i2cMPU;
    I2C_Params i2cMPUParams;
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // open I2CMPU
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // POWER ON MPU
    PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_ON);
    // Sleep 100 ms
    Task_sleep(100 * MS / Clock_tickPeriod);
    // init I2CMPU9250
    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    // close I2CMPU
    I2C_close(i2cMPU);
    // I2C parameters and handle

    I2C_Handle i2c;
    I2C_Params i2cParams;
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // open I2C
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C for temp007 and OPT3001\n");
    }
    tmp007_setup(&i2c);
    opt3001_setup(&i2c);

    // close I2C
    I2C_close(i2c);

    System_printf("OPT3001 AND TMP007 Setup and calibration OK\n");
    System_flush();

    /*  SENSOR LOOP */
    while (1) {
        i2c = I2C_open(Board_I2C, &i2cParams);
        // temp = tmp007_get_data(&i2c);
        // lux = opt3001_get_data(&i2c);
        I2C_close(i2c);
        if ((temp > 0.0) && (temp != last_temp)) {
            last_temp = temp;
            tempUpdateFlag = 1;
        }
        if ((lux > 0.0) && (lux != last_lux)) {
            last_lux = lux;
            luxUpdateFlag = 1;
        }

        if (programState == READ_DATA) {
            double axPositiveSum = 0;
            double axPositiveCount = 0;
            double ayPositiveSum = 0;
            double ayPositiveCount = 0;
            double azPositiveSum = 0;
            double azPositiveCount = 0;

            // read sensor data
            uint8_t i;
            buzzer(2500, 2, 150);
            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
            
            for (i = 0; i < SAMPLES; i++) {
                mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
                sensordata[i][0] = ax;
                sensordata[i][1] = ay;
                sensordata[i][2] = az;
                Task_sleep(MEAS_SLEEP * MS / Clock_tickPeriod);

                if (ax > 0) {
                    axPositiveSum = axPositiveSum + ax;
                    axPositiveCount++;
                }
                if (ay > 0) {
                    ayPositiveSum = ayPositiveSum + ay;
                    ayPositiveCount++;
                }
                if (az > -1) {
                    azPositiveSum = azPositiveSum + az + 1;
                    azPositiveCount++;
                }
            }
            I2C_close(i2cMPU);
            double axPositiveAvg = axPositiveSum / SAMPLES;
            double ayPositiveAvg = ayPositiveSum / SAMPLES;
            double azPositiveAvg = azPositiveSum / SAMPLES;
            uint8_t x_count = (uint8_t)(axPositiveAvg / ACC_TRESHOLD);
            uint8_t y_count = (uint8_t)(ayPositiveAvg / ACC_TRESHOLD);
            uint8_t z_count = (uint8_t)(azPositiveAvg / Z_TRESHOLD);

            // Lift up
            if (z_count) {
                System_printf("LIFT UP\n");
                System_printf("EXERCISE\n");
                System_flush();
                sendCmd(EXERCISE, z_count);
                buzzer(3500, z_count, 50);
            } else if ((x_count == y_count) && (x_count || y_count)) {
                if (axPositiveAvg > ayPositiveAvg) {
                    System_printf("SLIDE HORIZONTALLY\n");
                    System_printf("EAT\n");
                    System_flush();
                    sendCmd(EAT, x_count);
                    buzzer(2500, x_count, 50);
                } else {
                    System_printf("SLIDE VERTICALLY\n");
                    System_printf("PET\n");
                    System_flush();
                    sendCmd(PET, y_count);
                    buzzer(6000, y_count, 50);
                }
            } else if (x_count > y_count) {
                System_printf("SLIDE HORIZONTALLY\n");
                System_printf("EAT\n");
                System_flush();
                sendCmd(EAT, x_count);
                buzzer(2500, x_count, 50);
            }

            else if (y_count > x_count) {
                System_printf("SLIDE VERTICALLY\n");
                System_printf("PET\n");
                System_flush();
                sendCmd(PET, y_count);
                buzzer(6000, y_count, 50);
            } else {
                System_printf("STATIONARY\n");
                System_flush();
            }
        }
        Task_sleep(1000 * MS / Clock_tickPeriod);
    }
}

void buzzTaskFxn(UArg arg0, UArg arg1) {
    /* Startup Buzz sound */
    /*
    buzzerOpen(buzzerHandle);
    buzzerSetFrequency(2000);
    Task_sleep(500 * MS / Clock_tickPeriod);
    buzzerSetFrequency(2000);
    Task_sleep(500 * MS / Clock_tickPeriod);
    buzzerSetFrequency(2000);
    Task_sleep(500 * MS / Clock_tickPeriod);
    buzzerClose();
    */

    while (1) {
        if (playBuzzFlag) {
            playBuzzFlag = 0;
            if (buzzEatAlert) {
                buzzEatAlert = 0;
                buzzerOpen(buzzerHandle);
                buzzerSetFrequency(2000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(4000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(6000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(8000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(10000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(100 * MS / Clock_tickPeriod);
            }
            if (buzzExerAlert) {
                buzzExerAlert = 0;
                buzzerOpen(buzzerHandle);
                buzzerSetFrequency(6000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(4000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(4000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(4000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(6000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(100 * MS / Clock_tickPeriod);
            }
            if (buzzPetAlert) {
                buzzPetAlert = 0;
                buzzerOpen(buzzerHandle);
                buzzerSetFrequency(10000);

                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(8000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(6000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(4000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerSetFrequency(2000);
                Task_sleep(100 * MS / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(100 * MS / Clock_tickPeriod);
            }
        }
        Task_sleep(1500 * MS / Clock_tickPeriod);
    }
}

void button0Fxn(PIN_Handle handle, PIN_Id pinId) {
    programState = READ_DATA;
}

void button1Fxn(PIN_Handle handle, PIN_Id pinId) {
    programState = STOP;
    // Toggle music on and off
    if (playMusicFlag) {
        playMusicFlag = 0;
    } else {
        playMusicFlag = 1;
    }
}

void commTaskFxn(UArg arg0, UArg arg1) {
    char rxBuffer[RXBUFLEN] = {0};
    uint16_t senderAddr;
    uint16_t receiver = 0;
    int32_t result = StartReceive6LoWPAN();
    if (result == 0) {
        System_abort("Wireless receive start failed");
    }
    System_printf("Wireless init OK\n");
    System_printf("\n");
    System_flush();

    while (1) {
        if (GetRXFlag()) {
            memset(rxBuffer, 0, RXBUFLEN);
            receiver = 0;
            uint8_t actions = 0;
            Receive6LoWPAN(&senderAddr, rxBuffer, RXBUFLEN);
            System_printf(rxBuffer);
            System_printf("\n");
            System_flush();
            if (senderAddr == IEEE80154_SERVER_ADDR) {
                char msg[80] = {0};
                strncpy(msg, rxBuffer, 80);
                actions = parseMsg(msg, &receiver);
                if (receiver == 280) {
                    updatePetNeedFlags(actions);
                    newAlertFlag = 1;
                    playBuzzFlag = 1;
                }
            }
        }
    }
}

void musicTaskFxn(UArg arg0, UArg arg1) {
    uint16_t tuneFreqs[] = {1319, 1175, 740, 830, 1109, 988, 587, 659, 988, 880, 554, 659, 880};
    uint16_t tuneTimingsMs[] = {100, 100, 200, 300, 100, 100, 200, 300, 100, 100, 200, 200, 200};
    uint8_t i = 0;
    while (1) {
        if (playMusicFlag) {
            for (i = 0; i < 13; i++) {
                buzzerOpen(buzzerHandle);
                buzzerSetFrequency(tuneFreqs[i]);
                Task_sleep(tuneTimingsMs[i] * MS / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(tuneTimingsMs[i] * MS / Clock_tickPeriod);
            }
            playMusicFlag = 0;
        }

        Task_sleep(500 * MS / Clock_tickPeriod);
    }
}

int main(void) {
    // Initialize board and I2C
    Board_initGeneral();
    Board_initI2C();
    Init6LoWPAN();
    configMask_t configInUse = getConfigMask();

    button0Handle = PIN_open(&button0State, button0Config);
    if (!button0Handle) {
        System_abort("Error initializing button0 pins\n");
    }

    // Register Button 0 Callback Function
    if (PIN_registerIntCb(button0Handle, &button0Fxn) != 0) {
        System_abort("Error registering button0 callback function");
    }

    button1Handle = PIN_open(&button1State, button1Config);
    if (!button1Handle) {
        System_abort("Error initializing button0 pins\n");
    }

    // Register Button 0 Callback Function
    if (PIN_registerIntCb(button1Handle, &button1Fxn) != 0) {
        System_abort("Error registering button0 callback function");
    }

    /* MENU TASK */
    Task_Handle menuTaskHandle;
    Task_Params menuTaskParams;
    Task_Params_init(&menuTaskParams);
    menuTaskParams.stackSize = STACKSIZE2K;
    menuTaskParams.stack = &menuTaskStack;
    menuTaskParams.priority = 2;
    menuTaskHandle = Task_create(menuTaskFxn, &menuTaskParams, NULL);
    if (menuTaskHandle == NULL) {
        System_abort("Menu Task create failed!");
    }

    /* DISPLAY TASK*/
    Task_Params displayParams;
    Task_Params_init(&displayParams);
    displayParams.stackSize = STACKSIZE2K;
    displayParams.stack = &displayTaskStack;
    displayParams.priority = 2;

    // MENU TASK HANDLE
    Task_Handle displayHandle = Task_create(displayTask, &displayParams, NULL);

    if (displayHandle == NULL) {
        System_abort("Task create failed!");
    }

    /* SENSOR TASK */
    if (configInUse & 0x01) {
        Task_Handle sensorTaskHandle;
        Task_Params sensorTaskParams;
        Task_Params_init(&sensorTaskParams);
        sensorTaskParams.stackSize = STACKSIZE2K;
        sensorTaskParams.stack = &sensorTaskStack;
        sensorTaskParams.priority = 2;
        sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
        if (sensorTaskHandle == NULL) {
            System_abort("MPU9250 SENSOR create failed!");
        }
    }

    /* COMM TASK PARAMS AND CONSTRUCT */
    if (configInUse & 0x10) {
        Task_Params commParams;
        Task_Params_init(&commParams);
        commParams.stackSize = STACKSIZE2K;
        commParams.stack = &commStack;
        commParams.priority = 1;
        Task_construct(&commStruct, (Task_FuncPtr)commTaskFxn, &commParams, NULL);
    }

    /* BUZZER TASK */
    if (configInUse & 0x10) {
        Task_Handle buzzTaskHandle;
        Task_Params buzzTaskParams;
        Task_Params_init(&buzzTaskParams);
        buzzTaskParams.stackSize = STACKSIZE05K;
        buzzTaskParams.priority = 2;
        buzzTaskHandle = Task_create(buzzTaskFxn, &buzzTaskParams, NULL);
        if (buzzTaskHandle == NULL) {
            System_abort("Buzzer task creation failed!\n");
        }
    }

    /* MUSIC TASK */
    if (configInUse & 0x20) {
        Task_Params musicTaskParams;
        Task_Handle musicTaskHandle;
        Task_Params_init(&musicTaskParams);
        musicTaskParams.stackSize = STACKSIZE1K;
        musicTaskParams.stack = &musicTaskStack;
        musicTaskParams.priority = 2;
        musicTaskHandle = Task_create(musicTaskFxn, &musicTaskParams, NULL);
        if (musicTaskHandle == NULL) {
            System_abort("Task create failed!");
        }
    }
    /* Sanity check */
    System_printf("READY TO GO\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();
    return (0);
}