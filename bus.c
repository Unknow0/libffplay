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
#include <logger.h>

#include "libffplay.h"

void *bus_loop(void *arg)
	{
	bus_t *bus=arg;
	struct event *e;

	pthread_mutex_lock(&bus->mutex);
start:
	while(bus->event_count==0)
		pthread_cond_wait(&bus->cond, &bus->mutex);
	e=bus->event+bus->event_off;
	pthread_mutex_unlock(&bus->mutex);

	e->callback(e->arg);

	pthread_mutex_lock(&bus->mutex);
	bus->event_off=(bus->event_off+1)%BUS_MAX_EVENT;
	bus->event_count--;
	goto start;
	}

bus_t *bus_create()
	{
	bus_t *bus=malloc(sizeof(bus_t));
	memset(bus, 0, sizeof(bus_t));	

	pthread_mutex_init(&bus->mutex, NULL);
	pthread_cond_init(&bus->cond, NULL);
	pthread_create(&bus->loop, NULL, &bus_loop, bus);
	return bus;
	}

void bus_destroy(bus_t *bus)
	{
	pthread_cancel(bus->loop);
	pthread_cond_signal(&bus->cond);
	pthread_join(bus->loop);

	pthread_cond_destroy(&bus->cond);
	pthread_mutex_destroy(&bus->mutex);
	}

void bus_add(bus_t *bus, void (*callback)(void*), void *arg)
	{
	pthread_mutex_lock(&bus->mutex);
	if(bus->event_count<BUS_MAX_EVENT)
		{
		unsigned int i=(bus->event_off+bus->event_count)%BUS_MAX_EVENT;
		bus->event[i].callback=callback;
		bus->event[i].arg=arg;
		bus->event_count++;
		pthread_cond_signal(&bus->cond);
		}
	pthread_mutex_unlock(&bus->mutex);
	}
