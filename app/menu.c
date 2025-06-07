/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>

#if !defined(ENABLE_OVERLAY)
#include "ARMCM0.h"
#endif
#include "app/dtmf.h"
#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
#include "sram-overlay.h"
#endif
#include "ui/inputbox.h"
#include "ui/menu.h"
#include "ui/ui.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

uint8_t gUnlockAllTxConfCnt;

// Define F_LOCK_NONE if not already defined
#ifndef F_LOCK_NONE
#define F_LOCK_NONE 0
#endif

#ifdef ENABLE_F_CAL_MENU
void writeXtalFreqCal(const int32_t value, const bool update_eeprom)
{
    BK4819_WriteRegister(BK4819_REG_3B, 22656 + value);

    if (update_eeprom)
    {
        struct
        {
            int16_t BK4819_XtalFreqLow;
            uint16_t EEPROM_1F8A;
            uint16_t EEPROM_1F8C;
            uint8_t VOLUME_GAIN;
            uint8_t DAC_GAIN;
        } __attribute__((packed)) misc;

        gEeprom.BK4819_XTAL_FREQ_LOW = value;

        // radio 1 .. 04 00 46 00 50 00 2C 0E
        // radio 2 .. 05 00 46 00 50 00 2C 0E
        //
        EEPROM_ReadBuffer(0x1F88, &misc, 8);
        misc.BK4819_XtalFreqLow = value;
        EEPROM_WriteBuffer(0x1F88, &misc);
    }
}
#endif

void MENU_StartCssScan(void)
{
    SCANNER_Start(true);
    gUpdateStatus = true;
    gCssBackgroundScan = true;

    gRequestDisplayScreen = DISPLAY_MENU;
}

void MENU_CssScanFound(void)
{
    if (gScanCssResultType == CODE_TYPE_DIGITAL || gScanCssResultType == CODE_TYPE_REVERSE_DIGITAL)
    {
        gMenuCursor = UI_MENU_GetMenuIdx(MENU_R_DCS);
    }
    else if (gScanCssResultType == CODE_TYPE_CONTINUOUS_TONE)
    {
        gMenuCursor = UI_MENU_GetMenuIdx(MENU_R_CTCS);
    }

    MENU_ShowCurrentSetting();

    gUpdateStatus = true;
    gUpdateDisplay = true;
}

void MENU_StopCssScan(void)
{
    gCssBackgroundScan = false;

#ifdef ENABLE_VOICE
    gAnotherVoiceID = VOICE_ID_SCANNING_STOP;
#endif
    gUpdateDisplay = true;
    gUpdateStatus = true;
}

int MENU_GetLimits(uint8_t menu_id, int32_t *pMin, int32_t *pMax)
{
    *pMin = 0;

    switch (menu_id)
    {
    case MENU_SQL:
        //*pMin = 0;
        *pMax = 9;
        break;

    case MENU_STEP:
        //*pMin = 0;
        *pMax = STEP_N_ELEM - 1;
        break;

    case MENU_ABR:
        //*pMin = 0;
        *pMax = 61;
        break;

    case MENU_ABR_MIN:
        //*pMin = 0;
        *pMax = 9;
        break;

    case MENU_ABR_MAX:
        *pMin = 1;
        *pMax = 10;
        break;

    case MENU_F_LOCK:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_F_LOCK) - 1;
        break;

    case MENU_MDF:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_MDF) - 1;
        break;

    case MENU_TXP:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_TXP) - 1;
        break;

    case MENU_SFT_D:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_SFT_D) - 1;
        break;

    case MENU_TDR:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_RXMode) - 1;
        break;

#ifdef ENABLE_VOICE
    case MENU_VOICE:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_VOICE) - 1;
        break;
#endif

    case MENU_SC_REV:
        //*pMin = 0;
        *pMax = 104;
        break;

    case MENU_ROGER:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_ROGER) - 1;
        break;

    case MENU_PONMSG:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_PONMSG) - 1;
        break;

    case MENU_R_DCS:

    case MENU_R_CTCS:

    case MENU_W_N:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_W_N) - 1;
        break;

