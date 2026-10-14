#include "core.h"
#include "display_model.h"
#include <stdio.h>
#include <string.h>

/** Returns v / d rounded to nearest, halves away from zero (d > 0). */
static I32 RoundDiv(I32 v, I32 d)
{
    return v >= 0 ? (v + d / 2) / d : -((-v + d / 2) / d);
}

/** See display_model.h. */
int TempToDisplay(I16 c100, bool f, int *pTenths)
{
    I32 v = f ? (I32)c100 * 9 / 5 + 3200 : c100; /* hundredths of the display unit */
    I32 t = RoundDiv(v, 10);
    if (pTenths)
        *pTenths = (int)((t < 0 ? -t : t) % 10);
    return (int)(t / 10);
}

/** See display_model.h. */
void FormatTemp(char *pOut, int n, I16 c100, bool f, bool withTenths)
{
    I32 v = f ? (I32)c100 * 9 / 5 + 3200 : c100;
    if (withTenths)
    {
        I32 t = RoundDiv(v, 10);
        I32 a = t < 0 ? -t : t;
        snprintf(pOut, n, "%s%ld.%ld", t < 0 ? "-" : "", (long)(a / 10), (long)(a % 10));
    }
    else
    {
        snprintf(pOut, n, "%ld", (long)RoundDiv(v, 100));
    }
}

/** Returns the mode name shown on the home screen ("HEAT", "COOL", "OFF"). */
static const char *ModeStr(CtrlMode m)
{
    switch (m)
    {
        case kCtrlModeHeat:
            return "HEAT";
        case kCtrlModeCool:
            return "COOL";
        default:
            return "OFF";
    }
}

/** Returns the bottom-bar text for a Zigbee link state. */
static const char *LinkStr(LinkState l)
{
    switch (l)
    {
        case kLinkJoined:
            return "Zigbee: connected";
        case kLinkPairing:
            return "Zigbee: pairing...";
        case kLinkLost:
            return "Zigbee: lost parent";
        case kLinkFault:
            return "Zigbee: radio fault";
        default:
            return "Zigbee: not paired";
    }
}

/* ---------------- menu views ---------------- */

#define TITLE_H  22
#define ROW_Y0   24
#define ROW_H    20
#define ROWS_VIS 4
#define HINT_Y   108

/** Draw an inverted title bar with pT on the left and optional pRight right-aligned. */
static void TitleBar(GfxFb *pFb, const char *pT, const char *pRight)
{
    GfxFillRect(pFb, 0, 0, GFX_W, TITLE_H, true);
    GfxText(pFb, &kFontSmall, 6, 2, pT, false);
    if (pRight)
        GfxText(pFb, &kFontSmall, GFX_W - 6 - GfxTextWidth(&kFontSmall, pRight), 2, pRight, false);
}

/** Draw a separator line and the hint text at the bottom of a menu view. */
static void HintLine(GfxFb *pFb, const char *pH)
{
    GfxHline(pFb, 0, HINT_Y - 4, GFX_W);
    GfxText(pFb, &kFontSmall, 6, HINT_Y, pH, true);
}

/**
 * Draw up to ROWS_VIS label/value rows. With select, scroll so pV->sel is visible, invert the
 * selected row and draw scroll marks when rows are hidden above/below.
 */
static void RenderRows(GfxFb *pFb, const MenuView *pV, bool select)
{
    int first = 0;
    if (select && pV->sel >= ROWS_VIS)
        first = pV->sel - ROWS_VIS + 1;
    for (int i = first; i < pV->nRows && i < first + ROWS_VIS; i++)
    {
        int y = ROW_Y0 + (i - first) * ROW_H;
        bool inv = select && i == pV->sel;
        if (inv)
            GfxFillRect(pFb, 0, y - 1, GFX_W, ROW_H, true);
        GfxText(pFb, &kFontSmall, 10, y, pV->label[i], !inv);
        if (pV->value[i][0])
            GfxText(pFb, &kFontSmall, GFX_W - 10 - GfxTextWidth(&kFontSmall, pV->value[i]), y, pV->value[i], !inv);
    }
    /* scroll marks on the right edge when rows are hidden */
    if (select && first > 0)
        GfxFillRect(pFb, GFX_W - 4, ROW_Y0, 3, 6, true);
    if (select && first + ROWS_VIS < pV->nRows)
        GfxFillRect(pFb, GFX_W - 4, ROW_Y0 + ROWS_VIS * ROW_H - 8, 3, 6, true);
}

