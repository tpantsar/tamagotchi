/* jannepetteri */

/* C Standard library */
#include <aanet.h>
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "sensors/tmp007.h"
#include "buzzer.h"
#include <driverlib/aon_batmon.h> //patteri

/* Task */
#define STACKSIZE 2048
#define SMALLSTACKSIZE 512 // aanitask kayttaa tata, 2048 naytti niin isolta pelkalle aanelle
#define MAXKOKO 15         // sliding window muuttujille, jos otetaan dataa 5 kertaa 3 sekuntia =15, talla voisi luultavasti max 2sekunnin pituisia liikkeita maaritella
#define BLENGTH 80         // viestin pituus joka kerataan bufferista

void sendMsg(UART_Handle handle, char *msg, int length);

// viestit taustajarjestelmaan
char messageRuoki[20] = "id:153,EAT:2";
char messageLeiki[20] = "id:153,PET:2";
char messageLeikki[20] = "id:153,EXERCISE:2";
char hoivamsg[50] = "id:153,MSG1:Happy,MSG2:Can survive rough times";
char hoivamsgRadio[20] = "id:153,MSG1:Happy";
char messageVaroitus[45] = "id:153,MSG1:Sceptical,MSG2:Planning escape..";
char messageVaroitusRadio[25] = "id:153,MSG1:Sceptical";
char messageAktivoi[25] = "id:153,ACTIVATE:3;3;3";

Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char mpuTaskStack[STACKSIZE];
Char aaniTaskStack[SMALLSTACKSIZE];
Char analyseDataTaskStack[STACKSIZE];

uint8_t isHappy = 0;    // jos tamagotchi happy -> niin se voi pysya hallussa yhden laiminlyonnin verran
uint8_t survive = 0;    // survive toiminnallisuuden ja viestin triggeri flagi
uint8_t buffCount = 0;  // buffercounter, kun kokonaisen viestin edest� on k�sitelty merkkej� (BLENGTH) -> itse viesti kasitellaan
uint8_t uartBuffer[16]; // itse uart bufferin koko, tama nayttaisi riittavan 16
char testi[50];         // testailua varten
char uartStr[BLENGTH];  // viesti1 taustajarjestelmasta
char uartStr2[BLENGTH]; // viesti2 - 1 viesti ei aina riita, (niita tulee 1-3 kerralla + mahdolliset muiden viestit)
char radioBuffer[160];  // testi radioviesti vastaanotto
char tulosteluStr[100];

enum moves
{
    PRIMARY = 0,
    SECONDARY
};
enum moves moveState = PRIMARY;
enum state
{
    STOP = 0,
    WAITING,
    DATA_READY,
    RUOKI,
    LEIKKI,
    HOIVA,
    AKTIVOI,
    LEIKI
}; // STOP -> liikkeentunnistus pois paalta
enum state programState = WAITING;

enum communication
{
    RADIO = 0,
    GATEWAY
};
enum communication commState = GATEWAY;

//----------------globaalit muuttujat---------------
float ambientLight = -1000.0;
float temperature = -100.0;
//--sliding window globaalit muuttujat
float accx[MAXKOKO] = {0};
float accy[MAXKOKO] = {0};
float accz[MAXKOKO] = {0};
float gyrox[MAXKOKO] = {0};
float gyroy[MAXKOKO] = {0};
float gyroz[MAXKOKO] = {0};
uint8_t index = 0; // indeksimuuttuja sliding window

//--------------------------------------------------
#include "liike.h"
#include <apufunktiot.h>

static PIN_Handle buttonHandle; // vasen nappi
static PIN_State buttonState;

static PIN_Handle rightButtonHandle; // oikea nappi (katsottuna ettÃ¤ johto osoittaa ylÃ¶s ja katsot monitoria)
static PIN_State rightButtonState;

static PIN_Handle ledHandle;
static PIN_State ledState;

static PIN_Handle mpuHandle; // liikeanturi
static PIN_State mpuState;   // liikeanturi

static PIN_Config mpuConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE};

PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tÃ¤llÃ¤ vakiolla
};
PIN_Config buttonWakeConfig[] = {
    // vasemmalle napille alustettu virtojen katkaisu/pÃ¤Ã¤llelaitto
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tÃ¤llÃ¤ vakiolla
};

PIN_Config rightButtonConfig[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tÃ¤llÃ¤ vakiolla
};

PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE // Asetustaulukko lopetetaan aina tÃ¤llÃ¤ vakiolla
};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1};

// Primary liikkeet default (1piippaus ja alaledi paalla) - Secondary (2piippausta ja alaled off)
void buttonFxn(PIN_Handle handle, PIN_Id pinId)
{
    if (moveState == PRIMARY)
    {
        PIN_setOutputValue(ledHandle, Board_LED0, 0);
        moveState = SECONDARY; // secondary moves (alaledi off - kaksi piippausta): AKTIVOI,HOIVAA

        if (aaniState == SILENCE)
        {
            aaniState = TWOBEEPS;
        }
    }
    else
    {
        PIN_setOutputValue(ledHandle, Board_LED0, 1);
        moveState = PRIMARY; // primary moves (alaled on - yksi piippaus):RUOKI,LEIKI,LIIKU

        if (aaniState == SILENCE)
        {
            aaniState = ONEBEEP;
        }
    }
}

// kommunikointi Gateway(defaulttina) 1piippaus - Radio 2 piippausta - Liiketunnistus seis 3 piippausta
void rightButtonFxn(PIN_Handle handle, PIN_Id pinId)
{
    if (programState == STOP)
    {
        programState = DATA_READY;
        commState = GATEWAY;
        if (aaniState == SILENCE)
        {
            aaniState = ONEBEEP;
        }
        System_printf("liiketunnistus paalla - gateway\n\r");
        System_flush();
    }
    else if (commState == GATEWAY)
    {
        commState = RADIO;
        if (aaniState == SILENCE)
        {
            aaniState = TWOBEEPS;
        }
        System_printf("liiketunnistus paalla - radio\n\r");
        System_flush();
    }
    else
    {
        programState = STOP;
        if (aaniState == SILENCE)
        {
            aaniState = THREEBEEPS;
        }
        System_printf("liiketunnistus pysaytetty \n\r");
        System_flush();
    }
}

// oikean kyljen kautta katolleen ja takas (tunnistaa katollaan -> oikeakylki)
int analyseAktivoi()
{
    uint8_t i;
    for (i = 0; i < MAXKOKO; i++)
    {
        if (aktivoi(i))
        {
            return 1;
        }
    }
    return 0;
}

// lampimassa ja pimeassa hoivataan
int analyseHoiva()
{
    if (temperature > 32 && ambientLight < 0.1)
    {
        return 1;
    }
    return 0;
}
// tunnistaa vaakatasossa suoritetun ympyr�liikkeen
int analyseLeiki()
{
    uint8_t i;
    uint8_t xylos = 0;
    uint8_t y = 0;
    uint8_t xalas = 0;

    for (i = 0; i < MAXKOKO; i++)
    {
        y += ymove(i);
        xylos += xmoveu(i);
        xalas += xmoved(i);
        if (xylos > 0 && xalas > 0 && y > 1)
        {
            return 1;
        }
    }
    return 0;
}
// 2-3 hyppya vaakatasosta poydalta
int analyseLiiku()
{
    uint8_t i;
    uint8_t counter = 0;

    for (i = 0; i < MAXKOKO; i++)
    {
        if (jump(i))
        {
            counter++;
        }
        if (counter >= 3)
        {             // jos 3 suunnanmuutosta (ylos tai alas) globaaleissa muuttujissa (n. 3s sisalla)
            return 1; // palauttaa true jonka seurauksena muuttujat nollautuu analyseTaskissa
        }
    }
    return 0;
}

// vasemmalle kallistus katolleen ja takas
int analyseRuoki()
{
    uint8_t i;
    for (i = 0; i < MAXKOKO; i++)
    {
        if (ruoki(i))
        {
            return 1;
        }
    }
    return 0;
}

