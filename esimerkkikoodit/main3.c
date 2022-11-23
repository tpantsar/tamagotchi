/*
 *  ======== main.c ========
 *  JTKJ-harjoitustyö
 *  Samppa Alatalo, Henri Paussu
 *
 *  Työhön liittyvät kommentit merkitty "JTKJ:"
 *  Muu koodi englanniksi luettavuuden säilyttämiseksi valmiin koodin kanssa
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>

/* Board Header files */
#include "Board.h"

/* JTKJ Header files */
#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h"

#include <math.h>

/* Task Stacks */
#define STACKSIZE 2048
Char commTaskStack[STACKSIZE];
Char MPUTaskStack[STACKSIZE];
Char FSMTaskStack[STACKSIZE];

/* FSM states */
/* 0 = IDLE; 1 = READ_SENSOR; 2 = MSG_WAITING; 3 = SEND_CHEER */
// JTKJ: typedef enum ei suostu toimimaan, ei edes kun koitettiin kopioida sen syntaksi suoraan CC2650STK.h-tiedostosta
uint8_t State = 0;
uint8_t MSG_SENT = 0; // External state for sent messages to prevent clutter

/* Clock global variables */
Clock_Handle hSensClk, hMsgClk;
Clock_Params sensClkParams, msgClkParams;

/* JTKJ: Display */
Display_Handle hDisplay;

/* Structures used by the MPU task */
// The axis
struct Direction
{
    float x;
    float y;
    float z;
};
// Mean and variance
struct Threshold
{
    struct Direction mean;
    struct Direction var;
};
// Types of movement: standing, elevator or stairs + actual measured acceleration
struct Movement
{
    struct Threshold stand;
    struct Threshold elev;
    struct Threshold stair;
    struct Threshold acc; // For measured acceleration
};

/* Pin Button1 configured as power button */
static PIN_Handle hPowerButton;
static PIN_State sPowerButton;

PIN_Config cPowerButton[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

PIN_Config cPowerWake[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE};

/* Pin Button0 configured as input */
static PIN_Handle hButton0;
static PIN_State sButton0;

PIN_Config cButton0[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE};

/* MPU PIN setup */
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;

static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

/* I2C PIN setup */
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

/* Leds */
static PIN_Handle hLed;
static PIN_State sLed;

PIN_Config cLed[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, // JTKJ: CONFIGURE LEDS AS OUTPUT (SEE LECTURE MATERIAL)
    PIN_TERMINATE};

/* Handler for power button */
void powerButtonFxn(PIN_Handle handle, PIN_Id pinId)
{
    Display_clear(hDisplay);
    Display_close(hDisplay);
    Task_sleep(1000000 / Clock_tickPeriod);

    PIN_close(hPowerButton);

    PINCC26XX_setWakeup(cPowerWake);
    Power_shutdown(NULL, 0);
}

/* Handler for button0 */
void button0Fxn(PIN_Handle handle, PIN_Id pinId)
{
    // Toggles the sensor timer on and off, i.e. toggles sensor data collection
    if (Clock_isActive(hSensClk))
    {
        Clock_stop(hSensClk);
    }
    else
    {
        Clock_start(hSensClk);
    }
}

/* Sensor timer clock handler */
void sensClkFxn(UArg arg0)
{
    if (State == 0)
    {
        State = 1;
    }
}

/* Sent message timer clock handler */
void msgClkFxn(UArg arg0)
{
    if (MSG_SENT == 1)
    {
        MSG_SENT = 0;
        Clock_stop(hMsgClk);
    }
}

/* FSM Task */
void FSMTask(UArg arg0, UArg arg1)
{
    // Count for testing the sensor timer state to prevent unintentional printing
    uint16_t count = 0;

    // Init display
    Display_Params displayParams;
    displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);
    char address[7];

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL)
    {
        System_abort("Error initializing Display\n");
    }

    if (hDisplay)
    {
        sprintf(address, "%x", IEEE80154_MY_ADDR);
        Display_print0(hDisplay, 2, 2, address);

        Task_sleep(3 * 1000000 / Clock_tickPeriod);

        Display_clear(hDisplay);
    }

    // Check that display works
    Display_clear(hDisplay);
    Display_print0(hDisplay, 5, 4, "Good day,");
    Display_print0(hDisplay, 6, 5, "master");

    Task_sleep(3 * 1000000 / Clock_tickPeriod);

    Display_clear(hDisplay);

    // Start sensor timer
    Clock_start(hSensClk);

    // Print button UI markers
    Display_print0(hDisplay, 0, 13, "MIT");
    Display_print0(hDisplay, 11, 13, "OFF");

    // Main loop
    while (1)
    {
        // Check if a new message is available
        if (State == 0 && GetRXFlag())
        {
            State = 2;
        }

        // Check if sensor timer is running
        // JTKJ: Tarkistus tehdään tässä, koska napin käsittelijäfunktiossa
        // pienikin kosketus Display-funktioihin sai MCU:n jumiin
        if (State != 1 && Clock_isActive(hSensClk))
        {
            count = 0;
            Display_clearLine(hDisplay, 9);
        }
        // If the sensor timer isn't running and count is reached, display info on screen
        else if (count > 9)
        {
            Display_clearLine(hDisplay, 5);
            Display_print0(hDisplay, 9, 1, "Mittaus POIS");
        }
        // Add to count if the timer isn't running. This prevents unwanted prints on screen
        else
        {
            count++;
        }
        Task_sleep(10000 / Clock_tickPeriod);
    }
}

