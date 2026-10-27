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

/** See sensor_cycle.h. */
void SensorStatsNoteFail(SensorStats *pSt)
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
 * before that gap. A failed bus reset and a soft-reset NACK each count as a reset error. A NACK
 * skips the settle.
 */
static void Recover(const SensorBus *pB, SensorStats *pSt)
{
    U8 cmd = SENSOR_CMD_RESET;
    if (pB->busReset(pB->pCtx) != 0)
        Bump(&pSt->resetErr);
    Bump(&pSt->busResets);
    if (pB->tx(pB->pCtx, &cmd, 1) != 0)
        Bump(&pSt->resetErr);
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
            Bump(&pSt->measErr);
        Recover(pB, pSt);
    }
    SensorStatsNoteFail(pSt);
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
        Bump(&pSt->resetErr);
        if (pB->busReset(pB->pCtx) != 0)
            Bump(&pSt->resetErr);
        Bump(&pSt->busResets);
        pB->delayMs(pB->pCtx, SENSOR_RETRY_GAP_MS);
    }
    return -1;
}