#ifdef ENABLE_ALARM
    case MENU_AL_MOD:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_AL_MOD) - 1;
        break;
#endif

    case MENU_RESET:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_RESET) - 1;
        break;

    case MENU_COMPAND:
    case MENU_ABR_ON_TX_RX:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_RX_TX) - 1;
        break;

#ifndef ENABLE_FEAT_F4HWN
#ifdef ENABLE_AM_FIX
    case MENU_AM_FIX:
#endif
#endif
#ifdef ENABLE_AUDIO_BAR
    case MENU_MIC_BAR:
#endif
    case MENU_BCL:
    case MENU_BEEP:
    case MENU_S_ADD1:
    case MENU_S_ADD2:
    case MENU_S_ADD3:
    case MENU_STE:
    case MENU_D_ST:

#ifndef ENABLE_FEAT_F4HWN
    case MENU_SCREN:
#endif
#ifdef ENABLE_FEAT_F4HWN
    case MENU_SET_TMR:
#endif
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_OFF_ON) - 1;
        break;
    case MENU_AM:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gModulationStr) - 1;
        break;

#ifndef ENABLE_FEAT_F4HWN
    case MENU_SCR:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_SCRAMBLER) - 1;
        break;
#endif

    case MENU_AUTOLK:
        *pMax = 40;
        break;

    case MENU_TOT:
        //*pMin = 0;
        *pMin = 5;
        *pMax = 179;
        break;

#ifdef ENABLE_VOX
    case MENU_VOX:
#endif
    case MENU_RP_STE:
        //*pMin = 0;
        *pMax = 10;
        break;

    case MENU_MEM_CH:
    case MENU_1_CALL:
    case MENU_DEL_CH:
    case MENU_MEM_NAME:
        //*pMin = 0;
        *pMax = MR_CHANNEL_LAST;
        break;

    case MENU_SLIST1:
    case MENU_SLIST2:
    case MENU_SLIST3:
        *pMin = -1;
        *pMax = MR_CHANNEL_LAST;
        break;

    case MENU_SAVE:
        //*pMin = 0;
        *pMax = 5;
        break;

    case MENU_MIC:
        //*pMin = 0;
        *pMax = 4;
        break;

    case MENU_S_LIST:
        //*pMin = 0;
        *pMax = 5;
        break;

    case MENU_BAT_TXT:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_BAT_TXT) - 1;
        break;

    case MENU_BATCAL:
        *pMin = 1600;
        *pMax = 2200;
        break;

    case MENU_BATTYP:
        //*pMin = 0;
        *pMax = 2;
        break;

    case MENU_F1SHRT:
    case MENU_F1LONG:
    case MENU_F2SHRT:
    case MENU_F2LONG:
    case MENU_MLONG:
        //*pMin = 0;
        *pMax = gSubMenu_SIDEFUNCTIONS_size - 1;
        break;

#ifdef ENABLE_FEAT_F4HWN_SLEEP
    case MENU_SET_OFF:
        *pMax = 120;
        break;
#endif

#ifdef ENABLE_FEAT_F4HWN
    case MENU_SET_TOT:
    case MENU_SET_EOT:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_SET_TOT) - 1;
        break;
#ifdef ENABLE_FEAT_F4HWN_CTR
    case MENU_SET_CTR:
        *pMin = 1;
        *pMax = 15;
        break;
#endif

#ifdef ENABLE_FEAT_F4HWN_INV
    case MENU_SET_INV:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_OFF_ON) - 1;
        break;
#endif
    case MENU_SET_LCK:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_SET_LCK) - 1;
        break;
    case MENU_SET_MET:
    case MENU_SET_GUI:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_SET_MET) - 1;
        break;
#ifdef ENABLE_FEAT_F4HWN_NARROWER
    case MENU_SET_NFM:
        //*pMin = 0;
        *pMax = ARRAY_SIZE(gSubMenu_SET_NFM) - 1;
        break;
#endif
#ifdef ENABLE_FEAT_F4HWN_VOL
    case MENU_SET_VOL:
        //*pMin = 0;
        *pMax = 63;
        break;
