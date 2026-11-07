/* SPDX-License-Identifier: GPL-3.0-or-later */
/* New regression: public SDL joystick -> Smart Pro -> semantic input. */
#include "../src/device.h"
#include <assert.h>
#include <stdio.h>

bool plat_resident_tell(const char *fmt, ...) { (void)fmt; return false; }

int main(void)
{
	in_state st = { 0 };
	assert(SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) == 0);
	int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 9, 1);
	assert(index >= 0);
	SDL_Joystick *pad = SDL_JoystickOpen(index);
	assert(pad && plat_input_init());
	for (int i = 0; i < 6; i++) assert(SDL_JoystickSetVirtualAxis(pad, i, 0) == 0);
	plat_input_settle(&st);
	/* The reported physical B selects with the old index1->ACCEPT mapping.
	 * Keep A and B asymmetric: simultaneous presses would hide a swap. */
	assert(SDL_JoystickSetVirtualButton(pad, 1, 1) == 0);
	plat_input_poll(&st);
	assert(st.pressed[IN_BACK] && st.down[IN_BACK]);
	assert(!st.down[IN_ACCEPT] && !st.pressed[IN_ACCEPT]);
	assert(SDL_JoystickSetVirtualButton(pad, 1, 0) == 0);
	assert(SDL_JoystickSetVirtualButton(pad, 0, 1) == 0);
	plat_input_poll(&st);
	assert(st.pressed[IN_ACCEPT] && st.down[IN_ACCEPT]);
	assert(!st.down[IN_BACK] && !st.pressed[IN_BACK]);
	assert(SDL_JoystickSetVirtualButton(pad, 0, 0) == 0);
	/* No evidence supports changing the prior X/Y contract. */
	assert(SDL_JoystickSetVirtualButton(pad, 2, 1) == 0);
	plat_input_poll(&st);
	assert(st.pressed[IN_Y] && !st.down[IN_X]);
	assert(SDL_JoystickSetVirtualButton(pad, 2, 0) == 0);
	assert(SDL_JoystickSetVirtualButton(pad, 3, 1) == 0);
	plat_input_poll(&st);
	assert(st.pressed[IN_X] && !st.down[IN_Y]);
	assert(SDL_JoystickSetVirtualButton(pad, 3, 0) == 0);
	assert(SDL_JoystickSetVirtualHat(pad, 0, SDL_HAT_LEFT) == 0);
	assert(SDL_JoystickSetVirtualAxis(pad, 0, -24000) == 0);
	plat_input_poll(&st);
	assert(st.pressed[IN_LEFT]);
	assert(SDL_JoystickSetVirtualAxis(pad, 0, 0) == 0);
	plat_input_poll(&st);
	assert(st.down[IN_LEFT] && !st.pressed[IN_LEFT]);
	assert(SDL_JoystickSetVirtualHat(pad, 0, SDL_HAT_CENTERED) == 0);
	plat_input_poll(&st);
	assert(!st.down[IN_LEFT]);
	plat_input_quit();
	SDL_JoystickClose(pad);
	SDL_JoystickDetachVirtual(index);
	SDL_Quit();
	puts("PASS: Smart Pro A accepts, B backs, X/Y unchanged, overlapping directions");
	return 0;
}
