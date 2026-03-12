#include "config.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "runtime/perf.h"
#include <abi/syscalls.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static constexpr uint32_t DOOM_KEY_QUEUE_CAPACITY = 128u;
static constexpr uint32_t DOOM_ARGV_CAPACITY	  = 16u;
static constexpr uint32_t DOOM_CMDLINE_CAPACITY	  = 256u;

void I_Quit(void);

typedef struct doom_key_event {
	unsigned char key;
	uint8_t	      pressed;
	uint8_t	      _pad[2];
} doom_key_event_t;

typedef struct doom_cubeos_state {
	sys_desktop_channel_info_t channel;
	uint32_t		  *surface;
	uint32_t		   start_time_us;
	uint32_t		   key_head;
	uint32_t		   key_tail;
	bool			   close_requested;
	char			   cmdline[DOOM_CMDLINE_CAPACITY];
	char			  *argv[DOOM_ARGV_CAPACITY];
	int			   argc;
	doom_key_event_t	   key_queue[DOOM_KEY_QUEUE_CAPACITY];
} doom_cubeos_state_t;

static doom_cubeos_state_t g_doom;
static char		   g_program_name[] = "doom";

static const sys_desktop_window_desc_t g_doom_window = {
	.x	  = -1,
	.y	  = -1,
	.width	  = DOOMGENERIC_RESX,
	.height	  = DOOMGENERIC_RESY,
	.base_r	  = 0x12,
	.base_g	  = 0x16,
	.base_b	  = 0x1D,
	.accent_r = 0xB7,
	.accent_g = 0x2D,
	.accent_b = 0x1E,
};

static inline bool doom_key_queue_full(void)
{
	return (uint32_t)(g_doom.key_head - g_doom.key_tail) >= DOOM_KEY_QUEUE_CAPACITY;
}

static inline bool doom_key_queue_empty(void)
{
	return g_doom.key_head == g_doom.key_tail;
}

static void doom_key_queue_push(int pressed, unsigned char key)
{
	if (key == 0 || doom_key_queue_full())
		return;

	g_doom.key_queue[g_doom.key_head & (DOOM_KEY_QUEUE_CAPACITY - 1u)] = (doom_key_event_t){
		.key	 = key,
		.pressed = (uint8_t)(pressed != 0),
	};
	g_doom.key_head++;
}

static bool doom_key_queue_pop(int *pressed, unsigned char *key)
{
	if (!pressed || !key || doom_key_queue_empty())
		return false;

	doom_key_event_t event = g_doom.key_queue[g_doom.key_tail & (DOOM_KEY_QUEUE_CAPACITY - 1u)];
	g_doom.key_tail++;

	*pressed = event.pressed != 0;
	*key	 = event.key;
	return true;
}

