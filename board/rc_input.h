#ifndef __RC_INPUT_H
#define __RC_INPUT_H

/* Defines */
#define RCINPUT_1MHZ_CLOCK_FREQUENCY        1000000
#define MAX_NUMBER_OF_INPUTS        12
#define CPPM_SYNC_LIMIT_MIN         3000    /* 3 ms */
#define CPPM_SYNC_LIMIT_MAX         30000   /* 30 ms */
#define RSSI_THRESHOLD_PERCENT      5       /* RSSI threshold in percent */
#define RSSI_TIMEOUT                100     /* Number of bad RSSI measurements
                                               to disable connection */

/* Global variable defines */

/* Typedefs */
typedef enum {
    INPUT_CH1 = 0,
    INPUT_CH2 = 1,
    INPUT_CH3 = 2,
    INPUT_CH4 = 3,
    INPUT_CH5 = 4,
    INPUT_CH6 = 5
} RCInput_Capture_Channel;

typedef enum {
    MODE_CPPM_INPUT = 1,
    MODE_PWM_INPUT = 2
} RCInput_Mode_Selector;

typedef struct {
    uint32_t active_connection;
    uint16_t number_active_connection;
    uint16_t value[MAX_NUMBER_OF_INPUTS];
    uint16_t rssi;
    uint16_t rssi_frequency;
} RCInput_Data;

/* Global function defines */
msg_t RCInputInit();

#endif