/* Communication task */
void commTask(UArg arg0, UArg arg1)
{
    char payload[16];
    uint16_t senderAddr;

    // Radio to receive mode
    int32_t result = StartReceive6LoWPAN();
    if (result != true)
    {
        System_abort("Wireless receive mode failed");
    }

    while (1)
    {
        // If there's a new message available
        if (State == 2)
        {
            // Turn on LED
            PIN_setOutputValue(hLed, Board_LED0, !PIN_getOutputValue(Board_LED0));

            // Reset payload and fetch message
            memset(payload, 0, 16);
            Receive6LoWPAN(&senderAddr, payload, 16);

            // Display message
            Display_clearLines(hDisplay, 7, 10);
            Display_print0(hDisplay, 7, 0, payload);

            // State to IDLE
            State = 0;

            // Back to receive mode
            if (StartReceive6LoWPAN() != true)
            {
                System_abort("Wireless receive mode failed");
            }

            // Wait, then turn off LED
            Task_sleep(2 * 1000000 / Clock_tickPeriod);
            PIN_setOutputValue(hLed, Board_LED0, !PIN_getOutputValue(Board_LED0));
        }
        // If stairs have been detected and a cheer hasn't been sent in the last ~20s, send cheer
        else if (State == 3 && MSG_SENT != 1)
        {
            sprintf(payload, "%x ON VOIMAA", IEEE80154_MY_ADDR);
            Send6LoWPAN(0xFFFF, &payload, strlen(payload)); // 0xFFFF for broadcast, according to lecture material

            // Set MSG_SENT as true and start the message timer
            MSG_SENT = 1;
            Clock_start(hMsgClk);

            // State to IDLE
            State = 0;

            // Back to receive mode
            if (StartReceive6LoWPAN() != true)
            {
                System_abort("Wireless receive mode failed");
            }
        }
        Task_sleep(10000 / Clock_tickPeriod);
    }
}

