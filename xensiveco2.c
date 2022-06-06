#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>
#include "xensiveco2.h"

int i2c_fd;          /**< I2C interface*/
int  uart_fd;         /**< UART interface */
uint8_t           intPin;       /**< Interrupt pin */

const uint16_t baudrateBps = 9600;      /**< UART baud rate in bps */
const uint32_t freqHz      = 100000;    /**< I2C frequency in Hz*/
const uint8_t       unusedPin = 0xFFU; /**< Unused pin */
xensiv_pasco2_t   dev;          /**< XENSIV™ PAS CO2 corelib object */


/**
 * @brief   Begins the sensor
 *
 * @details Initializes the serial interface if the initialization
 *          is delegated to the PASCO2Serial class.
 *          Sets the I2C freq or UART baudrate to the default values
 *          prior the serial interface initialization.
 *          Initializes the interrupt pin if used.
 *
 * @return  XENSIV™ PAS CO2 error code
 * @retval  XENSIV_PASCO2_OK if success
 * @pre     None
 */
Error_t begin(bool i2c, bool uart)
{
    int32_t ret = XENSIV_PASCO2_OK;
    xensiv_pasco2_measurement_config_t  measConf;

    /* Initialize sensor interface */
    if(i2c)
    {
        #ifndef PAS_CO2_SERIAL_PAL_INIT_EXTERNAL
        char devname[20];
        int dev_index = 1;
        snprintf(devname, 19, "/dev/i2c-%d", dev_index);
        i2c_fd = open(devname, O_RDWR);
        if (ioctl(i2c_fd, I2C_SLAVE, XENSIV_PASCO2_I2C_ADDR) < 0) {
            return -1;
        }
        #endif
        ret = xensiv_pasco2_init_i2c(&dev, &i2c_fd);
    }
    else if(uart)
    {
        #ifndef PAS_CO2_SERIAL_PAL_INIT_EXTERNAL

        #endif
        //ret = xensiv_pasco2_init_uart(&dev, uart);
    }

    /**
     * Set the sensor in idle mode.
     * In case PWM_DIS is by hardware configuring
     * the device to continuous mode
     */
    ret = xensiv_pasco2_get_measurement_config(&dev, &measConf);
    INO_ASSERT_RET(ret);
    measConf.b.op_mode = XENSIV_PASCO2_OP_MODE_IDLE;

    ret = xensiv_pasco2_set_measurement_config(&dev, measConf);

    return ret;
}

/**
 * @brief   Ends the sensor
 *
 * @details Deinitializes the serial interface if the deinitialization
 *          is delegated to the PASCO2Serial class.
 *          Deinitializes the interrupt pin if used.
 *
 * @return  XENSIV™ PAS CO2 error code
 * @retval  XENSIV_PASCO2_OK always
 * @pre     begin()
 */
Error_t end(bool i2c, bool uart)
{
    /**< Deinitialize sensor interface*/
    if(i2c)
    {
        #ifndef PAS_CO2_SERIAL_PAL_INIT_EXTERNAL
        #if !defined(ARDUINO_ARCH_ESP32)
        close(i2c_fd);
        #endif
        #endif
    }
    else if(uart)
    {
        #ifndef PAS_CO2_SERIAL_PAL_INIT_EXTERNAL
        close(uart_fd);
        #endif
    }

    /* Deinitialize interrupt pin */
    if(unusedPin != intPin)
    {
        //detachInterrupt(digitalPinToInterrupt(intPin));
    }

    return XENSIV_PASCO2_OK;
}