/** Clear the frame and draw a non-home menu view (list, text, value editor or full-screen warning). */
static void RenderView(GfxFb *pFb, const MenuView *pV)
{
    char pos[8];
    GfxClear(pFb);
    switch (pV->kind)
    {
        case kViewList:
            snprintf(pos, sizeof pos, "%u/%u", (unsigned)(pV->sel + 1), (unsigned)pV->nRows);
            TitleBar(pFb, pV->title, pos);
            RenderRows(pFb, pV, true);
            break;
        case kViewText:
            TitleBar(pFb, pV->title, NULL);
            RenderRows(pFb, pV, false);
            break;
        case kViewValue:
        {
            TitleBar(pFb, pV->title, NULL);
            const Font *pF = pV->bigIsText ? &kFontMed : &kFontBig;
            int w = GfxTextWidth(pF, pV->big), uw = pV->bigUnit[0] ? GfxTextWidth(&kFontMed, pV->bigUnit) + 4 : 0;
            int x = (GFX_W - w - uw) / 2, y = pV->bigIsText ? 48 : 34;
            GfxText(pFb, pF, x, y, pV->big, true);
            if (uw)
                GfxText(pFb, &kFontMed, x + w + 4, y + 6, pV->bigUnit, true);
            /* up/down arrows either side */
            for (int i = 0; i < 7; i++)
            {
                GfxHline(pFb, 20 - i, 50 + i, 1 + 2 * i);
                GfxHline(pFb, GFX_W - 20 - i, 70 - i, 1 + 2 * i);
            }
            break;
        }
        case kViewWarning:
        {
            GfxFillRect(pFb, 0, 0, GFX_W, GFX_H, true);
            int w = GfxTextWidth(&kFontMed, pV->title);
            GfxText(pFb, &kFontMed, (GFX_W - w) / 2, 6, pV->title, false);
            for (int i = 0; i < pV->nRows; i++)
            {
                w = GfxTextWidth(&kFontSmall, pV->label[i]);
                GfxText(pFb, &kFontSmall, (GFX_W - w) / 2, 46 + i * 24, pV->label[i], false);
            }
            return;
        }
        default:
            break;
    }
    if (pV->hint[0])
        HintLine(pFb, pV->hint);
}

