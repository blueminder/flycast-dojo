/*
	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

enum DreamcastKey
{
	// Real buttons
	DC_BTN_C           = 1 << 0,
	DC_BTN_B           = 1 << 1,
	DC_BTN_A           = 1 << 2,
	DC_BTN_START       = 1 << 3,
	DC_DPAD_UP         = 1 << 4,
	DC_DPAD_DOWN       = 1 << 5,
	DC_DPAD_LEFT       = 1 << 6,
	DC_DPAD_RIGHT      = 1 << 7,
	DC_BTN_Z           = 1 << 8,
	DC_BTN_Y           = 1 << 9,
	DC_BTN_X           = 1 << 10,
	DC_BTN_D           = 1 << 11,
	DC_DPAD2_UP        = 1 << 12,
	DC_DPAD2_DOWN      = 1 << 13,
	DC_DPAD2_LEFT      = 1 << 14,
	DC_DPAD2_RIGHT     = 1 << 15,
	DC_BTN_RELOAD      = 1 << 16,	// Not a real button but handled like one

	// System buttons
	EMU_BTN_NONE			= 0,
	EMU_BTN_TRIGGER_LEFT	= 1 << 17,
	EMU_BTN_TRIGGER_RIGHT	= 1 << 18,
	EMU_BTN_MENU			= 1 << 19,
	EMU_BTN_FFORWARD		= 1 << 20,
	EMU_BTN_ANA_UP			= 1 << 21,
	EMU_BTN_ANA_DOWN		= 1 << 22,
	EMU_BTN_ANA_LEFT		= 1 << 23,
	EMU_BTN_ANA_RIGHT		= 1 << 24,
	EMU_BTN_ESCAPE			= 1 << 25,
	EMU_BTN_JUMP_STATE		= 1 << 26,
	EMU_BTN_QUICK_SAVE		= 1 << 27,
	EMU_BTN_RECORD  		= 1 << 28,
	EMU_BTN_PLAY    		= 1 << 29,

	// Button combinations
	DC_CMB_X_Y_A_B          = (DC_BTN_X | DC_BTN_Y | DC_BTN_A | DC_BTN_B) + 1, // DC Face Buttons
	DC_CMB_X_Y_LT           = (DC_BTN_X | DC_BTN_Y | EMU_BTN_TRIGGER_LEFT) + 1, // DC 3P
	DC_CMB_A_B_RT           = (DC_BTN_A | DC_BTN_B | EMU_BTN_TRIGGER_RIGHT) + 1, // DC 3K
	DC_CMB_X_A              = (DC_BTN_X | DC_BTN_A) + 1, // DC LP + LK
	DC_CMB_Y_B              = (DC_BTN_Y | DC_BTN_B) + 1, // DC MP + MK
	DC_CMB_LT_RT            = (EMU_BTN_TRIGGER_LEFT | EMU_BTN_TRIGGER_RIGHT) + 1, // DC HP + HK

	NAOMI_CMB_1_2_3         = (DC_BTN_A | DC_BTN_B | DC_BTN_X) + 1, // NAOMI 3P
	NAOMI_CMB_4_5_6         = (DC_BTN_Y | DC_DPAD2_UP | DC_DPAD2_DOWN) + 1, // NAOMI 3K
	NAOMI_CMB_1_4           = (DC_BTN_A | DC_BTN_Y) + 1, // NAOMI LP + LK
	NAOMI_CMB_2_5           = (DC_BTN_B | DC_DPAD2_UP) + 1, // NAOMI MP + MK
	NAOMI_CMB_3_6           = (DC_BTN_X | DC_DPAD2_DOWN) + 1, // NAOMI HP + HK
	NAOMI_CMB_1_2           = (DC_BTN_A | DC_BTN_B) + 1, // NAOMI 1+2
	NAOMI_CMB_2_3           = (DC_BTN_B | DC_BTN_X) + 1, // NAOMI 2+3
	NAOMI_CMB_3_4           = (DC_BTN_X | DC_BTN_Y) + 1, // NAOMI 3+4
	NAOMI_CMB_4_5           = (DC_BTN_Y | DC_DPAD2_UP) + 1, // NAOMI 4+5

	// Real axes
	DC_AXIS_LT		 = 0x10000,
	DC_AXIS_RT		 = 0x10001,
	DC_AXIS_X        = 0x20000,
	DC_AXIS_Y        = 0x20001,
	DC_AXIS_X2		 = 0x20002,
	DC_AXIS_Y2		 = 0x20003,

	// System axes
	EMU_AXIS_NONE        = 0x00000,
	EMU_AXIS_DPAD1_X     = DC_DPAD_LEFT,
	EMU_AXIS_DPAD1_Y     = DC_DPAD_UP,
	EMU_AXIS_DPAD2_X     = DC_DPAD2_LEFT,
	EMU_AXIS_DPAD2_Y     = DC_DPAD2_UP,
	EMU_AXIS_BTN_A       = 0x40000 | DC_BTN_A,
	EMU_AXIS_BTN_B       = 0x40000 | DC_BTN_B,
	EMU_AXIS_BTN_C       = 0x40000 | DC_BTN_C,
	EMU_AXIS_BTN_D       = 0x40000 | DC_BTN_D,
	EMU_AXIS_BTN_X       = 0x40000 | DC_BTN_X,
	EMU_AXIS_BTN_Y       = 0x40000 | DC_BTN_Y,
	EMU_AXIS_BTN_Z       = 0x40000 | DC_BTN_Z,
	EMU_AXIS_BTN_START   = 0x40000 | DC_BTN_START,
	EMU_AXIS_DPAD_UP     = 0x40000 | DC_DPAD_UP,
	EMU_AXIS_DPAD_DOWN   = 0x40000 | DC_DPAD_DOWN,
	EMU_AXIS_DPAD_LEFT   = 0x40000 | DC_DPAD_LEFT,
	EMU_AXIS_DPAD_RIGHT  = 0x40000 | DC_DPAD_RIGHT,
	EMU_AXIS_DPAD2_UP    = 0x40000 | DC_DPAD2_UP,
	EMU_AXIS_DPAD2_DOWN  = 0x40000 | DC_DPAD2_DOWN,
	EMU_AXIS_DPAD2_LEFT  = 0x40000 | DC_DPAD2_LEFT,
	EMU_AXIS_DPAD2_RIGHT = 0x40000 | DC_DPAD2_RIGHT,

	// Button combinations (as axes)
	EMU_AXIS_CMB_X_Y_A_B  = DC_CMB_X_Y_A_B,
	EMU_AXIS_CMB_X_Y_LT   = DC_CMB_X_Y_LT,
	EMU_AXIS_CMB_A_B_RT   = DC_CMB_A_B_RT,
	EMU_AXIS_CMB_X_A      = DC_CMB_X_A,
	EMU_AXIS_CMB_Y_B      = DC_CMB_Y_B,
	EMU_AXIS_CMB_LT_RT    = DC_CMB_LT_RT,

	EMU_AXIS_CMB_1_2_3    = NAOMI_CMB_1_2_3,
	EMU_AXIS_CMB_4_5_6    = NAOMI_CMB_4_5_6,
	EMU_AXIS_CMB_1_4      = NAOMI_CMB_1_4,
	EMU_AXIS_CMB_2_5      = NAOMI_CMB_2_5,
	EMU_AXIS_CMB_3_6      = NAOMI_CMB_3_6,
	EMU_AXIS_CMB_1_2      = NAOMI_CMB_1_2,
	EMU_AXIS_CMB_2_3      = NAOMI_CMB_2_3,
	EMU_AXIS_CMB_3_4      = NAOMI_CMB_3_4,
	EMU_AXIS_CMB_4_5      = NAOMI_CMB_4_5,

};