/**
 * @brief       Triggers the internal measuring of the sensor
 *
 * @details     The function start the measurement controlling the different
 *              sensor modes and features depending on the configured arguments.
 *
 *              Single shot
 *              ---------------------------------------------------------------
 *              If the function is called with no arguments, the sensor
 *              will be triggered to perform a single shot measurement.
 *              The user needs to poll with getCO2() until the CO2 value is
 *              available and has been read out from the sensor.
 *              The CO2 concentration value read will be zero as long as
 *              no value is available or if any error occurred in the
 *              readout attempt.
 *              Polling example:
 *
 *              @code
 *              PASCO2Serial cotwo(serial_intf);
 *              int16_t   co2ppm;
 *
 *              serial_intf.begin();
 *
 *              cotwo.begin();
 *
 *              cotwo.startMeasure();
 *
 *              do{ cotwo.getCO2(co2ppm); } while (co2ppm == 0);
 *              @endcode
 *
 *              Periodic measurement
 *              ---------------------------------------------------------------
 *              Periodic measurements (periodInSec) will configure the sensor
 *              to perform a measurement every desired period. Between 5 and
 *              4095 seconds.
 *              Without further arguments, the user has to poll with getCO2()
 *              until the value is available. Any super loop or thread
 *              routine, can just consists on reading the CO2 (getCO2()).
 *              For example, measure every 5 minutes:
 *
 *              @code
 *              PASCO2Serial cotwo(serial_intf);
 *              int16_t   co2ppm;
 *
 *              serial_intf.begin();
 *
 *              cotwo.begin();
 *
 *              cotwo.startMeasure(300);
 *
 *              while(1)
 *              {
 *                  delay(300);
 *                  do{ cotwo.getCO2(co2ppm); } while (co2ppm == 0);
 *                  // ... do something with the co2 value ...
 *              }
 *              @endcode
 *
 *              Synching readouts with the hardware interrupt
 *              ---------------------------------------------------------------
 *              In order not to saturate the sensor with constant serial
 *              requests, especially in periodic mode, it is recommended
 *              to synch the readout with a timer. Or even better using
 *              the hardware GPIO hardware interrupt.
 *              If the interrupt pin has been provided, passing
 *              a callback function will enable the interrupt mode. The
 *              type of interrupt is decided depending on the value of the
 *              rest of the arguments and operations modes.
 *              Some example:
 *
 *              @code
 *              volatile bool intFlag = false;
 *              void cback(void *)
 *              {
 *                  intFlag = true;
 *              }
 *
 *              PASCO2Serial cotwo(serial_intf, interrupt);
 *              int16_t   co2ppm;
 *
 *              serial_intf.begin();
 *
 *              cotwo.begin();
 *
 *              cotwo.startMeasure(300,0,cback);
 *
 *              while(1)
 *              {
 *                  while(!intFlag) { // block or yield() };
 *                  cotwo.getCO2(co2ppm);
 *                  // ... do something with the co2 value ...
 *                  intFlag = false;
 *              }
 *              @endcode
 *
 *              Alarm mode
 *              ---------------------------------------------------------------
 *              If the alarm threshold argument is non-zero, the alarm mode
 *              is activated, and the sensor internal flag will be enabled
 *              if the concentration of CO2 goes above the specified value.
 *              This option is better combined with the interupt mode. Thus,
 *              if the interrupt mode is available and a callback function
 *              is passed, the interrupt will occurr only when the co2
 *              concentration goes above the threshold.
 *              This makes mostly sense for periodic measurement configuration.
 *              But it can be used as well for a single shot configuration
 *
 * @param[in]   periodInSec Enables periodic measurement with the specified period.
 *                          The default value is 0, meaning single shot operation.
 *                          The valid period range goes between 5 and 4095 seconds
 * @param[in]   alarmTh     Enables upper alarm threshold mode for the specified
 *                          ppm value
 *                          The default value is 0, meaning no alarm mode.
 *                          For any non-zero value, the sensor will internally set
 *                          the alarm flag. If an interrupt callback function is
 *                          provided, then the interrupt will occurr only when the
 *                          defined threshold has been tresspassed
 * @param[in]   cback       Pointer to the callback function to be called upon
 *                          interrupt
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         begin()
 */