#endif
#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
    case MENU_SET_KEY:
        //*pMin = 0;
        *pMax = 4;
        break;
#endif
#endif

    default:
        return -1;
    }

    return 0;
}

void MENU_AcceptSetting(void)
{
    int32_t Min;
    int32_t Max;
    FREQ_Config_t *pConfig = &gTxVfo->freq_config_RX;

    if (!MENU_GetLimits(UI_MENU_GetCurrentMenuId(), &Min, &Max))
    {
        if (gSubMenuSelection < Min)
            gSubMenuSelection = Min;
        else if (gSubMenuSelection > Max)
            gSubMenuSelection = Max;
    }

    switch (UI_MENU_GetCurrentMenuId())
    {
    default:
        return;

    case MENU_SQL:
        gEeprom.SQUELCH_LEVEL = gSubMenuSelection;
        gVfoConfigureMode = VFO_CONFIGURE;
        break;

    case MENU_STEP:
        gTxVfo->STEP_SETTING = FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection);
        if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
        {
            gRequestSaveChannel = 1;
        }
        return;

    case MENU_R_DCS:
    {
        if (gSubMenuSelection == 0)
        {
            if (pConfig->CodeType == CODE_TYPE_CONTINUOUS_TONE)
            {
                return;
            }
            pConfig->Code = 0;
            pConfig->CodeType = CODE_TYPE_OFF;
        }
        else if (gSubMenuSelection < 105)
        {
            pConfig->CodeType = CODE_TYPE_DIGITAL;
            pConfig->Code = gSubMenuSelection - 1;
        }
        else
        {
            pConfig->CodeType = CODE_TYPE_REVERSE_DIGITAL;
            pConfig->Code = gSubMenuSelection - 105;
        }
        gRequestSaveChannel = 1;
        return;
    }
    case MENU_R_CTCS:
    {
        if (gSubMenuSelection == 0)
        {
            if (pConfig->CodeType != CODE_TYPE_CONTINUOUS_TONE)
            {
                return;
            }
            pConfig->Code = 0;
            pConfig->CodeType = CODE_TYPE_OFF;
        }
        else
        {
            pConfig->Code = gSubMenuSelection - 1;
            pConfig->CodeType = CODE_TYPE_CONTINUOUS_TONE;
        }
        gRequestSaveChannel = 1;
        return;
    }
    case MENU_SFT_D:
        gTxVfo->TX_OFFSET_FREQUENCY_DIRECTION = gSubMenuSelection;
        gRequestSaveChannel = 1;
        return;

    case MENU_OFFSET:
        gTxVfo->TX_OFFSET_FREQUENCY = gSubMenuSelection;
        gRequestSaveChannel = 1;
        return;

    case MENU_W_N:
        gTxVfo->CHANNEL_BANDWIDTH = gSubMenuSelection;
        gRequestSaveChannel = 1;
        return;

#ifndef ENABLE_FEAT_F4HWN
    case MENU_SCR:
        gTxVfo->SCRAMBLING_TYPE = gSubMenuSelection;
#if 0
                if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
                    BK4819_EnableScramble(gSubMenuSelection - 1);
                else
                    BK4819_DisableScramble();
#endif
        gRequestSaveChannel = 1;
        return;
#endif

    case MENU_BCL:
        gTxVfo->BUSY_CHANNEL_LOCK = gSubMenuSelection;
        gRequestSaveChannel = 1;
        return;

    case MENU_MEM_CH:
        gTxVfo->CHANNEL_SAVE = gSubMenuSelection;
#if 0
                gEeprom.MrChannel[0] = gSubMenuSelection;
#endif
        gRequestSaveChannel = 2;
        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
        gFlagResetVfos = true;
        return;

    case MENU_MEM_NAME:
        for (int i = 9; i >= 0; i--)
        {
            if (edit[i] != ' ' && edit[i] != '_' && edit[i] != 0x00 && edit[i] != 0xff)
                break;
            edit[i] = ' ';
        }

        SETTINGS_SaveChannelName(gSubMenuSelection, edit);
        return;

    case MENU_SAVE:
        gEeprom.BATTERY_SAVE = gSubMenuSelection;
        break;

    case MENU_ABR:
        gEeprom.BACKLIGHT_TIME = gSubMenuSelection;
#ifdef ENABLE_FEAT_F4HWN
        gBackLight = false;
#endif
        break;

    case MENU_ABR_MIN:
        gEeprom.BACKLIGHT_MIN = gSubMenuSelection;
        gEeprom.BACKLIGHT_MAX = MAX(gSubMenuSelection + 1, gEeprom.BACKLIGHT_MAX);
        break;

    case MENU_ABR_MAX:
        gEeprom.BACKLIGHT_MAX = gSubMenuSelection;
        gEeprom.BACKLIGHT_MIN = MIN(gSubMenuSelection - 1, gEeprom.BACKLIGHT_MIN);
        break;


#ifdef ENABLE_FEAT_F4HWN
        gDW = gEeprom.DUAL_WATCH;
        gSaveRxMode = true;
#endif

        gFlagReconfigureVfos = true;
        gUpdateStatus = true;
        break;

    case MENU_BEEP:
        gEeprom.BEEP_CONTROL = gSubMenuSelection;
        break;

#ifdef ENABLE_VOICE
    case MENU_VOICE:
        gEeprom.VOICE_PROMPT = gSubMenuSelection;
        gUpdateStatus = true;
        break;
#endif

    case MENU_SC_REV:
        gEeprom.SCAN_RESUME_MODE = gSubMenuSelection;
        break;

    case MENU_MDF:
        gEeprom.CHANNEL_DISPLAY_MODE = gSubMenuSelection;
        break;

    case MENU_AUTOLK:
        gEeprom.AUTO_KEYPAD_LOCK = gSubMenuSelection;
        gKeyLockCountdown = gEeprom.AUTO_KEYPAD_LOCK * 30; // 15 seconds step
        break;

    case MENU_S_ADD1:
        gTxVfo->SCANLIST1_PARTICIPATION = gSubMenuSelection;
        SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true, false, true);
        gVfoConfigureMode = VFO_CONFIGURE;
        gFlagResetVfos = true;
        return;

    case MENU_S_ADD2:
        gTxVfo->SCANLIST2_PARTICIPATION = gSubMenuSelection;
        SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true, false, true);
        gVfoConfigureMode = VFO_CONFIGURE;
        gFlagResetVfos = true;
        return;

    case MENU_S_ADD3:
        gTxVfo->SCANLIST3_PARTICIPATION = gSubMenuSelection;
        SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true, false, true);
        gVfoConfigureMode = VFO_CONFIGURE;
        gFlagResetVfos = true;
        return;

    case MENU_STE:
        gEeprom.TAIL_TONE_ELIMINATION = gSubMenuSelection;
        break;

    case MENU_RP_STE:
        gEeprom.REPEATER_TAIL_TONE_ELIMINATION = gSubMenuSelection;
        break;