void analyseDataFxn(UArg arg0, UArg arg1)
{
    while (1)
    {
        if (programState == DATA_READY)
        {
            /*ensin tarkastetaan monimutkaisempaa dataa ja muutetaan tilaa jos ehdot tayttyy
             *, jos minkaan tilan vaatimukset eivat tayty odotellaan uutta dataa (WAITING)*/
            if (moveState == PRIMARY)
            { // primary moves (vasemmasta napista muuttaa primary-secondary)
                if (analyseLiiku())
                {
                    programState = LEIKKI;
                    if (aaniState == SILENCE)
                    {
                        aaniState = TWOBEEPS;
                    }
                    nollaaMuuttujat();
                }
                else if (analyseLeiki())
                {
                    programState = LEIKI;
                    if (aaniState == SILENCE)
                    {
                        aaniState = TWOBEEPS;
                    }
                    nollaaMuuttujat();
                }
                else if (analyseRuoki())
                {
                    programState = RUOKI;
                    if (aaniState == SILENCE)
                    {
                        aaniState = TWOBEEPS;
                    }
                    nollaaMuuttujat();
                }
                else
                {
                    programState = WAITING;
                }
            }
            else
            { // secondary moves
                if (analyseAktivoi())
                {
                    programState = AKTIVOI;
                    if (aaniState == SILENCE)
                    {
                        aaniState = TWOBEEPS; // liike tunnistettu piippaa 2 kertaa
                    }
                    nollaaMuuttujat();
                }
                else if (!isHappy && analyseHoiva())
                {
                    programState = HOIVA;
                    temperature = 0;
                    ambientLight = 1;
                    isHappy = 1; // happy flag -> selviaa yhden laiminlyonnin
                    if (aaniState == SILENCE)
                    {
                        aaniState = TWOBEEPS;
                    }
                }
                else
                {
                    programState = WAITING;
                }
            }
        }
        Task_sleep(200000 / Clock_tickPeriod);
    }
}

void uartFxn(UART_Handle handle, void *rxBuf, size_t len)
{
    // buffcountilla kirjoitus oikeaan merkkijonoon, buffcount nollataan kun viestit luettu
    if (commState == GATEWAY)
    {
        if (buffCount < BLENGTH)
        {
            strncat(uartStr, rxBuf, BLENGTH);
            buffCount++;
        }
        else if (buffCount < (BLENGTH * 2))
        {
            strncat(uartStr2, rxBuf, BLENGTH);
            buffCount++;
        }
    }
    UART_read(handle, rxBuf, 1);
}

