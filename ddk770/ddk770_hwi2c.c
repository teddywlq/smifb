/*******************************************************************
* 
*         Copyright (c) 2007 by Silicon Motion, Inc. (SMI)
* 
*  All rights are reserved. Reproduction or in part is prohibited
*  without the written consent of the copyright owner.
* 
*  hwi2c.c --- SMI DDK 
*  This file contains the source code for hardware i2c.
* 
*******************************************************************/
#include "ddk770_reg.h"
#include "ddk770_power.h"
#include "ddk770_hwi2c.h"
#include "ddk770_gpio.h"
#include "ddk770_help.h"

/*
 *  This function initializes the hardware i2c
 *
 *  Return Value:
 *       0   - Success
 *      -1   - Fail to initialize i2c
 */
long ddk770_hwI2CInit(
    unsigned char i2cNumber //I2C0 or I2C1
)
{
    unsigned long value;

    /* Enable GPIO pins as IIC clock & data */

    value = FIELD_SET(peekRegisterDWord(GPIO_MUX), GPIO_MUX, I2C3, ENABLE);
    pokeRegisterDWord(GPIO_MUX, value);
   

#if 0
		ddk770_gpioMode(24,0);
		ddk770_gpioMode(25,0);
#endif		

              
    /* Enable the I2C Controller and set the bus speed mode */
    value = FIELD_SET(peekRegisterByte(I2C_CTRL), I2C_CTRL, EN, ENABLE);
    value = FIELD_SET(value, I2C_CTRL, MODE, STANDARD);        
    pokeRegisterByte(I2C_CTRL, value);
    
    return 0;
}

/*
 *  This function closes the hardware i2c.
 */
void ddk770_hwI2CClose(
    unsigned char i2cNumber //I2C0 or I2C1
)
{
    unsigned long value;

    value = FIELD_SET(peekRegisterDWord(GPIO_MUX), GPIO_MUX, I2C3, DISABLE);
    pokeRegisterDWord(GPIO_MUX, value);
    

    /* Disable I2C controller */
    value = FIELD_SET(peekRegisterByte(I2C_CTRL), I2C_CTRL, EN, DISABLE);
    pokeRegisterByte(I2C_CTRL, value);
}

/*
 *  This function waits until the transfer is completed within the timeout value.
 *
 *  Return Value:
 *       0   - Transfer is completed
 *      -1   - Tranfer is not successful (timeout)
 */
static long ddk770_hwI2CWaitTXDone(
    unsigned char i2cNumber //I2C0 or I2C1
)
{
    unsigned long timeout;


    /* Wait until the transfer is completed. */
    timeout = HWI2C_WAIT_TIMEOUT;
    while ((FIELD_VAL_GET(peekRegisterByte(I2C_STATUS), I2C_STATUS, TX) != I2C_STATUS_TX_COMPLETED) &&
           (timeout != 0))
        timeout--;
    
    if (timeout == 0)
        return (-1);

    return 0;
}

/*
 *  This function writes data to the i2c slave device registers.
 *
 *  Parameters:
 *      deviceAddress   - i2c Slave device address
 *      length          - Total number of bytes to be written to the device
 *      pBuffer         - The buffer that contains the data to be written to the
 *                     i2c device.   
 *
 *  Return Value:
 *      Total number of bytes those are actually written.
 */
unsigned long ddk770_hwI2CWriteData(
    unsigned char i2cNumber, //I2C0 or I2C1
    unsigned char deviceAddress,
    unsigned long length,
    unsigned char *pBuffer
)
{
    unsigned char count, i;
    unsigned long totalBytes = 0;
    //unsigned char value;
    //unsigned long timeout;

    /* Set the Device Address */
    pokeRegisterByte(I2C_SLAVE_ADDRESS, deviceAddress & ~0x01);
    
    /* Write data.
     * Note:
     *      Only 16 byte can be accessed per i2c start instruction.
     */
    do
    {
        /* Reset I2C by writing 0 to I2C_RESET register to clear the previous status. */
        pokeRegisterByte(I2C_RESET, 0);
        
        /* Set the number of bytes to be written */
        if (length < MAX_HWI2C_FIFO)
            count = length - 1;
        else
            count = MAX_HWI2C_FIFO - 1;
        pokeRegisterByte(I2C_BYTE_COUNT, count);
        
        /* Move the data to the I2C data register */
        for (i = 0; i <= count; i++)
            pokeRegisterByte(I2C_DATA0 + i, *pBuffer++);

        /* Start the I2C */
        pokeRegisterByte(I2C_CTRL, FIELD_SET(peekRegisterByte(I2C_CTRL), I2C_CTRL, CTRL, START));
        
        /* Wait until the transfer is completed. */
        if (ddk770_hwI2CWaitTXDone(i2cNumber) != 0)
            break;
    
        /* Substract length */
        length -= (count + 1);
        
        /* Total byte written */
        totalBytes += (count + 1);
        
    } while (length > 0);
            
    return totalBytes;
}

/*
 *  This function reads data from the slave device and stores them
 *  in the given buffer
 *
 *  Parameters:
 *      deviceAddress   - i2c Slave device address
 *      length          - Total number of bytes to be read
 *      pBuffer         - Pointer to a buffer to be filled with the data read
 *                     from the slave device. It has to be the same size as the
 *                     length to make sure that it can keep all the data read.   
 *
 *  Return Value:
 *      Total number of actual bytes read from the slave device
 */