#ifdef ENABLE_AUDIO_BAR
    case MENU_MIC_BAR:
        gSetting_mic_bar = gSubMenuSelection;
        break;
#endif

    case MENU_COMPAND:
        gTxVfo->Compander = gSubMenuSelection;
        SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true, false, true);
        gVfoConfigureMode = VFO_CONFIGURE;
        gFlagResetVfos = true;
        //          gRequestSaveChannel = 1;
        return;

    case MENU_1_CALL:
        gEeprom.CHAN_1_CALL = gSubMenuSelection;
        break;

    case MENU_S_LIST:
        gEeprom.SCAN_LIST_DEFAULT = gSubMenuSelection;
        break;

#ifdef ENABLE_ALARM
    case MENU_AL_MOD:
        gEeprom.ALARM_MODE = gSubMenuSelection;
        break;
#endif

    case MENU_BAT_TXT:
        gSetting_battery_text = gSubMenuSelection;
        break;

    case MENU_PONMSG:
        gEeprom.POWER_ON_DISPLAY_MODE = gSubMenuSelection;
        break;

    case MENU_AM:
        gTxVfo->Modulation = gSubMenuSelection;
        gRequestSaveChannel = 1;
        return;

#ifndef ENABLE_FEAT_F4HWN
#ifdef ENABLE_AM_FIX
    case MENU_AM_FIX:
        gSetting_AM_fix = gSubMenuSelection;
        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
        gFlagResetVfos = true;
        break;
