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

#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <logger.h>
#include <cfg.h>

#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>

#include "libffplay.h"

bus_t *bus_create();
void bus_add(bus_t*, void(*)(void*), void*);
void bus_destoy(bus_t*);

logger_t *l;

int decode_packet(player_t *p, AVPacket *pkt, int *got_frame)
	{
	int decoded=0;
	int ret;
	AVCodecContext *ctx=p->in_ctx->streams[p->in_st_idx]->codec;

	ret=avcodec_decode_audio4(ctx, p->inframe, got_frame, pkt);
	if(ret<0)
		{
		error(l, "failed to decode packet %s", av_err2str(ret));
		goto err;
		}
	decoded=FFMIN(ret, pkt->size);


	if(*got_frame)
		{
		p->last_pts=p->inframe->pts!=AV_NOPTS_VALUE?p->inframe->pts:av_frame_get_best_effort_timestamp(p->inframe);
		ret=swr_convert_frame(p->swr, p->outframe, p->inframe);
		if(ret<0)
			{
			error(l, "failed to feed filter: %s", av_err2str(ret));
			goto err;
			}
		av_write_uncoded_frame(p->out_ctx, p->out_st_idx, p->outframe);
		if(ret<0 && ret!=AVERROR(EAGAIN) && ret!=AVERROR_EOF)
			{
			error(l, "error while filtering data: %s", av_err2str(ret));
			goto err;
			}
		}
	ret=decoded;
err:
	if(ctx->refcounted_frames)
		av_frame_unref(p->inframe);
	return ret;
	}

int player_open(player_t *p)
	{
	AVStream *st;
	AVCodec *dec=NULL;
	AVStream *s=p->out_ctx->streams[p->out_st_idx];
	int ret=avformat_open_input(&p->in_ctx, p->file, NULL, NULL);
	if (ret< 0)
		{
		error(l, "failed to open file '%s': %s", p->file, av_err2str(ret));
		goto err;
		}
	ret=avformat_find_stream_info(p->in_ctx, NULL);
	if(ret<0)
		{
		error(l, "failed to find stream: %s", av_err2str(ret));
		goto err;
		}
	ret=av_find_best_stream(p->in_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if(ret<0)
		{
		error(l, "failed to find best stream: %s", av_err2str(ret));
		goto err;
		}

	p->in_st_idx=ret;
	st=p->in_ctx->streams[ret];
	dec=avcodec_find_decoder(st->codec->codec_id);
	if(!dec)
		{
		error(l, "can't find codec");
		goto err;
		}
	ret=avcodec_open2(st->codec, dec, NULL);
	if(ret<0)
		{
		error(l, "failed to open codec %s", av_err2str(ret));
		goto err;
		}
	uint8_t ch_layout[64];
	int got_frame=0;

// TODO check if stream match first
	av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, s->codec->channel_layout);
	info(l, "%s (%d) %d %s", ch_layout, s->codec->channels, s->codec->sample_rate, av_get_sample_fmt_name(s->codec->sample_fmt));
	av_opt_set_int(p->swr, "ocl", s->codec->channel_layout, 0);
	av_opt_set_int(p->swr, "osr", s->codec->sample_rate, 0);
	av_opt_set_int(p->swr, "osf", s->codec->sample_fmt, 0);

	av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, st->codec->channel_layout);
	info(l, "%s (%d) %d %s", ch_layout, st->codec->channels, st->codec->sample_rate, av_get_sample_fmt_name(st->codec->sample_fmt));
	av_opt_set_int(p->swr, "icl", st->codec->channel_layout, 0);
	av_opt_set_int(p->swr, "isr", st->codec->sample_rate, 0);
	av_opt_set_int(p->swr, "isf", st->codec->sample_fmt, 0);
	ret=swr_init(p->swr);
	if(ret<0)
		{
		error(l, "failed to init resampler: %s", av_err2str(ret));
		goto err;
		}
	p->duration=p->in_ctx->streams[p->in_st_idx]->duration;
	return 0;
err:
	if(p->in_ctx)
		avformat_close_input(&p->in_ctx);
	return ret;
	}

void player_checkstate(player_t *p)
	{
	if(p->next_state!=PLAYER_STATE_NULL)
		{
		p->curr_state=p->next_state;
		p->next_state=PLAYER_STATE_NULL;
		if(p->on_state_change)
			bus_add(p->bus, (void(*)(void*))p->on_state_change, p);
		}
	}

void player_waitstate(player_t *p)
	{
	while(p->next_state==PLAYER_STATE_NULL)
		pthread_cond_wait(&p->cond, &p->mutex);
	player_checkstate(p);
	}