void sendMsg(UART_Handle handle, char *msg, int length)
{
    if (commState == GATEWAY)
    { // viestitys uartilla
        UART_write(handle, msg, length + 1);
    }
    else
    {
        Send6LoWPAN(IEEE80154_SERVER_ADDR, (uint8_t *)msg, length); // viestitys radio paalla
        StartReceive6LoWPAN();
    }
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1)
{
    UART_Handle uart;
    UART_Params uartParams;
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readCallback = &uartFxn;
    uartParams.baudRate = 9600;            // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8;    // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE;   // 1
    uart = UART_open(Board_UART, &uartParams);

    if (uart == NULL)
    {
        System_abort("Error opening the UART\n");
    }

    UART_read(uart, uartBuffer, 1);

    // char payload[16]; // viestipuskuri
    uint16_t senderAddr;

    // Radio alustetaan vastaanottotilaan
    int32_t result = StartReceive6LoWPAN();
    if (result != true)
    {
        System_abort("Wireless receive start failed");
    }

    while (1)
    {
        if (commState == RADIO && GetRXFlag())
        {
            // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)

            // Luetaan viesti puskuriin payload
            Receive6LoWPAN(&senderAddr, radioBuffer, BLENGTH);
            StartReceive6LoWPAN();
            radioStrs();
            // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
            // sprintf(tulosteluStr,"%s -radio\r\n",radioBuffer);
            // System_printf(tulosteluStr);
            // System_flush();
            // memset(tulosteluStr,0,100);
        }
        // jos tamagotchi on pidetty aiemmin tyytyvaisena ja taustajarjestelma lahettaa viestin etta se on karkaamassa
        //  readMsg palauttaa 1 -> survive=True
        survive = readMsg(); // lukee taustajarjestelman lahettamat viestit (UART) - radioviesteille tarttee tehda viela jotain.
        if (survive)
        {
            sendMsg(uart, messageAktivoi, strlen(messageAktivoi));
            if (commState == GATEWAY)
            {
                sendMsg(uart, messageVaroitus, strlen(messageVaroitus));
            }
            else
            {
                sendMsg(uart, messageVaroitusRadio, strlen(messageVaroitusRadio)); // radion kautta toivottiin lyhyita viesteja
            }
            isHappy = 0; // ei ole enaa tyytyvainen
            survive = 0; // flag muutetaan
        }

        switch (programState)
        {
        case RUOKI:
            sendMsg(uart, messageRuoki, strlen(messageRuoki));
            programState = WAITING;
            break;
        case LEIKKI:
            sendMsg(uart, messageLeikki, strlen(messageLeikki));
            programState = WAITING;
            break;
        case HOIVA:
            if (commState == GATEWAY)
            {
                sendMsg(uart, hoivamsg, strlen(hoivamsg));
            }
            else
            {
                sendMsg(uart, hoivamsgRadio, strlen(hoivamsgRadio)); // radion kautta lyhyempi viesti
            }
            programState = WAITING;
            break;
        case AKTIVOI:
            sendMsg(uart, messageAktivoi, strlen(messageAktivoi));
            programState = WAITING;
            break;
        case LEIKI:
            sendMsg(uart, messageLeiki, strlen(messageLeiki));
            programState = WAITING;
            break;
        default:
            // programState oli jotain muuta esim. DATA_READY, ei muuteta mitaan
            break;
        }

        Task_sleep(1000000 / Clock_tickPeriod); // 1 sekunti
    }
}

// Taskifunktio
void sensorTaskFxn(UArg arg0, UArg arg1)
{
    // alaledi paalle
    PIN_setOutputValue(ledHandle, Board_LED0, 1);

    I2C_Handle i2c;       // muiden sensorien vayla
    I2C_Params i2cParams; // muiden sensorien vayla

    I2C_Handle i2cMPU;
    I2C_Params i2cMPUParams;

    //    I2C_Transaction i2cMessage;

    uint8_t sensorCounter = 0; // ei lueta joka tickilla kaikkia sensoreja, counteri apuna

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    Task_sleep(100000 / Clock_tickPeriod); // mpu sensor powerup wait
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL)
    {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU setup and calibration
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();
    mpu9250_setup(&i2cMPU);
    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();
    I2C_close(i2cMPU);

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // lampomittari setup ja calibraario
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }
    Task_sleep(100000 / Clock_tickPeriod);
    tmp007_setup(&i2c);
    I2C_close(i2c);

    // Avataan yhteys, valoisuusmittari setup ja calibraatio
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL)
    {
        System_abort("Error Initializing I2C\n");
    }
    
    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);
    I2C_close(i2c);

    double light = -1000; // valimuuttuja,
    while (1)
    {
        if (programState == WAITING)
        {
            uint32_t time = Clock_getTicks() / 10000;

            // Lampo-SENSORI:n luku n. 3 sekunnin valein
            if (sensorCounter % 17 == 0)
            {
                i2c = I2C_open(Board_I2C_TMP, &i2cParams); // muiden sensorien vayla auki
                temperature = tmp007_get_data(&i2c);       // datan lukemiset globaaliin muuttujaan
                I2C_close(i2c);                            // datat luettu niin kiinni
            }

            // VALOSENSORI:n luku n. 2 sekunnin valein
            else if (sensorCounter % 11 == 0)
            {
                i2c = I2C_open(Board_I2C_TMP, &i2cParams); // muiden sensorien vayla auki
                light = opt3001_get_data(&i2c);            // datan lukemiset globaaliseen muuttujaan, opt3001 ei meinaa ehtia kaikkea lukea kun haetaan 5 kertaa per sek
                if (light >= 0)
                {
                    ambientLight = light; // purkkaratkaisu valomittarin hitauteen, saa parantaa jos keksii miten
                }
                I2C_close(i2c); // datat luettu niin kiinni
            }

            // LIIKESENSORI:n luku n. 0,2 sekunnin valein
            else
            {
                i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);                                                                      // mpu (liiketunnistin) vÃ¤ylÃ¤ auki (vain 1kpl vaylia kerrallaan auki)
                mpu9250_get_data(&i2cMPU, &accx[index], &accy[index], &accz[index], &gyrox[index], &gyroy[index], &gyroz[index]); // datan lukeminen

                I2C_close(i2cMPU); // mpu (liiketunnistin) vayla kiinni
                if (index == MAXKOKO - 1)
                { // sliding window indeksin paivitys
                    index = 0;
                }
                else
                {
                    index++;
                }
                // luetaan liikesensori 3 kertaa ennenkuin annetaan lupa analysoida dataa -> DATA_READY
                if (sensorCounter % 3 == 0 && programState != STOP)
                {                              // jos liiketunnistuksen keskeytys (oikea nappi) ei ole laitettu paalle niin DATA_READY
                    programState = DATA_READY; // DATA_READY vasta kun tarpeeksi uutta liikedataa keratty (muiden sensorien luku alkuluvuilla 11 & 17 niin ei hairitse liikekeraysta huonoon aikaan)
                }
            }
        }
        sensorCounter++;

        Task_sleep(150000 / Clock_tickPeriod); // 0,15 s  eli datan kerays 6 kertaa sekunnissa
    }
}