#endif
#endif

    case MENU_DEL_CH:
        SETTINGS_UpdateChannel(gSubMenuSelection, NULL, false, false, true);
        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
        gFlagResetVfos = true;
        return;

    case MENU_RESET:
        SETTINGS_FactoryReset(gSubMenuSelection);
        return;

    case MENU_F_LOCK:
    {
        if (gSubMenuSelection == F_LOCK_NONE)
        { // select 10 times to enable
            gUnlockAllTxConfCnt++;
#ifdef ENABLE_FEAT_F4HWN
            if (gUnlockAllTxConfCnt < 3)
#else
            if (gUnlockAllTxConfCnt < 10)
#endif
                return;
        }
        else
            gUnlockAllTxConfCnt = 0;

        gSetting_F_LOCK = gSubMenuSelection;

        break;
    }
#ifndef ENABLE_FEAT_F4HWN
    case MENU_SCREN:
        gSetting_ScrambleEnable = gSubMenuSelection;
        gFlagReconfigureVfos = true;
        break;
#endif
#ifdef ENABLE_F_CAL_MENU
    case MENU_F_CALI:
        writeXtalFreqCal(gSubMenuSelection, true);
        return;
#endif
    case MENU_BATCAL:
        gBatteryCalibration[3] = gSubMenuSelection;
        SETTINGS_SaveBatteryCalibration(gBatteryCalibration);
        return;
    case MENU_BATTYP:
        gEeprom.BATTERY_TYPE = gSubMenuSelection;
        break;
    case MENU_F1SHRT:
    case MENU_F1LONG:
    case MENU_F2SHRT:
    case MENU_F2LONG:
    case MENU_MLONG:
    {
        uint8_t *fun[] = {
            &gEeprom.KEY_1_SHORT_PRESS_ACTION,
            &gEeprom.KEY_1_LONG_PRESS_ACTION,
            &gEeprom.KEY_2_SHORT_PRESS_ACTION,
            &gEeprom.KEY_2_LONG_PRESS_ACTION,
            &gEeprom.KEY_M_LONG_PRESS_ACTION};
        *fun[UI_MENU_GetCurrentMenuId() - MENU_F1SHRT] = gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].id;
    }
    break;

#ifdef ENABLE_FEAT_F4HWN_SLEEP
    case MENU_SET_OFF:
        gSetting_set_off = gSubMenuSelection;
        break;
#endif

#ifdef ENABLE_FEAT_F4HWN
    case MENU_SET_TOT:
        gSetting_set_tot = gSubMenuSelection;
        break;
    case MENU_SET_EOT:
        gSetting_set_eot = gSubMenuSelection;
        break;
#endif
#ifdef ENABLE_FEAT_F4HWN_CTR
    case MENU_SET_CTR:
        gSetting_set_ctr = gSubMenuSelection;
        break;
    case MENU_SET_INV:
        gSetting_set_inv = gSubMenuSelection;
        break;
    case MENU_SET_LCK:
        gSetting_set_lck = gSubMenuSelection;
        break;
    case MENU_SET_MET:
        gSetting_set_met = gSubMenuSelection;
        break;
    case MENU_SET_GUI:
        gSetting_set_gui = gSubMenuSelection;
        break;
#endif
#ifdef ENABLE_FEAT_F4HWN_NARROWER
    case MENU_SET_NFM:
        gSetting_set_nfm = gSubMenuSelection;
        RADIO_SetTxParameters();
        RADIO_SetupRegisters(true);
        break;
#endif
#ifdef ENABLE_FEAT_F4HWN_VOL
    case MENU_SET_VOL:
        gSubMenuSelection = gEeprom.VOLUME_GAIN;
        break;
#endif
        gRequestSaveSettings = true;
    }