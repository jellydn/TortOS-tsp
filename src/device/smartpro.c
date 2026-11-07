/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Regenerated TG5040 port from pinned upstream and recovered transcript text.
 * This is not claimed to be the lost final source. Not for Smart Pro S. */
#include "db.h"
#include "device.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <alsa/asoundlib.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#endif

const char *P_ROOT = "/mnt/SDCARD/TortOS";
const char *P_CARD = "/mnt/SDCARD";
const char *P_ROMS = "/mnt/SDCARD/Roms";
const char *P_USERDATA = "/mnt/SDCARD/.userdata/smartpro";
const char *P_SHARED = "/mnt/SDCARD/.userdata/shared";
const char *P_WEB = "/mnt/SDCARD/TortOS/res/web";
const char *P_FONT = "/mnt/SDCARD/TortOS/menu.ttf";

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *screen;
static SDL_Joystick *pad;
static int keys[32], nkeys;
static bool keyboard[IN_COUNT], hardware[IN_COUNT], pulses[IN_COUNT];
static bool menu_down, menu_used, menu_pulse;
static in_button volume_as[2] = { IN_NONE, IN_NONE };
static int volume = 8, brightness = 6, muted = -1;
static int disp_fd = -1, jack_fd = -1;
#ifdef __linux__
static snd_mixer_t *mixer;
#endif

SDL_Renderer *plat_renderer(void) { return ren; }
void plat_draw_to_screen(void) { SDL_SetRenderTarget(ren, screen); }

void plat_present(void)
{
	int w, h;
	Uint8 r, g, b, a;
	SDL_Rect dst;
	SDL_GetRenderDrawColor(ren, &r, &g, &b, &a);
	/* OutputSize describes the current target, not necessarily the panel. */
	SDL_SetRenderTarget(ren, NULL);
	SDL_GetRendererOutputSize(ren, &w, &h);
	dst.w = h * TORTOS_SCREEN_W / TORTOS_SCREEN_H;
	dst.h = h;
	if (dst.w > w) {
		dst.w = w;
		dst.h = w * TORTOS_SCREEN_H / TORTOS_SCREEN_W;
	}
	dst.x = (w - dst.w) / 2;
	dst.y = (h - dst.h) / 2;
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, screen, NULL, &dst);
	SDL_RenderPresent(ren);
	plat_draw_to_screen();
	SDL_SetRenderDrawColor(ren, r, g, b, a);
}

bool plat_video_init(void)
{
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return false;
	SDL_ShowCursor(SDL_DISABLE);
	win = SDL_CreateWindow("TortOS", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                       1280, 720, SDL_WINDOW_SHOWN);
	if (!win) goto fail;
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED |
	                         SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	if (!ren) goto fail;
	screen = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
	                           TORTOS_SCREEN_W, TORTOS_SCREEN_H);
	if (!screen) goto fail;
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	plat_draw_to_screen();
	return true;
fail:
	fprintf(stderr, "smartpro video: %s\n", SDL_GetError());
	plat_video_quit();
	return false;
}

void plat_video_quit(void)
{
	if (screen) SDL_DestroyTexture(screen);
	if (ren) SDL_DestroyRenderer(ren);
	if (win) SDL_DestroyWindow(win);
	screen = NULL; ren = NULL; win = NULL;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* The owner reports physical B selecting with the prior index1->ACCEPT map.
 * Reverse launcher A/B only; a labeled hardware trace must verify this runtime
 * against MinUI's differing stock label contract. Keep X/Y/game mapping intact. */
static const in_button buttons[] = {
	IN_ACCEPT, IN_BACK, IN_Y, IN_X, IN_L1, IN_R1, IN_SELECT, IN_START
};

static void menu_event(bool down)
{
	if (down) { menu_down = true; menu_used = false; }
	else {
		if (menu_down && !menu_used) menu_pulse = true;
		menu_down = false;
	}
}

static void volume_event(int k, bool down)
{
	if (down && volume_as[k] == IN_NONE) {
		volume_as[k] = menu_down ? (k ? IN_BRIGHTDN : IN_BRIGHTUP)
		                         : (k ? IN_VOLDN : IN_VOLUP);
		if (menu_down) menu_used = true;
		hardware[volume_as[k]] = true;
		pulses[volume_as[k]] = true;
	} else if (!down && volume_as[k] != IN_NONE) {
		hardware[volume_as[k]] = false;
		volume_as[k] = IN_NONE;
	}
}

bool plat_input_init(void)
{
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) return false;
	if (SDL_NumJoysticks() > 0) pad = SDL_JoystickOpen(0);
#ifdef __linux__
	if (nkeys == 0) for (int i = 0; i < 32; i++) {
		char path[64];
		unsigned char caps[KEY_MAX / 8 + 1] = { 0 };
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0) continue;
		ioctl(fd, EVIOCGBIT(EV_KEY, sizeof caps), caps);
		if ((caps[KEY_POWER / 8] & (1 << (KEY_POWER % 8))) ||
		    (caps[KEY_VOLUMEUP / 8] & (1 << (KEY_VOLUMEUP % 8))))
			keys[nkeys++] = fd;
		else close(fd);
	}