void *player_loop(void *arg)
	{
	player_t *p=arg;
	int ret, got_frame;
	AVPacket pkt;
start:
	pthread_mutex_lock(&p->mutex);
	while(p->curr_state==PLAYER_STATE_STOP || p->curr_state==PLAYER_STATE_NULL)
		player_waitstate(p);
	pthread_mutex_unlock(&p->mutex);
	if(p->curr_state==PLAYER_EXIT)
		goto flush;

	if(player_open(p)<0)
		{
		p->curr_state=PLAYER_STATE_STOP;
		goto start;
		}
	info(l, "playing '%s'", p->file);

	av_init_packet(&pkt);
	pkt.data=NULL;
	pkt.size=0;
	do
		{
		pthread_mutex_lock(&p->mutex);
		player_checkstate(p);
		while(p->curr_state==PLAYER_STATE_PAUSE)
			player_waitstate(p);
		pthread_mutex_unlock(&p->mutex);
		if(p->curr_state!=PLAYER_STATE_PLAY)
			goto flush;

		ret=av_read_frame(p->in_ctx, &pkt);
		if(ret>=0 && pkt.stream_index==p->in_st_idx)
			{
			AVPacket org=pkt;
			do
				{
				ret=decode_packet(p, &pkt, &got_frame);
				if(ret<0)
					{
					av_free_packet(&org);
					goto err;
					}
				pkt.data+=ret;
				pkt.size-=ret;
				} 
			while(pkt.size>0);
			av_free_packet(&org);
			}
		else
			av_free_packet(&pkt);
		}
	while(ret>=0);
flush:
	pkt.data=NULL;
	pkt.size=0;
	do
		{
		ret=decode_packet(p, &pkt, &got_frame);
		}
	while(got_frame);
err:
	avcodec_close(p->in_ctx->streams[p->in_st_idx]->codec);
	avformat_close_input(&p->in_ctx);
	if(p->curr_state==PLAYER_STATE_PLAY)
		{
		p->curr_state=PLAYER_STATE_STOP;
		info(l, "EOF");
		if(p->on_eof)
			bus_add(p->bus, (void(*)(void*))p->on_eof, p);
		}
	if(p->curr_state==PLAYER_EXIT)
		return;
	goto start;
	}

void player_play(player_t *p, const char *file)
	{
	int ret;
	int path_size=strlen(file)+1;

	player_setstate(p, PLAYER_STATE_STOP);
	while(p->next_state!=PLAYER_STATE_NULL)
		usleep(100);
	
	if(p->file_alloc<path_size)
		{
		char *path=realloc(p->file, path_size);
		if(path==NULL)
			return;
		p->file=path;
		p->file_alloc=path_size;
		}

	strcpy(p->file, file);
	
	player_setstate(p, PLAYER_STATE_PLAY);
	}

void player_setstate(player_t *p, player_state_e state)
	{
	p->next_state=state;
	pthread_cond_signal(&p->cond);
	}

void player_destroy(player_t *p)
	{
	p->next_state=PLAYER_EXIT;
	pthread_cond_signal(&p->cond);

	pthread_join(p->loop, NULL);
	pthread_cond_destroy(&p->cond);
	pthread_mutex_destroy(&p->mutex);

	av_write_trailer(p->out_ctx);
	
	avcodec_close(p->out_ctx->streams[p->out_st_idx]->codec);
	avformat_free_context(p->out_ctx);
	swr_free(&p->swr);
	bus_destroy(p->bus);

	av_frame_free(&p->outframe);
	av_frame_free(&p->inframe);

	free(p->file);
	free(p);
	}

int player_metadata(char *file, void (*callback)(const char *, const char *, void *), void *data)
	{
	AVFormatContext *fmt_ctx=NULL;
	AVDictionaryEntry *tag=NULL;

	info(l, "opening %s", file);
	int ret=avformat_open_input(&fmt_ctx, file, NULL, NULL);
	if(ret>=0)
		{
		ret=avformat_find_stream_info(fmt_ctx, NULL);
		if(ret>=0)
			{
			ret=av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
			if(ret>=0)
				{
				while((tag=av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
					callback(tag->key, tag->value, data);
				}
			}
		avformat_close_input(&fmt_ctx);
		}
	return ret>=0;
	}

player_t *player_init(char *outfile, char *outfmt)
	{
	if(l==NULL)
		l=get_logger("ffplay");
	player_t *p=malloc(sizeof(player_t));
	if(p)
		{
		memset(p, 0, sizeof(player_t));
		int ret=avformat_alloc_output_context2(&p->out_ctx, NULL, outfmt, outfile);
		if(ret<0)
			goto err;
		else
			{
			AVStream *s;
			AVCodec *c;

			c=avcodec_find_encoder(p->out_ctx->oformat->audio_codec);
			if(!c)
				{
				error(l, "failed to get encoder");
				goto err;
				}
			s=avformat_new_stream(p->out_ctx, c);
			if(!s)
				{
				error(l, "can't add new stream");
				goto err;
				}
			s->codec->sample_fmt=c->sample_fmts?c->sample_fmts[0]:AV_SAMPLE_FMT_FLTP;
			s->codec->sample_rate=44100;
			s->codec->channel_layout=AV_CH_LAYOUT_STEREO;
			ret=avcodec_open2(s->codec, c, NULL);
			if(ret<0)
				{
				error(l, "can't open codec %s", av_err2str(ret));
				goto err;
				}
			p->out_st_idx=s->index;
			avformat_write_header(p->out_ctx, NULL);

			p->swr=swr_alloc();
			av_opt_set_channel_layout(p->swr, "out_channel_layout", s->codec->channel_layout, 0);
			av_opt_set_int(p->swr, "out_sample_rate", s->codec->sample_rate, 0);
			av_opt_set_sample_fmt(p->swr, "out_sample_fmt", s->codec->sample_fmt, 0);

			p->outframe=av_frame_alloc();
			p->outframe->channel_layout=s->codec->channel_layout;
			p->outframe->sample_rate=s->codec->sample_rate;
			p->outframe->format=s->codec->sample_fmt;

			p->inframe=av_frame_alloc();

			p->bus=bus_create();

			pthread_mutex_init(&p->mutex, NULL);
			pthread_cond_init(&p->cond, NULL);

			pthread_create(&p->loop, NULL, player_loop, p);
			}
		}
	return p;
err:
	player_destroy(p);
	return NULL;
	}