Error_t startMeasure(int16_t periodInSec, int16_t alarmTh, void (*cback) (void *))
{
    xensiv_pasco2_measurement_config_t  measConf;
    xensiv_pasco2_interrupt_config_t intConf;
    int32_t ret = XENSIV_PASCO2_OK;

    /* Get meas configuration*/
    ret = xensiv_pasco2_get_measurement_config(&dev, &measConf);
    INO_ASSERT_RET(ret);
    /**
     * Set the device in idle mode to avoid
     * any conflict if stopMeasure() was not
     * previously called.
     */
    measConf.b.op_mode = XENSIV_PASCO2_OP_MODE_IDLE;

    ret = xensiv_pasco2_set_measurement_config(&dev, measConf);
    INO_ASSERT_RET(ret);
    /* Get int configuration */
    ret = xensiv_pasco2_get_interrupt_config(&dev, &intConf);

    /* Default configuration */
    measConf.b.op_mode = XENSIV_PASCO2_OP_MODE_SINGLE;
    intConf.b.int_func = XENSIV_PASCO2_INTERRUPT_FUNCTION_DRDY;


    if( periodInSec > 0 )
    {
        ret = xensiv_pasco2_set_measurement_rate(&dev, periodInSec);
        INO_ASSERT_RET(ret);
        measConf.b.op_mode = XENSIV_PASCO2_OP_MODE_CONTINUOUS;
    }

    if( alarmTh >  0 )
    {
        ret = xensiv_pasco2_set_alarm_threshold(&dev, alarmTh);
        INO_ASSERT_RET(ret);
        intConf.b.alarm_typ = XENSIV_PASCO2_ALARM_TYPE_LOW_TO_HIGH;
        intConf.b.int_func  = XENSIV_PASCO2_INTERRUPT_FUNCTION_ALARM;
    }
    else
    {
        ret = xensiv_pasco2_set_alarm_threshold(&dev, 0x0000);
        INO_ASSERT_RET(ret);
        intConf.b.alarm_typ = XENSIV_PASCO2_ALARM_TYPE_HIGH_TO_LOW;
    }

    if(cback != NULL)
    {
        /* Enable mcu interupt */
        //attachInterrupt(digitalPinToInterrupt(intPin), (void (*)())cback, RISING);

        /* Enable sensor interrupt */
        intConf.b.int_typ = XENSIV_PASCO2_INTERRUPT_TYPE_HIGH_ACTIVE;
    }
    else
    {
        /* Disable sensor interrupt */
        intConf.b.int_func = XENSIV_PASCO2_INTERRUPT_FUNCTION_NONE;

        /* Disable mcu interrupt */
        //detachInterrupt(digitalPinToInterrupt(intPin));
    }

    ret = xensiv_pasco2_set_interrupt_config(&dev, intConf);
    INO_ASSERT_RET(ret);
    ret = xensiv_pasco2_set_measurement_config(&dev, measConf);

    return  ret;
}

/**
 * @brief       Stops the internal measuring of the sensor
 *
 * @details     Sets operation mode to idle
 *
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         begin()
 */
Error_t stopMeasure()
{
    int32_t ret = XENSIV_PASCO2_OK;

    xensiv_pasco2_measurement_config_t  measConf;

    /* Get meas configuration*/
    ret = xensiv_pasco2_get_measurement_config(&dev, &measConf);

    /* Set meas configuration to idle mode */
    measConf.b.op_mode = XENSIV_PASCO2_OP_MODE_IDLE;
    ret = xensiv_pasco2_set_measurement_config(&dev, measConf);
    INO_ASSERT_RET(ret);
    return ret;
}

/**
 * @brief       Gets the CO2 concentration measured
 *
 *
 * @details     The value read is zero when no measurement is
 *              yet available or an error has ocurrred.
 *
 * @param[out]  co2ppm  CO2 concentration read (in ppm)
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         startMeasure()
 */
Error_t getCO2(uint16_t * CO2PPM)
{
    int32_t ret = XENSIV_PASCO2_OK;

    /* Initially set to 0.*/
    *CO2PPM = 0;

    /* Read the data */
    ret = xensiv_pasco2_get_result(&dev, (uint16_t*)&CO2PPM);
    INO_ASSERT_RET(ret);
    /* Clear masks from status register */
    ret = xensiv_pasco2_clear_measurement_status(&dev,(XENSIV_PASCO2_REG_MEAS_STS_INT_STS_CLR_MSK | XENSIV_PASCO2_REG_MEAS_STS_ALARM_CLR_MSK));

    return ret;
}

/**
 * @brief       Gets diagnosis information
 *
 * @details     The sensor status registers includes the following flags:
 *              - Sensor ready
 *              - PWM pin enabled
 *              - Temperature out of range error
 *              - IR emitter voltage out of range error
 *              - Communication error
 *              which will be stored in the Diag_t struct varible passed by argument.
 *              After reading the flags, these are cleared in the device writing in
 *              the corresponding clear flag bitfields.
 *
 * @param[out]  diagnosis  Struct to store the diagnosis flags values
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         None
 */