#endif
	return true;
}

void plat_input_flush(void)
{
	SDL_Event e;
	SDL_PumpEvents();
	while (SDL_PollEvent(&e)) { }
#ifdef __linux__
	struct input_event ev;
	for (int i = 0; i < nkeys; i++)
		while (read(keys[i], &ev, sizeof ev) == sizeof ev) { }
#endif
	memset(keyboard, 0, sizeof keyboard);
	memset(hardware, 0, sizeof hardware);
	memset(pulses, 0, sizeof pulses);
	menu_down = menu_used = menu_pulse = false;
	volume_as[0] = volume_as[1] = IN_NONE;
}

void plat_input_settle(in_state *st)
{
	plat_input_flush();
	memset(st, 0, sizeof *st);
}

void plat_input_quit(void)
{
	if (pad) SDL_JoystickClose(pad);
	pad = NULL;
	/* Retain power input while SDL is released for the owned game. */
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static in_button map_key(SDL_Keycode k)
{
	switch (k) {
	case SDLK_LEFT: return IN_LEFT;
	case SDLK_RIGHT: return IN_RIGHT;
	case SDLK_UP: return IN_UP;
	case SDLK_DOWN: return IN_DOWN;
	case SDLK_RETURN: return IN_ACCEPT;
	case SDLK_ESCAPE: case SDLK_BACKSPACE: return IN_BACK;
	default: return IN_NONE;
	}
}

void plat_input_poll(in_state *st)
{
	SDL_Event e;
	memset(st->pressed, 0, sizeof st->pressed);
	if (plat_terminating()) st->quit_requested = true;
	plat_audio_jack_poll();
	plat_mute_poll(true);
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) st->quit_requested = true;
		if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			bool down = e.type == SDL_KEYDOWN;
			in_button b = map_key(e.key.keysym.sym);
			if (e.key.repeat) continue;
			if (b != IN_NONE) {
				keyboard[b] = down;
				if (down) pulses[b] = true;
			}
			if (e.key.keysym.sym == SDLK_TAB) menu_event(down);
			if (e.key.keysym.sym == SDLK_EQUALS) volume_event(0, down);
			if (e.key.keysym.sym == SDLK_MINUS) volume_event(1, down);
			if (down && e.key.keysym.sym == SDLK_q) st->quit_requested = true;
		}
		if (pad && (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) &&
		    e.jbutton.which == SDL_JoystickInstanceID(pad)) {
			bool down = e.type == SDL_JOYBUTTONDOWN;
			if (e.jbutton.button == 8) menu_event(down);
			if (e.jbutton.button == 128) volume_event(0, down);
			if (e.jbutton.button == 129) volume_event(1, down);
			if (down && e.jbutton.button < sizeof buttons / sizeof buttons[0])
				pulses[buttons[e.jbutton.button]] = true;
		}
	}
#ifdef __linux__
	struct input_event ev;
	for (int i = 0; i < nkeys; i++) {
		while (read(keys[i], &ev, sizeof ev) == sizeof ev) {
			if (ev.type != EV_KEY || ev.value == 2) continue;
			if (ev.code == KEY_VOLUMEUP) volume_event(0, ev.value == 1);
			if (ev.code == KEY_VOLUMEDOWN) volume_event(1, ev.value == 1);
			if (ev.code == KEY_POWER) {
				hardware[IN_POWER] = ev.value == 1;
				if (ev.value == 1) pulses[IN_POWER] = true;
			}
		}
	}