/* Task for reading and analysing accelerometer data */
void MPUTask(UArg arg0, UArg arg1)
{
    I2C_Handle i2c; // Interface for other sensors
    I2C_Params i2cParams;
    I2C_Handle i2cMPU; // Interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    struct Movement Move;

    uint8_t data_index = 0;
    float ax[20], ay[20], az[20], gx, gy, gz;
    double pres, temp, lux;
    char temp_str[9], pres_str[12], lux_str[12];

    // Introduce threshold values, only the x-axis provides meaningful differences
    Move.stand.mean.x = -0.0042274420988475175;
    Move.stand.var.x = -0.01092269572805851;

    Move.elev.mean.x = -0.0264892578125;
    Move.elev.var.x = 0.001473727258476051;

    Move.stair.mean.x = -0.015235162550403226;
    Move.stair.var.x = 0.008185769908057023;

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL)
    {
        System_abort("Error Initializing I2CMPU\n");
    }

    PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    I2C_close(i2cMPU);

    // Open I2C for other sensors
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }

    // BMP280 setup
    bmp280_setup(&i2c);

    // OPT3001 setup
    opt3001_setup(&i2c);

    I2C_close(i2c);

    // Loop indefinitely
    while (1)
    {
        // Triggered by sensor timer
        if (State == 1)
        {
            // Stop sensor timer during execution
            Clock_stop(hSensClk);

            // Open I2C for other sensors
            i2c = I2C_open(Board_I2C, &i2cParams);
            if (i2c == NULL)
            {
                System_abort("Error Initializing I2C\n");
            }

            // BMP280 ask data
            bmp280_get_data(&i2c, &pres, &temp);

            // OPT3001 ask data
            opt3001_get_data(&i2c, &lux);

            // Close other sensors' I2C
            I2C_close(i2c);

            // MPU open I2C
            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
            if (i2cMPU == NULL)
            {
                System_abort("Error Initializing I2CMPU\n");
            }

            /* Get data from MPU
             * Accelerometer values: ax, ay, az
             * Gyroscope values: gx, gy, gz
             */
            mpu9250_get_data(&i2cMPU, &ax[data_index], &ay[data_index], &az[data_index], &gx, &gy, &gz);

            // MPU close I2C
            I2C_close(i2cMPU);

            // Gather 20 datapoints into an array and analyse
            // JTKJ: Tunnistus tehdään täällä eikä omassa funktiossaan/taskissaan,
            // koska jostain mystisestä syystä kääntäjä ei anna antaa Move-tietorakennetta parametrinä,
            // vaikka aiemmilla tietorakennetoteutuksilla samanlaiset kutsut toimivat
            if (data_index >= 19)
            {
                // Get mean and variance for the datapoints
                for (data_index = 0; data_index < 20; data_index++)
                {
                    Move.acc.mean.x += ax[data_index] / 20.0;
                }
                for (data_index = 0; data_index < 20; data_index++)
                {
                    Move.acc.var.x += pow((ax[data_index] - Move.acc.mean.x), 2) / 19.0;
                }

                // If movement is detected as standing, simply clear the line
                if (Move.acc.var.x <= (1.5 * Move.stand.var.x))
                {
                    Display_clearLine(hDisplay, 5);
                }
                // If detected as elevator, print a small insult
                else if (Move.acc.var.x <= (1.5 * Move.elev.var.x))
                {
                    Display_print0(hDisplay, 5, 2, "Laiskimus...");
                }
                // If detected as stairs, cheer
                else if (Move.acc.var.x <= (1.5 * Move.stair.var.x))
                {
                    Display_print0(hDisplay, 5, 1, "Urheilullista!");
                    State = 3;
                    Task_sleep(10000 / Clock_tickPeriod);
                }
                // If detection fails, simply clear the line, as with standing
                else
                {
                    Display_clearLine(hDisplay, 5);
                }

                // Reset the index counter
                data_index = 0;
            }
            // If more data needs to be collected, simply add 1 to the index counter
            else
            {
                data_index++;
            }

            sprintf(temp_str, "%.1f C", temp);
            sprintf(pres_str, "%.1f hPa", pres);
            sprintf(lux_str, "%.1f lx", lux);

            // Print temperature, air pressure and illuminance on screen
            Display_print0(hDisplay, 1, 1, temp_str);
            Display_print0(hDisplay, 2, 1, pres_str);
            Display_print0(hDisplay, 3, 1, lux_str);

            // State to IDLE
            State = 0;

            // Start sensor timer
            Clock_start(hSensClk);
        }
        Task_sleep(10000 / Clock_tickPeriod);
    }
    // MPU9250 power off (never reached)
    PIN_setOutputValue(hMpuPin, Board_MPU_POWER, Board_MPU_POWER_OFF);
}

