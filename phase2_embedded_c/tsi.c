/*******************************************************************************
*
* tsi.c
*
* Low level driver for the Kinetis TSI module.
*
* API: gsiInit(), tsiRead(), tsiReadRaw()
*
* David Kennedy
* August 27 2012
*******************************************************************************/
#include "kinetis.h"

#include "hardware.h"
#include "globalDefs.h"

static const struct {
    uint32_t portEnable;
    uint32_t port;
    uint32_t pin;
} tsiMap[TSI_COUNT] = {
/* 0*/{SIM_PORTB_ENABLE, PORTB,  0}, {SIM_PORTA_ENABLE, PORTA,  0},
/* 2*/{SIM_PORTA_ENABLE, PORTA,  1}, {SIM_PORTA_ENABLE, PORTA,  2},
/* 4*/{SIM_PORTA_ENABLE, PORTA,  3}, {SIM_PORTA_ENABLE, PORTA,  4},
/* 6*/{SIM_PORTB_ENABLE, PORTB,  1}, {SIM_PORTB_ENABLE, PORTB,  2},
/* 8*/{SIM_PORTB_ENABLE, PORTB,  3}, {SIM_PORTB_ENABLE, PORTB, 16},
/*10*/{SIM_PORTB_ENABLE, PORTB, 17}, {SIM_PORTB_ENABLE, PORTB, 18},
/*12*/{SIM_PORTB_ENABLE, PORTB, 19}, {SIM_PORTC_ENABLE, PORTC,  0},
/*14*/{SIM_PORTC_ENABLE, PORTC,  1}, {SIM_PORTC_ENABLE, PORTC,  2},
};

static uint16_t minCntr[TSI_COUNT];

static void tsiDisable()
{
    TSI0_GENCS &= ~TSI_GENCS_TSIEN;
}

static void tsiEnable()
{
    TSI0_GENCS |= TSI_GENCS_TSIEN;
}

static void tsiEnablePin(int pin)
{
    assert(pin >= 0 && pin < TSI_COUNT);

    SIM_SCGC5 |= tsiMap[pin].portEnable;
    PORT_PCR(tsiMap[pin].port, tsiMap[pin].pin) = PORT_IRQC_DISABLED |
                                                                PORT_MUX_ANALOG;
    TSI0_THRESHOLD[pin] = (1 << TSI_THRESHOLD_LTHH_SHIFT) |
                                           (0xFFFE << TSI_THRESHOLD_HTHH_SHIFT);
    TSI0_PEN |= 1 << pin;

    minCntr[pin] = 0xFFFF;
}

static void tsiSetScanc(uint32_t scanc)
{
    TSI0_SCANC = scanc;
}

static void tsiSetPrescale(uint16_t prescale)
{
    TSI0_GENCS = TSI0_GENCS & ~TSI_GENCS_PS_MASK
                       | ((prescale << TSI_GENCS_PS_SHIFT) & TSI_GENCS_PS_MASK);
}

int32_t tsiInit(const tsiConfig_t *cfg)
{
    int pin;

    SIM_SCGC5 |= SIM_TSI_ENABLE;

    tsiDisable();

    TSI0_PEN = 0;
    for (pin=0; pin < TSI_COUNT; pin ++) {
        if (cfg->pinEnable & (1 << pin)) {
            tsiEnablePin(pin);
        }
        minCntr[pin] = 0xFFFF;
    }

    tsiSetScanc(cfg->scanc);
    TSI0_GENCS = ((1-1) << TSI_GENCS_NSCN_SHIFT) | TSI_GENCS_STM;
    tsiSetPrescale(cfg->prescale);

    tsiEnable();

    return 1;
}

uint32_t tsiRead(const tsiConfig_t *cfg)
{
    int pin;
    uint32_t result = 0;
    uint16_t value;

    for (pin=0; pin < TSI_COUNT; pin++) {
        if (cfg->pinEnable & (1 << pin)) {
            value = TSI0_CNTR[pin];
            if (value < minCntr[pin]) {
                minCntr[pin] = value;
            }
            if (value > minCntr[pin] + cfg->threshold[pin]) {
                result |= (1 << pin);
            }
        }
    }
    return result;
}