#endif
	bool held[IN_COUNT];
	for (int i = 0; i < IN_COUNT; i++) held[i] = keyboard[i] || hardware[i];
	if (pad && SDL_JoystickGetAttached(pad)) {
		for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++)
			held[buttons[i]] |= SDL_JoystickGetButton(pad, i) != 0;
		Uint8 hat = SDL_JoystickGetHat(pad, 0);
		held[IN_LEFT] |= (hat & SDL_HAT_LEFT) != 0;
		held[IN_RIGHT] |= (hat & SDL_HAT_RIGHT) != 0;
		held[IN_UP] |= (hat & SDL_HAT_UP) != 0;
		held[IN_DOWN] |= (hat & SDL_HAT_DOWN) != 0;
		for (int i = 0; i < 2; i++) {
			int x = SDL_JoystickGetAxis(pad, i ? 3 : 0);
			int y = SDL_JoystickGetAxis(pad, i ? 4 : 1);
			held[IN_LEFT] |= x < -12000; held[IN_RIGHT] |= x > 12000;
			held[IN_UP] |= y < -12000; held[IN_DOWN] |= y > 12000;
		}
	}
	held[IN_MENU] = menu_pulse;
	menu_pulse = false;
	for (int i = 0; i < IN_COUNT; i++) {
		if (pulses[i]) plat_set_btn(st, i, true);
		plat_set_btn(st, i, held[i]);
	}
	memset(pulses, 0, sizeof pulses);
}

bool device_power_pressed(void)
{
#ifdef __linux__
	bool pressed = false;
	struct input_event ev;
	for (int i = 0; i < nkeys; i++)
		while (read(keys[i], &ev, sizeof ev) == sizeof ev)
			if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 1)
				pressed = true;
	return pressed;
#else
	return false;
#endif
}

void device_display_release(void)
{
	void (*finish)(void) = SDL_GL_GetProcAddress("glFinish");
	if (finish) finish();
}
void device_display_take(void) { }

static int read_int(const char *path, int fallback)
{
	FILE *f = fopen(path, "r");
	int value;
	if (!f) return fallback;
	if (fscanf(f, "%d", &value) != 1) value = fallback;
	fclose(f);
	return value;
}
static int clamp(int v, int max) { return v < 0 ? 0 : v > max ? max : v; }
static const unsigned char bright_ladder[] = {
	4, 6, 10, 16, 24, 32, 48, 64, 96, 128, 192, 255
};
_Static_assert(sizeof bright_ladder - 1 == PLAT_BRIGHT_MAX, "brightness scale");
static void raw_brightness(int raw)
{
#ifdef __linux__
	unsigned long args[4] = { 0, raw, 0, 0 };
	if (disp_fd >= 0) ioctl(disp_fd, 0x102, args);
#endif
}
static void apply_volume(void)
{
#ifdef __linux__
	if (!mixer) return;
	for (snd_mixer_elem_t *e = snd_mixer_first_elem(mixer); e; e = snd_mixer_elem_next(e)) {
		const char *name = snd_mixer_selem_get_name(e);
		long lo, hi;
		if (strcmp(name, "digital volume") && strcmp(name, "DAC volume")) continue;
		if (!snd_mixer_selem_has_playback_volume(e)) continue;
		snd_mixer_selem_get_playback_volume_range(e, &lo, &hi);
		bool cut = volume == 0 || muted == 1;
		long unity = hi < 160 ? hi : 160;
		long raw = !strcmp(name, "DAC volume") ? (cut ? lo : unity)
		         : hi - (hi - lo) * volume / PLAT_VOL_MAX;
		if (snd_mixer_selem_set_playback_volume_all(e, raw) < 0)
			fprintf(stderr, "smartpro: cannot set %s\n", name);
	}
#endif
}

