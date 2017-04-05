/* gamma-gnomerr.h -- Gnome RR gamma adjustment header
   This file is part of Redshift.

   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2010-2014  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifndef REDSHIFT_GAMMA_GNOMERR_H
#define REDSHIFT_GAMMA_GNOMERR_H

#include <stdio.h>
#include <stdint.h>

#include <glib.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>

#include "redshift.h"


typedef struct {
	guint32 crtc;
	unsigned int ramp_size;
	unsigned short *saved_ramps;
} gnomerr_crtc_state_t;

typedef struct {
	GnomeRRScreen *screen;
	int preserve;
	int crtc_num_count;
	int* crtc_num;
	unsigned int crtc_count;
	gnomerr_crtc_state_t *crtcs;
} gnomerr_state_t;


int gnomerr_init(gnomerr_state_t *state);
int gnomerr_start(gnomerr_state_t *state);
void gnomerr_free(gnomerr_state_t *state);

void gnomerr_print_help(FILE *f);
int gnomerr_set_option(gnomerr_state_t *state, const char *key, const char *value);

void gnomerr_restore(gnomerr_state_t *state);
int gnomerr_set_temperature(gnomerr_state_t *state,
			  const color_setting_t *setting);


#endif /* ! REDSHIFT_GAMMA_GNOMERR_H */
