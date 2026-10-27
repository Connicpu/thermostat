/* Host tests for the SHT40 retry cycle. The bus is a function-pointer seam; no I2C driver. */
#include "core.h"
#include "tinytest.h"
#include "sensor_cycle.h"
#include "sht40_parse.h"
#include <string.h>

/**
 * Scripted bus. txFail/rxFail are nonzero for that call index. crcBad corrupts the frame on that rx.
 * A call past the scripted prefix succeeds.
 */
typedef struct
{
    int txFail[8];
    int rxFail[8];
    int crcBad[8];
    int nTx, nRx, nReset, resetFail;
    U32 delayMs[8];
    int nDelay;
    U8 cmd[8];
} Fake;

/** Build the 25.00 °C / 56.50 %RH frame used by the parser test. */
static void GoodFrame(U8 f[6])
{
    f[0] = 0x66;
    f[1] = 0x66;
    f[2] = 0;
    f[3] = 0x80;
    f[4] = 0x00;
    f[5] = 0;
    f[2] = Sht40Crc8(f, 2);
    f[5] = Sht40Crc8(f + 3, 2);
}

/** Record the command and fail when the script says so. */
static int Tx(void *pCtx, const U8 *pBuf, size_t n)
{
    Fake *pF = pCtx;
    int i = pF->nTx++;
    (void)n;
    pF->cmd[i] = pBuf[0];
    return i < 8 && pF->txFail[i] ? -1 : 0;
}

/** Return a good frame, a CRC-bad frame, or a failure. */
static int Rx(void *pCtx, U8 *pBuf, size_t n)
{
    Fake *pF = pCtx;
    int i = pF->nRx++;
    (void)n;
    if (i < 8 && pF->rxFail[i])
        return -1;
    GoodFrame(pBuf);
    if (i < 8 && pF->crcBad[i])
        pBuf[2] ^= 0x5A;
    return 0;
}

/** Count a bus reset. Returns failure when the script says so. */
static int BusReset(void *pCtx)
{
    Fake *pF = pCtx;
    pF->nReset++;
    return pF->resetFail ? -1 : 0;
}

/** Record a wait. */
static void Delay(void *pCtx, U32 ms)
{
    Fake *pF = pCtx;
    if (pF->nDelay < 8)
        pF->delayMs[pF->nDelay++] = ms;
}

/** The seam bound to f. */
static SensorBus Bus(Fake *pF)
{
    return (SensorBus){.pCtx = pF, .tx = Tx, .rx = Rx, .busReset = BusReset, .delayMs = Delay};
}

/** A clean measurement is one transfer pair, a 10 ms wait, and an ok count. No reset. */
static void FirstAttemptSucceeds(void)
{
    Fake f;
    SensorStats st;
    SensorReading r;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    SensorBus b = Bus(&f);
    r = SensorCycle(&b, &st, 10);
    CHECK(r.valid);
    CHECK_EQ(r.tempC100, 2510);
    CHECK_EQ(r.rhC100, 5650);
    CHECK_EQ(f.nTx, 1);
    CHECK_EQ(f.nRx, 1);
    CHECK_EQ(f.nReset, 0);
    CHECK_EQ(SENSOR_CMD_MEAS, 0xFD);
    CHECK_EQ(SENSOR_CMD_RESET, 0x94);
    CHECK_EQ(f.cmd[0], 0xFD);
    CHECK_EQ(f.nDelay, 1);
    CHECK_EQ(f.delayMs[0], SENSOR_MEAS_WAIT_MS);
    CHECK_EQ(st.ok, 1u);
    CHECK_EQ(st.measErr, 0u);
    CHECK_EQ(st.resetErr, 0u);
    CHECK_EQ(st.crcErr, 0u);
    CHECK_EQ(st.retries, 0u);
    CHECK_EQ(st.failRun, 0u);
}