/** Menu view or home screen. See display_model.h. */
void DisplayRender(GfxFb *pFb, const DisplayModel *pM)
{
    if (pM->view.kind != kViewHome && pM->view.kind != kViewHomeBanner)
    {
        RenderView(pFb, &pM->view);
        return;
    }
    char b[32];
    GfxClear(pFb);
    const bool f = pM->fahrenheit;

    /* ---- left: big room temperature ---- */
    if (pM->tempValid)
    {
        FormatTemp(b, sizeof b, pM->temp, f, true);
        int x = GfxText(pFb, &kFontBig, 4, 2, b, true);
        GfxText(pFb, &kFontMed, x + 2, 8, f ? FONT_DEGREE "F" : FONT_DEGREE "C", true);
    }
    else
    {
        GfxText(pFb, &kFontBig, 4, 2, "--.-", true);
    }
    if (pM->rhValid)
    {
        snprintf(b, sizeof b, "%u%% RH", (unsigned)((pM->rh + 50) / 100));
        GfxText(pFb, &kFontSmall, 8, 66, b, true);
    }
    if (pM->swing > 0 && pM->s.mode != kCtrlModeOff)
    {
        /* swing is a temperature *difference*: °F = °C * 1.8, no offset */
        int d = f ? (pM->swing * 18 + 50) / 100 : (pM->swing + 5) / 10; /* tenths */
        snprintf(b, sizeof b, "%s%d.%d" FONT_DEGREE, "+/-", d / 10, d % 10);
        GfxText(pFb, &kFontSmall, 96, 66, b, true);
    }

    /* program period (left column, under RH) */
    if (pM->s.mode != kCtrlModeOff)
    {
        const char *pPs = NULL;
        switch (pM->schedState)
        {
            case 0:
                pPs = "Day (no clock)";
                break;
            case 1:
                pPs = "Day";
                break;
            case 2:
                pPs = "Night";
                break;
            case 3:
                pPs = "Day, morning boost";
                break;
            default:
                break;
        }
        if (pPs)
            GfxText(pFb, &kFontSmall, 8, 84, pPs, true);
    }

    /* ---- right column: mode, setpoint(s), running state ---- */
    const int rx = 176;
    GfxVline(pFb, rx - 8, 4, 92);
    GfxText(pFb, &kFontMed, rx, 2, ModeStr(pM->s.mode), true);
    if (pM->s.fan == kCtrlFanOn)
        GfxText(pFb, &kFontSmall, rx, 84, "Fan: on", true);

    char h[8], c[8];
    FormatTemp(h, sizeof h, pM->s.heatSp, f, !f);
    FormatTemp(c, sizeof c, pM->s.coolSp, f, !f);
    switch (pM->s.mode)
    {
        /* "Set 21.5°" does not fit the column in medium font: drop "Set" in °C */
        case kCtrlModeHeat:
            snprintf(b, sizeof b, "%s%s" FONT_DEGREE, f ? "Set " : "", h);
            break;
        case kCtrlModeCool:
            snprintf(b, sizeof b, "%s%s" FONT_DEGREE, f ? "Set " : "", c);
            break;
        default:
            b[0] = 0;
            break;
    }
    GfxText(pFb, &kFontMed, rx, 32, b, true);

    const char *pSt = "Idle";
    if (pM->fault != kCtrlFaultNone)
        pSt = CtrlFaultStr(pM->fault);
    else if (pM->w)
        pSt = "Heating";
    else if (pM->y)
        pSt = "Cooling";
    else if (pM->waiting)
        pSt = "Waiting";
    else if (pM->g)
        pSt = "Fan";
    /* running state shown inverted when something is on */
    bool active = pM->w || pM->y || pM->g || pM->fault != kCtrlFaultNone;
    int tw = GfxTextWidth(&kFontSmall, pSt);
    if (active)
        GfxFillRect(pFb, rx - 2, 62, tw + 6, kFontSmall.height + 2, true);
    GfxText(pFb, &kFontSmall, rx + 1, 63, pSt, !active);

    /* ---- bottom status bar ---- */
    GfxHline(pFb, 0, 104, GFX_W);
    if (pM->view.kind == kViewHomeBanner)
    {
        GfxFillRect(pFb, 0, 105, GFX_W, GFX_H - 105, true);
        GfxText(pFb, &kFontSmall, 4, 108, pM->view.hint, false);
    }
    else if (pM->pBanner)
    {
        GfxText(pFb, &kFontSmall, 4, 108, pM->pBanner, true);
    }
    else if (pM->boost != kBoostNone)
    {
        const char *pD = pM->boostDir == kCtrlModeCool ? "cool" : "heat";
        if (pM->boost == kBoostToTarget)
        {
            FormatTemp(h, sizeof h, pM->boostSp, f, !f);
            snprintf(b, sizeof b, "BOOST %s to %s" FONT_DEGREE "%s", pD, h, f ? "F" : "C");
        }
        else
        {
            snprintf(b, sizeof b, "BOOST %s %u min left", pD, (unsigned)pM->boostMinLeft);
        }
        /* inverted bar so a running boost is obvious from across the room */
        GfxFillRect(pFb, 0, 105, GFX_W, GFX_H - 105, true);
        GfxText(pFb, &kFontSmall, 4, 108, b, false);
    }
    else
    {
        GfxText(pFb, &kFontSmall, 4, 108, LinkStr(pM->link), true);
    }
}
