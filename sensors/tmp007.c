/*
 * tmp007.c
 *
 *  Created on: 28.9.2016
 *  Author: Teemu Leppanen / UBIComp / University of Oulu
 *
 *  Datakirja: http://www.ti.com/lit/ds/symlink/tmp007.pdf
 */

#include <xdc/runtime/System.h>
#include <string.h>
#include "Board.h"
#include "tmp007.h"

void tmp007_setup(I2C_Handle *i2c)
{
    System_printf("TMP007: Config OK!\n");
    System_flush();
}

/**************** JTKJ: DO NOT MODIFY ANYTHING ABOVE THIS LINE ****************/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

double tmp007_get_data(I2C_Handle *i2c)
{
    double temperature = 0.0; // return value of the function

    // JTKJ: Find out the correct buffer sizes with this sensor?
    char txBuffer[1];
    char rxBuffer[2];

    // Muuttuja i2c-viestirakenteelle
    I2C_Transaction i2cMessage;

    // i2c-viestirakenne
    i2cMessage.slaveAddress = Board_TMP007_ADDR; // Laitteen osoite
    txBuffer[0] = TMP007_REG_TEMP;               // Rekisterin osoite lähetyspuskuriin
    i2cMessage.writeBuf = txBuffer;              // Lähetyspuskurin asetus
    i2cMessage.writeCount = 1;                   // Lähetetään 1 tavu
    i2cMessage.readBuf = rxBuffer;               // Vastaanottopuskurin asetus
    i2cMessage.readCount = 2;                    // Vastaanotetaan 2 tavua

    if (I2C_transfer(*i2c, &i2cMessage))
    {
        // JTKJ: Here the conversion from register value to temperature
        uint16_t value = rxBuffer[1] + (rxBuffer[0] << 8);
        temperature = (value >> 2) * 0.03125;
    }
    else
    {
        System_printf("TMP007: Data read failed!\n");
        System_flush();
    }

    return temperature;
}