Int main(void)
{
    /* Task variables */
    Task_Handle hCommTask;
    Task_Params commTaskParams;
    Task_Handle hMPUTask;
    Task_Params MPUTaskParams;
    Task_Handle hFSMTask;
    Task_Params FSMTaskParams;

    /* Initialize board */
    Board_initGeneral();
    Board_initI2C();

    /* Init power button */
    hPowerButton = PIN_open(&sPowerButton, cPowerButton);
    if (!hPowerButton)
    {
        System_abort("Error initializing power button shut pins\n");
    }
    if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0)
    {
        System_abort("Error registering power button callback function");
    }

    /* Init button0 */
    hButton0 = PIN_open(&sButton0, cButton0);
    if (!hButton0)
    {
        System_abort("Error initializing LED button shut pins\n");
    }
    if (PIN_registerIntCb(hButton0, &button0Fxn) != 0)
    {
        System_abort("Error registering LED button callback function");
    }

    /* Init Leds */
    hLed = PIN_open(&sLed, cLed);
    if (!hLed)
    {
        System_abort("Error initializing LED pin\n");
    }

    /* Init sensor timer clock */
    Clock_Params_init(&sensClkParams);
    sensClkParams.period = 250000 / Clock_tickPeriod;
    sensClkParams.startFlag = FALSE;

    /* Init message timer clock */
    Clock_Params_init(&msgClkParams);
    msgClkParams.period = 0;
    msgClkParams.startFlag = FALSE;

    /* Init MPU Task */
    Task_Params_init(&MPUTaskParams);
    MPUTaskParams.stackSize = STACKSIZE;
    MPUTaskParams.stack = &MPUTaskStack;
    MPUTaskParams.priority = 1;

    hMPUTask = Task_create(MPUTask, &MPUTaskParams, NULL);
    if (hMPUTask == NULL)
    {
        System_abort("Task create failed!");
    }

    /* Init Communication Task */
    Init6LoWPAN();

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority = 1;

    hCommTask = Task_create(commTask, &commTaskParams, NULL);
    if (hCommTask == NULL)
    {
        System_abort("Task create failed!");
    }

    /* Init FSM Task */
    Task_Params_init(&FSMTaskParams);
    FSMTaskParams.stackSize = STACKSIZE;
    FSMTaskParams.stack = &FSMTaskStack;
    FSMTaskParams.priority = 1;

    hFSMTask = Task_create(FSMTask, &FSMTaskParams, NULL);
    if (hFSMTask == NULL)
    {
        System_abort("Task create failed!");
    }

    /* Create a sensor timer clock for READ_SENSOR timing; ~3s timeout, ~250ms periods */
    hSensClk = Clock_create((Clock_FuncPtr)sensClkFxn, 3 * 1000000 / Clock_tickPeriod, &sensClkParams, NULL);
    if (hSensClk == NULL)
    {
        System_abort("Clock create failed!");
    }

    /* Create a second clock for MSG_SENT timer; ~20s timeout, no period */
    hMsgClk = Clock_create((Clock_FuncPtr)msgClkFxn, 20 * 1000000 / Clock_tickPeriod, &msgClkParams, NULL);
    if (hMsgClk == NULL)
    {
        System_abort("Clock create failed!");
    }

    /* Send hello to console */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