void plat_settings_init(void)
{
	volume = clamp(db_get_int(db_dev(), "volume", 8), PLAT_VOL_MAX);
	brightness = clamp(db_get_int(db_dev(), "brightness", 6), PLAT_BRIGHT_MAX);
#ifdef __linux__
	disp_fd = open("/dev/disp", O_RDWR | O_CLOEXEC);
	if (snd_mixer_open(&mixer, 0) >= 0 &&
	    (snd_mixer_attach(mixer, "default") < 0 ||
	     snd_mixer_selem_register(mixer, NULL, NULL) < 0 || snd_mixer_load(mixer) < 0)) {
		snd_mixer_close(mixer); mixer = NULL;
	}
	for (snd_mixer_elem_t *e = mixer ? snd_mixer_first_elem(mixer) : NULL; e;
	     e = snd_mixer_elem_next(e)) {
		const char *name = snd_mixer_selem_get_name(e);
		if (!strcmp(name, "Headphone") && snd_mixer_selem_has_playback_volume(e))
			snd_mixer_selem_set_playback_volume_all(e, 0);
		if (!strcmp(name, "DAC Swap") && snd_mixer_selem_is_enumerated(e))
			snd_mixer_selem_set_enum_item(e, SND_MIXER_SCHN_FRONT_LEFT, 0);
	}
	for (int i = 0; i < 32 && jack_fd < 0; i++) {
		char path[64];
		unsigned long bits = 0;
		snprintf(path, sizeof path, "/dev/input/event%d", i);
		int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0) continue;
		ioctl(fd, EVIOCGBIT(EV_SW, sizeof bits), &bits);
		if (bits & (1UL << SW_HEADPHONE_INSERT)) jack_fd = fd;
		else close(fd);
	}
#endif
	muted = read_int("/sys/class/gpio/gpio243/value", 0) != 0;
	apply_volume();
	raw_brightness(bright_ladder[brightness]);
}
int plat_volume_get(void) { return volume; }
int plat_brightness_get(void) { return brightness; }
void plat_volume_set_pct(int pct)
{
	volume = (clamp(pct, 100) * PLAT_VOL_MAX + 50) / 100;
	apply_volume();
	db_set_int(db_dev(), "volume", volume);
}
void plat_volume_nudge(int delta)
{
	volume = clamp(volume + delta, PLAT_VOL_MAX);
	apply_volume();
	db_set_int(db_dev(), "volume", volume);
	plat_osd_show(2, volume, PLAT_VOL_MAX);
}
void plat_brightness_set(int level)
{
	brightness = clamp(level, PLAT_BRIGHT_MAX);
	raw_brightness(bright_ladder[brightness]);
	db_set_int(db_dev(), "brightness", brightness);
}
void plat_brightness_nudge(int delta)
{
	plat_brightness_set(brightness + delta);
	plat_osd_show(1, brightness, PLAT_BRIGHT_MAX);
}
void plat_backlight(bool on) { raw_brightness(on ? bright_ladder[brightness] : 0); }
bool plat_mute_poll(bool own_volume)
{
	int now = read_int("/sys/class/gpio/gpio243/value", 0) != 0;
	if (now == muted) return false;
	muted = now;
	if (own_volume) apply_volume();
	else plat_resident_tell("SETMUTE\ton=%d", muted);
	return true;
}
bool plat_muted(void) { return muted == 1; }
void device_levels_forget(void) { muted = -1; }
void plat_audio_jack_poll(void) { }
bool plat_headphones_present(void)
{
#ifdef __linux__
	unsigned long bits = 0;
	return jack_fd >= 0 && ioctl(jack_fd, EVIOCGSW(sizeof bits), &bits) == 0 &&
	       (bits & (1UL << SW_HEADPHONE_INSERT));
#else
	return false;
#endif
}
bool plat_usb_audio_present(void) { return false; }
bool plat_cable_link(char *addr, size_t n) { (void)addr; (void)n; return false; }
void plat_cable_reconnect(void) { }
bool plat_has_wifi(void) { return false; }
bool plat_has_bluetooth(void) { return false; }
const char *plat_brightness_modifier(void) { return "MENU"; }
bool plat_battery_lights(void) { return false; }
bool plat_battery(int *pct, bool *charging)
{
	int fake = read_int("/tmp/tortos_fake_batt", -1);
	int value = fake >= 0 ? fake :
	            read_int("/sys/class/power_supply/axp2202-battery/capacity", -1);
	if (value < 0 || value > 100) return false;
	if (pct) *pct = value;
	if (charging) *charging = fake < 0 &&
		read_int("/sys/class/power_supply/axp2202-usb/online", 0) == 1;
	return true;
}