/** A CRC failure resets the bus, soft-resets, settles 2 ms, waits 20 ms, and the next attempt counts. */
static void CrcThenOkUsesOneRetry(void)
{
    Fake f;
    SensorStats st;
    SensorReading r;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.crcBad[0] = 1;
    b = Bus(&f);
    r = SensorCycle(&b, &st, 0);
    CHECK(r.valid);
    CHECK_EQ(r.tempC100, 2500);
    CHECK_EQ(st.ok, 1u);
    CHECK_EQ(st.crcErr, 1u);
    CHECK_EQ(st.measErr, 0u);
    CHECK_EQ(st.resetErr, 0u);
    CHECK_EQ(st.retries, 1u);
    CHECK_EQ(st.busResets, 1u);
    CHECK_EQ(f.nReset, 1);
    CHECK_EQ(f.cmd[0], 0xFD);
    CHECK_EQ(f.cmd[1], 0x94);
    CHECK_EQ(f.cmd[2], 0xFD);
    CHECK_EQ(f.delayMs[1], SENSOR_RESET_SETTLE_MS);
    CHECK_EQ(f.delayMs[2], SENSOR_RETRY_GAP_MS);
    CHECK_EQ(st.failRun, 0u);
    CHECK_EQ(st.failRunMax, 0u);
}

/** Three I2C failures stop. Each one resets the bus, including the last, and the failed run grows. */
static void ThreeI2cFailuresThenAGoodCycle(void)
{
    Fake f;
    SensorStats st;
    SensorReading r;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.txFail[0] = f.txFail[1] = f.txFail[2] = f.txFail[3] = f.txFail[4] = f.txFail[5] = 1;
    b = Bus(&f);
    r = SensorCycle(&b, &st, 0);
    CHECK(!r.valid);
    CHECK_EQ(r.tempC100, 0);
    CHECK_EQ(f.nTx, 6); /* measure + soft reset, three times */
    CHECK_EQ(f.nRx, 0);
    CHECK_EQ(f.nReset, 3);
    for (int i = 0; i < 6; i++)
        CHECK_EQ(f.cmd[i], (i % 2) == 0 ? 0xFD : 0x94);
    CHECK_EQ(st.measErr, 3u);
    CHECK_EQ(st.resetErr, 3u); /* the soft-reset NACKs, not the measurement failures */
    CHECK_EQ(st.retries, 2u);
    CHECK_EQ(st.busResets, 3u);
    CHECK_EQ(st.ok, 0u);
    CHECK_EQ(st.failRun, 1u);
    CHECK_EQ(st.failRunMax, 1u);
    CHECK_EQ(f.delayMs[0], SENSOR_RETRY_GAP_MS);
    memset(&f, 0, sizeof f);
    f.txFail[0] = f.txFail[1] = f.txFail[2] = f.txFail[3] = f.txFail[4] = f.txFail[5] = 1;
    b = Bus(&f);
    r = SensorCycle(&b, &st, 0);
    CHECK(!r.valid);
    CHECK_EQ(st.failRun, 2u);
    CHECK_EQ(st.failRunMax, 2u);
    memset(&f, 0, sizeof f);
    b = Bus(&f);
    r = SensorCycle(&b, &st, 0);
    CHECK(r.valid);
    CHECK_EQ(st.ok, 1u);
    CHECK_EQ(st.failRun, 0u);
    CHECK_EQ(st.failRunMax, 2u);
}

/** A receive NAK is retried the same way as a CRC failure, and a later success is the reading. */
static void ReceiveNakThenOk(void)
{
    Fake f;
    SensorStats st;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.rxFail[0] = 1;
    b = Bus(&f);
    CHECK(SensorCycle(&b, &st, 0).valid);
    CHECK_EQ(st.measErr, 1u);
    CHECK_EQ(st.resetErr, 0u);
    CHECK_EQ(st.retries, 1u);
    CHECK_EQ(st.busResets, 1u);
    CHECK_EQ(f.nRx, 2);
}

/** Soft reset tries again after a NACK and settles for 2 ms once a reset is ACKed. */
static void SoftResetRetriesThenAcks(void)
{
    Fake f;
    SensorStats st;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.txFail[0] = 1;
    b = Bus(&f);
    CHECK_EQ(SensorSoftReset(&b, &st), 0);
    CHECK_EQ(st.resetErr, 1u);
    CHECK_EQ(st.measErr, 0u);
    CHECK_EQ(st.busResets, 1u);
    CHECK_EQ(st.failRun, 0u);
    CHECK_EQ(f.cmd[0], 0x94);
    CHECK_EQ(f.cmd[1], 0x94);
    CHECK_EQ(f.nDelay, 2);
    CHECK_EQ(f.delayMs[0], SENSOR_RETRY_GAP_MS);
    CHECK_EQ(f.delayMs[1], SENSOR_RESET_SETTLE_MS);
}

