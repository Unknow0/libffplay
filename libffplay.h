/*******************************************************************************
 * This file is part of libffplay.
 *
 * libffplay is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * libffplay is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with libffplay; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 ******************************************************************************/
#ifndef _LIBFFPLAY_H
#define _LIBFFPLAY_H

#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libswresample/swresample.h>

#define BUS_MAX_EVENT 50

typedef enum player_state
	{
	PLAYER_STATE_NULL,
	PLAYER_STATE_STOP,
	PLAYER_STATE_PLAY,
	PLAYER_STATE_PAUSE,
	PLAYER_EXIT
	} player_state_e;

struct event
	{
	void (*callback)(void*);
	void *arg;
	struct event *next;
	};

typedef struct bus_t
	{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t loop;


	unsigned int event_off;
	unsigned int event_count;
	struct event event[BUS_MAX_EVENT];
	} bus_t;

typedef struct player
	{
	AVFormatContext *in_ctx;
	AVPacket *pkt;
	int in_st_idx;
	AVFormatContext *out_ctx;
	int out_st_idx;
	SwrContext *swr;

	pthread_t loop;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	player_state_e curr_state;
	player_state_e next_state;
	char *file;
	int file_alloc;

	/* callback */
	void (*onEof)(struct player *);

	bus_t *bus;
	} player_t;

player_t *player_init();

void player_play(player_t *player, const char *);
void player_setstate(player_t *p, player_state_e state);

void player_destroy(player_t *player);

int player_metadata(char *file, void (*callback)(const char *, const char *, void *), void *data);

#endif