uint32_t tsiReadRaw(uint32_t pin)
{
    uint16_t value = 0;
    assert((pin < TSI_COUNT));

    if (pin < TSI_COUNT) {
        value = TSI0_CNTR[pin];
        if (value < minCntr[pin]) {
            minCntr[pin] = value;
        }
    }

    return value;
}

/******************************************************************************/
typedef struct tsiPriv_s {
    bool        open;
    tsiConfig_t tsiConfig;
    int         readPos;
} tsiPriv_t;

static tsiPriv_t tsiPriv;

int tsi_install(void)
{
    int ret = TRUE;
    tsiPriv.open = FALSE;
    if (!deviceInstall("tsi0", tsi_open_r, tsi_ioctl, tsi_close_r, tsi_write_r,
                                                        tsi_read_r, &tsiPriv)) {
        ret = FALSE;
    }
    return ret;
}

/******************************************************************************/
int tsi_open_r(void *reent, devoptab_t *dot, int mode, int flags)
{
    tsiPriv_t *priv = (tsiPriv_t *)dot->priv;
    int tsi;

    if (priv->open) {
        return FALSE;
    }
    priv->tsiConfig.pinEnable = 0;
    priv->tsiConfig.scanc = TSI_SCANC_DEFAULT;
    priv->tsiConfig.prescale = 0;
    for (tsi = 0; tsi < TSI_COUNT; tsi++) {
        priv->tsiConfig.threshold[tsi] = 0;
    }
    priv->readPos = 0;

    if (tsiInit(&priv->tsiConfig)) {
        priv->open = TRUE;
        return TRUE;
    }
    return FALSE;
}

/******************************************************************************/
int tsi_ioctl(devoptab_t *dot, int cmd, int flags)
{
    tsiPriv_t *priv = (tsiPriv_t *)dot->priv;

    if (!priv->open) {
        return FALSE;
    }

    switch (cmd) {
    case IO_IOCTL_TSI_CONFIGURE:
        priv->tsiConfig.scanc = flags;

        tsiDisable();
        tsiSetScanc(priv->tsiConfig.scanc);
        tsiEnable();
        return TRUE;

    case IO_IOCTL_TSI_SET_PRESCALE:
        priv->tsiConfig.prescale = flags;

        tsiDisable();
        tsiSetPrescale(priv->tsiConfig.prescale);
        tsiEnable();
        return TRUE;

    case IO_IOCTL_TSI_CONFIGURE_PIN:
        {
            tsiConfigure_t *cfg = (tsiConfigure_t *)flags;
            if (cfg->pin >= TSI_COUNT) {
                return FALSE;
            }
            priv->tsiConfig.pinEnable |= 1 << cfg->pin;
            priv->tsiConfig.threshold[cfg->pin] = cfg->threshold;

            tsiDisable();
            tsiEnablePin(cfg->pin);
            tsiEnable();
        }
        return TRUE;

    default:
        assert(0);
        return FALSE;
    }
}

/******************************************************************************/
int tsi_close_r(void *reent, devoptab_t *dot)
{
    tsiPriv_t *priv = (tsiPriv_t *)dot->priv;

    if (priv->open) {
        priv->open = FALSE;
        return TRUE;
    }
    else {
        return FALSE;
    }
}

/******************************************************************************/
long tsi_write_r(void *reent, devoptab_t *dot, const void *buf, int len)
{
    tsiPriv_t *priv = (tsiPriv_t *)dot->priv;
    (void)priv;

    return len;
}

/******************************************************************************/
long tsi_read_r(void *reent, devoptab_t *dot, void *buf, int len)
{
    tsiPriv_t *priv = (tsiPriv_t *)dot->priv;
    uint16_t data;
    unsigned char *p = buf;
    long actual = 0;

    while (len > 0) {
        data = tsiRead(&priv->tsiConfig);

        switch (priv->readPos) {
        case 0:
        default:
            *p = data & 0xFF;
            priv->readPos = 1;
            break;

        case 1:
            *p = (data >> 8) & 0xFF;
            priv->readPos = 0;
            break;
        }
        p++;
        len--;
        actual++;
    }
    return actual;
}