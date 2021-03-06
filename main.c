#include "ch.h"
#include "hal.h"
#include "myusb.h"
#include "mpu6050.h"
#include "hmc5983.h"
#include "sensor_calibration.h"
#include "sensor_read.h"
#include "rc_output.h"
#include "eicu.h"
#include "chprintf.h"

volatile const char *kfly_error;

/* I2C interface #2 Configuration */
static const I2CConfig i2cfg2 = {
    OPMODE_I2C,
    400000,
    FAST_DUTY_CYCLE_2,
};

/* MPU6050 Configuration */
static Sensor_Calibration mpu6050cal = {
    .bias = {0.0f, 0.0f, 0.0f},
    .gain = {1.0f, 1.0f, 1.0f}
};
static MPU6050_Data mpu6050data;
static const MPU6050_Configuration mpu6050cfg = {
    MPU6050_DLPF_BW_42,             /* Digital low-pass filter config   */
    MPU6050_EXT_SYNC_DISABLED,      /* External sync config             */
    MPU6050_GYRO_FS_2000,           /* Gyro range config                */
    MPU6050_ACCEL_FS_16,            /* Accel range config               */
    MPU6050_FIFO_DISABLED,          /* FIFO config                      */
    MPU6050_INTMODE_ACTIVEHIGH |
    MPU6050_INTDRIVE_PUSHPULL  |
    MPU6050_INTLATCH_50USPULSE |
    MPU6050_INTCLEAR_ANYREAD,       /* Interrupt config                 */
    MPU6050_INTDRDY_ENABLE,         /* Interrupt enable config          */
    MPU6050_ADDRESS_AD0_HIGH,       /* MPU6050 address                  */
    MPU6050_CLK_X_REFERENCE,        /* Clock reference                  */
    4,                              /* Sample rate divider              */
    &mpu6050data,                   /* Pointer to data holder           */
    &I2CD2                          /* Pointer to I2C Driver            */
};

/* HMC5983 Configuration */
static Sensor_Calibration hmc5983cal = {
    .bias = {0.0f, 0.0f, 0.0f},
    .gain = {1.0f, 1.0f, 1.0f}
};
static HMC5983_Data hmc5983data;
static const HMC5983_Configuration hmc5983cfg = {
    HMC5983_TEMPERATURE_ENABLE,     /* Enable/disable temperature sensor */
    HMC5983_AVERAGE_8_SAMPLES,      /* Sample averaging config           */
    HMC5983_DATA_RATE_75D0_HZ,      /* Output data rate config           */
    HMC5983_MEAS_MODE_NORMAL,       /* Measurement mode config           */
    HMC5983_GAIN_1D3_GA,            /* Gain config                       */
    HMC5983_OP_MODE_CONTINOUS,      /* Operating mode config             */
    HMC5983_I2C_FAST_DISABLE,       /* Enable/disable 3.4 MHz I2C        */
    HMC5983_LOW_POWER_DISABLE,      /* Enable/disable low power mode     */
    HMC5983_ADDRESS,                /* HMC5983 address                   */
    &hmc5983data,                   /* Pointer to data holder            */
    &I2CD2                          /* Pointer to I2C Driver             */
};

/* External interrupt configuration */
static const EXTConfig extcfg = {
    {
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_RISING_EDGE    |
         EXT_CH_MODE_AUTOSTART      |
         EXT_MODE_GPIOC, HMC5983cb},    /* 13: HMC5983 IRQ      */
        {EXT_CH_MODE_FALLING_EDGE   |
         EXT_CH_MODE_AUTOSTART      |
         EXT_MODE_GPIOC, MPU6050cb},    /* 14: MPU6050 IRQ      */
        {EXT_CH_MODE_DISABLED, NULL},   /* 15: RF Module IRQ    */
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL},
        {EXT_CH_MODE_DISABLED, NULL}
    }
};

/* RC Output Configuration */
static const PWMConfig pwmcfg = {
    RCOUTPUT_1MHZ_CLOCK_FREQUENCY,      /* 1 MHz PWM clock frequency    */
    RCOUTPUT_400HZ,                     /* Initial PWM period: 400 Hz   */
    NULL,                               /* No callback */
    {
        {PWM_OUTPUT_ACTIVE_HIGH, NULL}, /* Active high, no callback     */
        {PWM_OUTPUT_ACTIVE_HIGH, NULL}, /* Active high, no callback     */
        {PWM_OUTPUT_ACTIVE_HIGH, NULL}, /* Active high, no callback     */
        {PWM_OUTPUT_ACTIVE_HIGH, NULL}  /* Active high, no callback     */
    },
    0,
    0
};
static const RCOutput_Configuration rcoutputcfg = {
    &PWMD4,
    &PWMD8,
    &pwmcfg
};