int main(void)
{
    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;

    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    Task_Handle aaniTaskHandle;
    Task_Params aaniTaskParams;

    Task_Handle analyseDataTaskHandle;
    Task_Params analyseDataTaskParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();
    Board_initUART();
    Board_initI2C();

    mpuHandle = PIN_open(&mpuState, mpuConfig);
    if (mpuHandle == NULL)
    {
        System_abort("Pin open failed!");
    }

    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle)
    {
        System_abort("Error initializing button pins\n");
    }
    rightButtonHandle = PIN_open(&rightButtonState, rightButtonConfig);
    if (!rightButtonHandle)
    {
        System_abort("Error initializing button pins\n");
    }
    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle)
    {
        System_abort("Error initializing LED pins\n");
    }
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0)
    {
        System_abort("Error registering left button callback function");
    }
    if (PIN_registerIntCb(rightButtonHandle, &rightButtonFxn) != 0)
    {
        System_abort("Error registering right button callback function");
    }
    // sensoreitten lukeminen globaaleihin muuttujiin
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    /*Data analyysi taski, jos liike tunnistetaan muutetaan programState asianmukaiseksi*/
    Task_Params_init(&analyseDataTaskParams);
    analyseDataTaskParams.stackSize = STACKSIZE;
    analyseDataTaskParams.stack = &analyseDataTaskStack;
    analyseDataTaskParams.priority = 2;
    analyseDataTaskHandle = Task_create(analyseDataFxn, &analyseDataTaskParams, NULL);
    if (analyseDataTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    // taustajarjestelman kanssa kommunikointi
    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 1;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL)
    {
        System_abort("Task create failed!");
    }

    /*Voit kutsua aania eri toiminnallisuuksiin muuttamalla aaniStatea halutuksi esim. aaniState=ONEBEEP;
     *muita aanitiloja SILENCE,ONEBEEP,TWOBEEPS,MUSIC
     *aanitilat palautuvat automaattisesti takaisin SILENCE:een suorituksen jalkeen*/
    Task_Params_init(&aaniTaskParams);
    aaniTaskParams.stackSize = SMALLSTACKSIZE;
    aaniTaskParams.stack = &aaniTaskStack;
    aaniTaskParams.priority = 2;
    aaniTaskHandle = Task_create(aaniTask, &aaniTaskParams, NULL);
    if (aaniTaskHandle == NULL)
    {
        System_abort("Task create failed");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}