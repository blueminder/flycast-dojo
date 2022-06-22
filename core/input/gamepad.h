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
	
	DC_BTN_GROUP_MASK   = 0xF000000,

	EMU_BUTTONS         = 0x3000000,
	EMU_BTN_MENU,
	EMU_BTN_FFORWARD,
	EMU_BTN_ESCAPE,
	EMU_BTN_INSERT_CARD,
	EMU_BTN_JUMP_STATE,
	EMU_BTN_QUICK_SAVE,
	EMU_BTN_RECORD,
	EMU_BTN_PLAY,
	EMU_BTN_RECORD_1,
	EMU_BTN_PLAY_1,
	EMU_BTN_RECORD_2,
	EMU_BTN_PLAY_2,
	EMU_BTN_PLAY_RND,
	EMU_BTN_SWITCH_PLAYER,
	EMU_BTN_STEP,
	EMU_BTN_PAUSE,
	EMU_CMB_X_Y_A_B,
	EMU_CMB_X_Y_A,
	EMU_CMB_X_Y_LT,
	EMU_CMB_A_B_RT,
	EMU_CMB_X_A,
	EMU_CMB_Y_B,
	EMU_CMB_LT_RT,
	EMU_CMB_1_2_3,
	EMU_CMB_4_5,
	EMU_CMB_4_5_6,
	EMU_CMB_1_4,
	EMU_CMB_2_5,
	EMU_CMB_3_4,
	EMU_CMB_3_6,
	EMU_CMB_1_2,
	EMU_CMB_1_3,
	EMU_CMB_2_3,
	EMU_CMB_1_2_4,
	EMU_CMB_1_2_3_4,
	EMU_CMB_1_5,
	EMU_CMB_2_4,

	// Real axes
	DC_AXIS_TRIGGERS	= 0x1000000,
	DC_AXIS_LT,
	DC_AXIS_RT,
	DC_AXIS_STICKS		= 0x2000000,
	DC_AXIS_LEFT,
	DC_AXIS_RIGHT,
	DC_AXIS_UP,
	DC_AXIS_DOWN,
	DC_AXIS2_LEFT,
	DC_AXIS2_RIGHT,
	DC_AXIS2_UP,
	DC_AXIS2_DOWN,

	// System axes
	EMU_AXIS_NONE        = 0,
};
