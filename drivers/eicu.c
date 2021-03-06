/* *
 *
 * Hardware Abstraction Layer for Extended Input Capture Unit
 *
 * */



#include "ch.h"
#include "hal.h"
#include "eicu.h" /* Should be in hal.h but is not a part of ChibiOS */

#if HAL_USE_EICU || defined(__DOXYGEN__)


/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   EICU Driver initialization.
 *
 * @init
 */

void eicuInit(void) {

  eicu_lld_init();
}

/**
 * @brief   Initializes the standard part of a @p EICUDriver structure.
 *
 * @param[out] eicup    Pointer to the @p EICUDriver object
 *
 * @init
 */
void eicuObjectInit(EICUDriver *eicup) {

  eicup->state  = EICU_STOP;
  eicup->config = NULL;
}

/**
 * @brief   Configures and activates the EICU peripheral.
 *
 * @param[in] eicup     Pointer to the @p EICUDriver object
 * @param[in] config    Pointer to the @p EICUConfig object
 *
 * @api
 */
void eicuStart(EICUDriver *eicup, const EICUConfig *config) {

  osalDbgCheck((eicup != NULL) && (config != NULL));

  osalSysLock();
  osalDbgAssert((eicup->state == EICU_STOP) || (eicup->state == EICU_READY),
                "invalid state");
  eicup->config = config;
  eicu_lld_start(eicup);
  eicup->state = EICU_READY;
  osalSysUnlock();
}

/**
 * @brief   Deactivates the EICU peripheral.
 *
 * @param[in] eicup     Pointer to the @p EICUDriver object
 *
 * @api
 */
void eicuStop(EICUDriver *eicup) {

  osalDbgCheck(eicup != NULL);

  osalSysLock();
  osalDbgAssert((eicup->state == EICU_STOP) || (eicup->state == EICU_READY),
                "invalid state");
  eicu_lld_stop(eicup);
  eicup->state = EICU_STOP;
  osalSysUnlock();
}

/**
 * @brief   Enables the extended input capture.
 *
 * @param[in] eicup     Pointer to the @p EICUDriver object
 *
 * @api
 */
void eicuEnable(EICUDriver *eicup) {

  osalDbgCheck(eicup != NULL);

  osalSysLock();
  osalDbgAssert(eicup->state == EICU_READY, "invalid state");
  eicu_lld_enable(eicup);
  eicup->state = EICU_WAITING;
  osalSysUnlock();
}

/**
 * @brief   Disables the extended input capture.
 *
 * @param[in] eicup     Pointer to the @p EICUDriver object
 *
 * @api
 */
void eicuDisable(EICUDriver *eicup) {

  osalDbgCheck(eicup != NULL);

  osalSysLock();
  osalDbgAssert((eicup->state == EICU_READY) || (eicup->state == EICU_WAITING) ||
                (eicup->state == EICU_ACTIVE) || (eicup->state == EICU_IDLE),
                 "invalid state");
  eicu_lld_disable(eicup);
  eicup->state = EICU_READY;
  osalSysUnlock();
}

#endif /* HAL_USE_EICU */