/** Three NACKs and the bring-up gives up. No measurement counter moves. */
static void SoftResetGivesUp(void)
{
    Fake f;
    SensorStats st;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.txFail[0] = f.txFail[1] = f.txFail[2] = 1;
    b = Bus(&f);
    CHECK(SensorSoftReset(&b, &st) != 0);
    CHECK_EQ(f.nTx, SENSOR_ATTEMPTS);
    CHECK_EQ(st.resetErr, 3u);
    CHECK_EQ(st.measErr, 0u);
    CHECK_EQ(st.busResets, 3u);
    CHECK_EQ(st.ok, 0u);
    CHECK_EQ(st.failRun, 0u);
}

/** A bus reset that returns failure counts even when the soft reset that follows is ACKed. */
static void BusResetFailureCounts(void)
{
    Fake f;
    SensorStats st;
    SensorReading r;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.txFail[0] = 1;
    f.resetFail = 1;
    b = Bus(&f);
    r = SensorCycle(&b, &st, 0);
    CHECK(r.valid);
    CHECK_EQ(st.measErr, 1u);
    CHECK_EQ(st.resetErr, 1u);
    CHECK_EQ(st.busResets, 1u);
    CHECK_EQ(f.cmd[0], 0xFD);
    CHECK_EQ(f.cmd[1], 0x94);
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    f.txFail[0] = 1;
    f.resetFail = 1;
    b = Bus(&f);
    CHECK_EQ(SensorSoftReset(&b, &st), 0);
    CHECK_EQ(st.resetErr, 2u); /* the NACK and the failed bus reset */
    CHECK_EQ(st.busResets, 1u);
    CHECK_EQ(st.measErr, 0u);
    CHECK_EQ(st.failRun, 0u);
}

/** Counters stop at 0xFFFFFFFF. A later good cycle still closes the failed run. */
static void CountersSaturate(void)
{
    Fake f;
    SensorStats st;
    SensorBus b;
    memset(&f, 0, sizeof f);
    memset(&st, 0, sizeof st);
    st.measErr = 0xfffffffeu;
    f.txFail[0] = 1;
    b = Bus(&f);
    CHECK(SensorCycle(&b, &st, 0).valid);
    CHECK_EQ(st.measErr, 0xffffffffu);
    memset(&f, 0, sizeof f);
    memset(&st, 0xff, sizeof st);
    f.txFail[0] = f.txFail[1] = f.txFail[2] = f.txFail[3] = f.txFail[4] = f.txFail[5] = 1;
    b = Bus(&f);
    CHECK(!SensorCycle(&b, &st, 0).valid);
    CHECK_EQ(st.ok, 0xffffffffu);
    CHECK_EQ(st.measErr, 0xffffffffu);
    CHECK_EQ(st.resetErr, 0xffffffffu);
    CHECK_EQ(st.crcErr, 0xffffffffu);
    CHECK_EQ(st.retries, 0xffffffffu);
    CHECK_EQ(st.busResets, 0xffffffffu);
    CHECK_EQ(st.failRun, 0xffffffffu);
    CHECK_EQ(st.failRunMax, 0xffffffffu);
    memset(&f, 0, sizeof f);
    b = Bus(&f);
    CHECK(SensorCycle(&b, &st, 0).valid);
    CHECK_EQ(st.ok, 0xffffffffu);
    CHECK_EQ(st.failRun, 0u);
    CHECK_EQ(st.failRunMax, 0xffffffffu);
}

/** Run every sensor-cycle test. */
int main(void)
{
    RUN(FirstAttemptSucceeds);
    RUN(CrcThenOkUsesOneRetry);
    RUN(ThreeI2cFailuresThenAGoodCycle);
    RUN(ReceiveNakThenOk);
    RUN(SoftResetRetriesThenAcks);
    RUN(SoftResetGivesUp);
    RUN(BusResetFailureCounts);
    RUN(CountersSaturate);
    DONE();
}
