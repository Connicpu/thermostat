/* sensor_cycle.c - see sensor_cycle.h. */
#include "core.h"
#include "sensor_cycle.h"
#include "sht40_parse.h"

/** Saturating increment of a counter. */
static void Bump(U32 *pN)
{
    if (*pN < 0xFFFFFFFFu)
        (*pN)++;
}

/** Record a cycle that returned a reading, and close the current failed run. */
static void NoteOk(SensorStats *pSt)
{
    Bump(&pSt->ok);
    if (pSt->failRun > pSt->failRunMax)
        pSt->failRunMax = pSt->failRun;
    pSt->failRun = 0;
}

/** Record a cycle that produced no reading. */
static void NoteFail(SensorStats *pSt)
{
    Bump(&pSt->failRun);
    if (pSt->failRun > pSt->failRunMax)
        pSt->failRunMax = pSt->failRun;
}

/**
 * One high-repeatability measurement.
 *
 * Returns 0 and writes the parsed sample on success, 1 on an I2C failure, 2 on a CRC failure.
 */
static int Measure(const SensorBus *pB, I16 *pT, U16 *pH)
{
    U8 cmd = SENSOR_CMD_MEAS;
    U8 f[6];
    if (pB->tx(pB->pCtx, &cmd, 1) != 0)
        return 1;
    pB->delayMs(pB->pCtx, SENSOR_MEAS_WAIT_MS);
    if (pB->rx(pB->pCtx, f, 6) != 0)
        return 1;
    if (!Sht40Parse(f, pT, pH))
        return 2;
    return 0;
}

/**
 * Bus reset, soft reset, then the inter-attempt gap. An ACK settles for SENSOR_RESET_SETTLE_MS
 * before that gap. A soft-reset NACK is an I2C error too, and skips the settle.
 */
static void Recover(const SensorBus *pB, SensorStats *pSt)
{
    U8 cmd = SENSOR_CMD_RESET;
    pB->busReset(pB->pCtx);
    Bump(&pSt->busResets);
    if (pB->tx(pB->pCtx, &cmd, 1) != 0)
        Bump(&pSt->i2cErr);
    else
        pB->delayMs(pB->pCtx, SENSOR_RESET_SETTLE_MS);
    pB->delayMs(pB->pCtx, SENSOR_RETRY_GAP_MS);
}

/** See sensor_cycle.h. */
SensorReading SensorCycle(const SensorBus *pB, SensorStats *pSt, I16 tempOffsetC100)
{
    SensorReading r = {0};
    for (int i = 0; i < SENSOR_ATTEMPTS; i++)
    {
        I16 t = 0;
        U16 h = 0;
        int rc;
        if (i > 0)
            Bump(&pSt->retries);
        rc = Measure(pB, &t, &h);
        if (rc == 0)
        {
            r.valid = true;
            r.tempC100 = (I16)(t + tempOffsetC100);
            r.rhC100 = h;
            NoteOk(pSt);
            return r;
        }
        if (rc == 2)
            Bump(&pSt->crcErr);
        else
            Bump(&pSt->i2cErr);
        Recover(pB, pSt);
    }
    NoteFail(pSt);
    return r;
}

/** See sensor_cycle.h. */
int SensorSoftReset(const SensorBus *pB, SensorStats *pSt)
{
    U8 cmd = SENSOR_CMD_RESET;
    for (int i = 0; i < SENSOR_ATTEMPTS; i++)
    {
        if (pB->tx(pB->pCtx, &cmd, 1) == 0)
        {
            pB->delayMs(pB->pCtx, SENSOR_RESET_SETTLE_MS);
            return 0;
        }
        Bump(&pSt->i2cErr);
        pB->busReset(pB->pCtx);
        Bump(&pSt->busResets);
        pB->delayMs(pB->pCtx, SENSOR_RETRY_GAP_MS);
    }
    return -1;
}