static unsigned char doom_translate_hid_key(const sys_key_event_t *event)
{
	if (!event)
		return 0;

	switch (event->keycode) {
	case 0x28:
		return KEY_ENTER;
	case 0x29:
		return KEY_ESCAPE;
	case 0x2a:
		return KEY_BACKSPACE;
	case 0x2b:
		return KEY_TAB;
	case 0x2c:
		return KEY_USE;
	case 0x2d:
		return KEY_MINUS;
	case 0x2e:
		return KEY_EQUALS;
	case 0x39:
		return KEY_CAPSLOCK;
	case 0x3a:
		return KEY_F1;
	case 0x3b:
		return KEY_F2;
	case 0x3c:
		return KEY_F3;
	case 0x3d:
		return KEY_F4;
	case 0x3e:
		return KEY_F5;
	case 0x3f:
		return KEY_F6;
	case 0x40:
		return KEY_F7;
	case 0x41:
		return KEY_F8;
	case 0x42:
		return KEY_F9;
	case 0x43:
		return KEY_F10;
	case 0x44:
		return KEY_F11;
	case 0x45:
		return KEY_F12;
	case 0x48:
		return KEY_PAUSE;
	case 0x49:
		return KEY_INS;
	case 0x4a:
		return KEY_HOME;
	case 0x4b:
		return KEY_PGUP;
	case 0x4c:
		return KEY_DEL;
	case 0x4d:
		return KEY_END;
	case 0x4e:
		return KEY_PGDN;
	case 0x4f:
		return KEY_RIGHTARROW;
	case 0x50:
		return KEY_LEFTARROW;
	case 0x51:
		return KEY_DOWNARROW;
	case 0x52:
		return KEY_UPARROW;
	case 0x53:
		return KEY_NUMLOCK;
	case 0x54:
		return KEYP_DIVIDE;
	case 0x55:
		return KEYP_MULTIPLY;
	case 0x56:
		return KEYP_MINUS;
	case 0x57:
		return KEYP_PLUS;
	case 0x58:
		return KEYP_ENTER;
	case 0x59:
		return KEYP_1;
	case 0x5a:
		return KEYP_2;
	case 0x5b:
		return KEYP_3;
	case 0x5c:
		return KEYP_4;
	case 0x5d:
		return KEYP_5;
	case 0x5e:
		return KEYP_6;
	case 0x5f:
		return KEYP_7;
	case 0x60:
		return KEYP_8;
	case 0x61:
		return KEYP_9;
	case 0x62:
		return KEYP_0;
	case 0x63:
		return KEYP_PERIOD;
	case 0x67:
		return KEYP_EQUALS;
	case 0xe0:
	case 0xe4:
		return KEY_FIRE;
	case 0xe1:
	case 0xe5:
		return KEY_RSHIFT;
	case 0xe2:
	case 0xe6:
		return KEY_LALT;
	default:
		break;
	}

	if (event->keycode >= 0x04 && event->keycode <= 0x1d)
		return (unsigned char)('a' + (event->keycode - 0x04));

	if (event->keycode >= 0x1e && event->keycode <= 0x26)
		return (unsigned char)('1' + (event->keycode - 0x1e));

	if (event->keycode == 0x27)
		return '0';

	switch (event->keycode) {
	case 0x2f:
		return '[';
	case 0x30:
		return ']';
	case 0x31:
		return '\\';
	case 0x33:
		return ';';
	case 0x34:
		return '\'';
	case 0x35:
		return '`';
	case 0x36:
		return ',';
	case 0x37:
		return '.';
	case 0x38:
		return '/';
	default:
		break;
	}

	if (event->ascii >= 32 && event->ascii <= 126) {
		unsigned char key = event->ascii;
		if (key >= 'A' && key <= 'Z')
			key = (unsigned char)(key - ('A' - 'a'));
		return key == ' ' ? KEY_USE : key;
	}

	return 0;
}

static void doom_handle_window_message(const sys_desktop_msg_t *msg)
{
	if (!msg)
		return;

	switch (msg->type) {
	case SYS_DESKTOP_MSG_KEY_EVENT: {
		unsigned char key = doom_translate_hid_key(&msg->payload.key);
		if (key != 0)
			doom_key_queue_push(msg->payload.key.pressed, key);
		break;
	}
	case SYS_DESKTOP_MSG_CLOSE_REQUEST:
	case SYS_DESKTOP_MSG_PEER_CLOSED:
		g_doom.close_requested = true;
		break;
	default:
		break;
	}
}

static void doom_pump_window_messages(void)
{
	PERF_USCOPE();
	if (g_doom.channel.handle == 0)
		return;

	for (;;) {
		sys_desktop_msg_t msg;
		int32_t		  result = syscall_desktop_recv(g_doom.channel.handle, &msg, 0);
		if (result == SYS_DESKTOP_RECV_TIMEOUT)
			break;
		if (result < 0) {
			g_doom.close_requested = true;
			break;
		}
		doom_handle_window_message(&msg);
	}
}