/* EICU Configuration */
static uint16_t ic_test;
static void testwidthcb(EICUDriver *eicup, eicuchannel_t channel)
{
    (void)eicup;
    (void)channel;
    ic_test = eicuGetWidth(eicup, channel);
    palTogglePad(GPIOC, GPIOC_LED_ERR);
}

static const EICU_IC_Settings ic1settings = {
    EICU_INPUT_ACTIVE_HIGH,
    testwidthcb
};
static const EICUConfig rcinputcfg = {
    EICU_INPUT_PULSE,
    1000000,
    {&ic1settings, NULL, NULL, NULL},
    NULL,
    NULL,
    EICU_PWM_CHANNEL_1,
    0
};


void panic(const char *err);

static THD_WORKING_AREA(waThreadTestEvents, 128);
static THD_FUNCTION(ThreadTestEvents, arg)
{
    (void)arg;
    event_listener_t el;
    eventmask_t events;
    int i = 0;

    chEvtRegisterMask(  &hmc5983cfg.data_holder->es,
                        &el,
                        HMC5983_DATA_AVAILABLE_EVENTMASK);  
    
    while(1)
    {
        events = chEvtWaitOne(HMC5983_DATA_AVAILABLE_EVENTMASK);

        if (events == HMC5983_DATA_AVAILABLE_EVENTMASK)
        {
            if (i++ > 10)
            {
                palTogglePad(GPIOC, GPIOC_LED_USR);
                i = 0;
            }
        }
    }

    return MSG_OK;
}

int main(void)
{
    /*
     * System initializations.
     * - HAL initialization, this also initializes the configured 
     *   device drivers and performs the board-specific initializations.
     * - Kernel initialization, the main() function becomes a thread
     *   and the RTOS is active.
     */
    halInit();
    chSysInit();

    /*
     *
     * Initializes a serial-over-USB CDC driver.
     * 
     */
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    /*
     *
     * Activates the USB driver and then the USB bus pull-up on D+.
     * 
     */
    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(500);
    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);

    /*
     *
     * Start I2C and set up sensors
     * 
     */
    i2cStart(&I2CD2, &i2cfg2);

    /* Initialize Accelerometer and Gyroscope */
    if (MPU6050Init(&mpu6050cfg) != MSG_OK)
        panic("MPU6050 init failed"); /* Initialization failed */

    /* Initialize Magnetometer */
    if (HMC5983Init(&hmc5983cfg) != MSG_OK)
        panic("HMC5983 init failed"); /* Initialization failed */

    /* Initialize Barometer */
    /* TODO: Add barometer code */

    /* Initialize sensor readout */
    if (SensorReadInit(&mpu6050cfg, &hmc5983cfg,
                       &mpu6050cal, &hmc5983cal) != MSG_OK)
        panic("Sensor Read init failed"); /* Initialization failed */

    /*
     *
     * Start the external interrupts
     *
     */
    extStart(&EXTD1, &extcfg);

    /*
     *
     * Initialize the RC Outputs
     *
     */
    if (RCOutputInit(&rcoutputcfg) != MSG_OK)
        panic("RC output init failed"); /* Initialization failed */

    /*
     *
     * Start RC Inputs
     *
     */
    eicuInit();
    eicuStart(&EICUD9, &rcinputcfg);
    eicuEnable(&EICUD9);

    /*
     *
     * Start test thread
     * 
     */
    chThdCreateStatic(  waThreadTestEvents,
                        sizeof(waThreadTestEvents),
                        HIGHPRIO,
                        ThreadTestEvents,
                        NULL);

    while(1)
    {
        chThdSleepMilliseconds(100);
        if (isUSBActive() == true)
            chprintf((BaseSequentialStream *)&SDU1, "Input width: %u\r\n", ic_test);
    }
}

void panic(const char *err)
{
    kfly_error = err;

    while (1)
    {
        palClearPad(GPIOC, GPIOC_LED_ERR);
        chThdSleepMilliseconds(200);
        palSetPad(GPIOC, GPIOC_LED_ERR);
        chThdSleepMilliseconds(200);
    }
}