unsigned long ddk770_hwI2CReadData(
    unsigned char i2cNumber, //I2C0 or I2C1
    unsigned char deviceAddress,
    unsigned long length,
    unsigned char *pBuffer
)
{
    unsigned char count, i;
    unsigned long totalBytes = 0; 
    //unsigned char value;
  

    /* Set the Device Address */
    pokeRegisterByte(I2C_SLAVE_ADDRESS, deviceAddress | 0x01);
    
    /* Read data and save them to the buffer.
     * Note:
     *      Only 16 byte can be accessed per i2c start instruction.
     */
    do
    {
        /* Reset I2C by writing 0 to I2C_RESET register to clear all the status. */
        pokeRegisterByte(I2C_RESET, 0);
        
        /* Set the number of bytes to be read */
        if (length <= MAX_HWI2C_FIFO)
            count = length - 1;
        else
            count = MAX_HWI2C_FIFO - 1;
        pokeRegisterByte(I2C_BYTE_COUNT, count);

        /* Start the I2C */
        pokeRegisterByte(I2C_CTRL, FIELD_SET(peekRegisterByte(I2C_CTRL), I2C_CTRL, CTRL, START));
        
        /* Wait until transaction done. */
        if (ddk770_hwI2CWaitTXDone(i2cNumber) != 0)
            break;

        /* Save the data to the given buffer */
        for (i = 0; i <= count; i++)
        {
            *pBuffer++ = peekRegisterByte(I2C_DATA0 + i);
        }

        /* Substract length by 16 */
        length -= (count + 1);
    
        /* Number of bytes read. */
        totalBytes += (count + 1); 
        
    } while (length > 0);
    
    return totalBytes;
}

/*
 *  This function reads the slave device's register
 *
 *  Parameters:
 *      deviceAddress   - i2c Slave device address which register
 *                        to be read from
 *      registerIndex   - Slave device's register to be read
 *
 *  Return Value:
 *      Register value
 */
unsigned char ddk770_hwI2CReadReg(
    unsigned char i2cNumber, //I2C0 or I2C1
    unsigned char deviceAddress, 
    unsigned char registerIndex
)
{
    unsigned char value = (0xFF);

	deviceAddress = (deviceAddress << 1);

    if (ddk770_hwI2CWriteData(i2cNumber, deviceAddress, 1, &registerIndex) == 1)
        ddk770_hwI2CReadData(i2cNumber, deviceAddress, 1, &value);

    return value;
}

/*
 *  This function reads the 16bit i2c slave device's register
 *
 *  Parameters:
 *      deviceAddress   - i2c Slave device address which register
 *                        to be read from
 *      registerIndex   - Slave device's register to be read
 *
 *  Return Value:
 *      Register value
 */
unsigned short hwI2CReadReg_16bit(
    unsigned char i2cNumber, //I2C0 or I2C1
    unsigned char deviceAddress, 
    unsigned short registerIndex
)
{
    unsigned short ret = 0xffff;
    unsigned char value[2]= {0xff, 0xff};
    unsigned char offset[2] = {0xff, 0xff};
    offset[0] = (registerIndex >> 8) & 0xff;
    offset[1] = registerIndex & 0xff;

    deviceAddress = (deviceAddress << 1);

    if (ddk770_hwI2CWriteData(i2cNumber, deviceAddress, 2, offset) == 2)
        ddk770_hwI2CReadData(i2cNumber, deviceAddress, 2, value);

    ret = value[0];
    ret = ret << 8 | value[1];
    return ret;
}

/*
 *  This function writes a value to the slave device's register
 *
 *  Parameters:
 *      deviceAddress   - i2c Slave device address which register
 *                        to be written
 *      registerIndex   - Slave device's register to be written
 *      data            - Data to be written to the register
 *
 *  Result:
 *          0   - Success
 *         -1   - Fail
 */
long ddk770_hwI2CWriteReg(
    unsigned char i2cNumber, //I2C0 or I2C1
    unsigned char deviceAddress, 
    unsigned char registerIndex, 
    unsigned char data
)
{
    unsigned char value[2];

	deviceAddress = (deviceAddress << 1);
    
    value[0] = registerIndex;
    value[1] = data;
    if (ddk770_hwI2CWriteData(i2cNumber, deviceAddress, 2, value) == 2)
        return 0;

    return (-1);
}

/*
 *  This function writes a value to the 16bit  slave device's register
 *
 *  Parameters:
 *      deviceAddress   - i2c Slave device address which register
 *                        to be written
 *      registerIndex   - Slave device's register to be written
 *      data            - Data to be written to the register
 *
 *  Result:
 *          0   - Success
 *         -1   - Fail
 */
long hwI2CWriteReg_16bit(
    unsigned char i2cNumber, //I2C0 or I2C1
    unsigned char deviceAddress, 
    unsigned short registerIndex, 
    unsigned short data
)
{
    unsigned char val[4];

    deviceAddress = (deviceAddress << 1);
    
    val[0] = (registerIndex >> 8) & 0xff;
    val[1] = registerIndex & 0xff;
    val[2] = (data >> 8) & 0xff;
    val[3] = data & 0xff;

    if (ddk770_hwI2CWriteData(i2cNumber, deviceAddress, 4, val) == 4)
        return 0;

    return (-1);
}