Error_t getDiagnosis(Diag_t * diagnosis)
{
    int32_t ret = XENSIV_PASCO2_OK;

    /* Get current status */
    ret = xensiv_pasco2_get_status(&dev, diagnosis);
    INO_ASSERT_RET(ret);
    /* Clear read flags */
    ret = xensiv_pasco2_clear_status(&dev, (XENSIV_PASCO2_REG_SENS_STS_ICCER_CLR_MSK |
                                            XENSIV_PASCO2_REG_SENS_STS_ORVS_CLR_MSK  |
                                            XENSIV_PASCO2_REG_SENS_STS_ORTMP_CLR_MSK ));
    return ret;
}

/**
 * @brief       Configures the sensor automatic baseline compensation
 *
 * @param[in]   aboc        Automatic baseline compenstation mode
 * @param[in]   abocRef     Automatic baseline compensation reference
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         begin()
 */
Error_t setABOC(ABOC_t aboc, int16_t abocRef)
{
    xensiv_pasco2_measurement_config_t  measConf;
    int32_t ret = XENSIV_PASCO2_OK;

    /* Get meas configuration */
    ret = xensiv_pasco2_get_measurement_config(&dev, &measConf);
    INO_ASSERT(ret);
    /* Set compensation offset */
    ret = xensiv_pasco2_set_offset_compensation(&dev, (uint16_t) abocRef);
    INO_ASSERT(ret);
    /* Set meas configuration with ABOC */
    measConf.b.boc_cfg = aboc;
    ret = xensiv_pasco2_set_measurement_config(&dev, measConf);
    INO_ASSERT(ret);
    return ret;
}

/**
 * @brief       Sets the sensor pressure reference
 *
 * @param[in]   pressRef    Pressure reference value. Min value is 600, and max 1600.
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         begin()
 */
Error_t setPressRef(uint16_t pressRef)
{
    int32_t ret = XENSIV_PASCO2_OK;

    ret = xensiv_pasco2_set_pressure_compensation(&dev, pressRef);
    return ret;
}

/**
 * @brief       Resets the sensor via serial command
 *
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         begin()
 */
Error_t reset()
{
    int32_t ret = XENSIV_PASCO2_OK;

    ret = xensiv_pasco2_cmd(&dev, XENSIV_PASCO2_CMD_SOFT_RESET);
    return ret;
}

/**
 * @brief       Gets device product identifier
 *
 * @param[out]  prodID  Product identifier
 * @param[out]  revID   Version identifier
 * @return      XENSIV™ PAS CO2 error code
 * @retval      XENSIV_PASCO2_OK if success
 * @pre         begin()
 */
Error_t getDeviceID(uint8_t * prodID, uint8_t * revID)
{
    int32_t ret = XENSIV_PASCO2_OK;
    xensiv_pasco2_id_t id;

    ret = xensiv_pasco2_get_id(&dev, &id);
    INO_ASSERT_RET(ret);
    *prodID = id.b.prod;
    *revID =  id.b.rev;

    return ret;
}

int32_t xensiv_pasco2_plat_i2c_transfer(void * ctx, uint16_t dev_addr, const uint8_t * tx_buffer, size_t tx_len, uint8_t * rx_buffer, size_t rx_len)
{
    (void)(dev_addr);
    int fd = *(int*)(ctx);
    size_t ret = 0;
    ret = write(fd,tx_buffer,tx_len);
    if( ret != tx_len)
    {
        return XENSIV_PASCO2_ERR_COMM;
    }

    if(NULL != rx_buffer)
    {
        ret = read(fd, rx_buffer, rx_len);
        if( ret != rx_len)
        {
            return XENSIV_PASCO2_ERR_COMM;
        }
    }

   return XENSIV_PASCO2_OK;
}

void xensiv_pasco2_plat_delay(uint32_t ms)
{
    usleep(ms*1000);
}

uint16_t xensiv_pasco2_plat_htons(uint16_t x)
{
    uint16_t rev_x = ((x & 0xFF) << 8) | ((x & 0xFF00) >> 8);

    return rev_x;
}

void xensiv_pasco2_plat_assert(int expr)
{
    INO_ASSERT(expr);
}