static void doom_present(void)
{
	PERF_USCOPE();
	if (g_doom.channel.handle == 0)
		return;

	sys_desktop_msg_t redraw = {
		.type	  = SYS_DESKTOP_MSG_REDRAW,
		.reserved = 0,
	};
	if (syscall_desktop_send(g_doom.channel.handle, &redraw) == 0) {
		// Real hardware needs a cooperative handoff here so the compositor can
		// drain the redraw and present it before Doom starts the next frame.
		syscall_sleep(0);
	}
}

static void doom_open_window(void)
{
	if (syscall_desktop_open_window(&g_doom_window, &g_doom.channel) != 0)
		syscall_exit();

	g_doom.surface = (uint32_t *)(uintptr_t)g_doom.channel.buffer;
	if (!g_doom.surface)
		syscall_exit();
}

static void doom_build_argv(const char *cmdline)
{
	g_doom.argc    = 1;
	g_doom.argv[0] = g_program_name;

	if (!cmdline || cmdline[0] == '\0') {
		g_doom.argv[1] = NULL;
		return;
	}

	uint32_t len = 0;
	while (cmdline[len] != '\0' && len + 1u < sizeof(g_doom.cmdline)) {
		g_doom.cmdline[len] = cmdline[len];
		len++;
	}
	g_doom.cmdline[len] = '\0';

	char *cursor = g_doom.cmdline;
	while (*cursor != '\0' && g_doom.argc + 1 < (int)DOOM_ARGV_CAPACITY) {
		while (*cursor != '\0' && isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor == '\0')
			break;

		g_doom.argv[g_doom.argc++] = cursor;
		while (*cursor != '\0' && !isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor == '\0')
			break;

		*cursor = '\0';
		cursor++;
	}

	g_doom.argv[g_doom.argc] = NULL;
}

void DG_Init(void)
{
	doom_open_window();
	g_doom.start_time_us   = syscall_time_us();
	g_doom.key_head	       = 0;
	g_doom.key_tail	       = 0;
	g_doom.close_requested = false;

	for (uint32_t i = 0; i < DOOMGENERIC_RESX * DOOMGENERIC_RESY; i++)
		g_doom.surface[i] = 0;
	doom_present();
}

void DG_DrawFrame(void)
{
	PERF_USCOPE();
	if (!g_doom.surface || !DG_ScreenBuffer)
		return;

	memcpy(g_doom.surface, DG_ScreenBuffer,
	       DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint32_t));

	doom_present();
	doom_pump_window_messages();
}

void DG_SleepMs(uint32_t ms)
{
	uint32_t deadline = syscall_time_us() + ms * 1000u;

	while ((int32_t)(deadline - syscall_time_us()) > 0) {
		uint32_t now	      = syscall_time_us();
		uint32_t remaining_us = deadline - now;
		uint32_t sleep_ticks  = remaining_us / 5000u;
		if (sleep_ticks == 0)
			sleep_ticks =
				1; // Hier vlt 0 zulassen. und daraus quasi eine busy loop machen
		syscall_sleep(sleep_ticks);
	}
}

uint32_t DG_GetTicksMs(void)
{
	return (syscall_time_us() - g_doom.start_time_us) / 1000u;
}

int DG_GetKey(int *pressed, unsigned char *key)
{
	doom_pump_window_messages();
	int result = doom_key_queue_pop(pressed, key) ? 1 : 0;
	return result;
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}

void main(void *args)
{
	memset(&g_doom, 0, sizeof(g_doom));
	doom_build_argv((const char *)args);

	doomgeneric_Create(g_doom.argc, g_doom.argv);
	PERF_UINIT();

	while (!g_doom.close_requested) {
		doom_pump_window_messages();
		if (!g_doom.close_requested) {
			doomgeneric_Tick();
		}
		PERF_UREPORT_IF_DUE();
		if (g_doom.close_requested)
			break;
	}

	I_Quit();
	syscall_exit();
}
