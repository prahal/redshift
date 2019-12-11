/* gamma-gnomerr.c -- Gnome RR gamma adjustment source
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(s) gettext(s)
#else
# define _(s) s
#endif

#include <glib.h>
#include <gtk/gtk.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>

#include "gamma-gnomerr.h"
#include "redshift.h"
#include "colorramp.h"


typedef struct {
	guint32 crtc;
	unsigned int ramp_size;
	unsigned short *saved_ramps;
	gboolean is_gamma_valid;
} gnomerr_crtc_state_t;

typedef struct {
	GnomeRRScreen *screen;
	int preserve;
	int crtc_num_count;
	int* crtc_num;
	unsigned int crtc_count;
	gnomerr_crtc_state_t *crtcs;
} gnomerr_state_t;


#define RANDR_VERSION_MAJOR  1
#define RANDR_VERSION_MINOR  3


static int
gnomerr_init(gnomerr_state_t **state)
{
	/* Initialize state. */
	*state = malloc(sizeof(gnomerr_state_t));
	if (*state == NULL) return -1;

	gnomerr_state_t *s = *state;
	s->crtc_num = NULL;

	s->crtc_num_count = 0;
	s->crtc_count = 0;
	s->crtcs = NULL;

	s->preserve = 0;

	return 0;
}

static int
gnomerr_start(gnomerr_state_t *state)
{
	GError *error = NULL;
	guint i, count;

	/* Get screen */
	gtk_init(NULL, NULL);
	state->screen = gnome_rr_screen_new(gdk_screen_get_default(), &error);
	if (state->screen == NULL) {
		g_printerr(_("Screen could not be found: %s.\n"),
			error->message);
		g_error_free(error);
		return -1;
	}

	/* Get list of CRTCs for the screen */
	GnomeRRCrtc **crtcs = gnome_rr_screen_list_crtcs(state->screen);

	count = 0;
	for (i = 0; crtcs[i] != NULL; i++) count++;
	state->crtc_count = count;

	state->crtcs = calloc(state->crtc_count, sizeof(gnomerr_crtc_state_t));
	if (state->crtcs == NULL) {
		perror("malloc");
		state->crtc_count = 0;
		return -1;
	}

	/* Save CRTC identifier in state
	   Save size and gamma ramps of all CRTCs.
	   Current gamma ramps are saved so we can restore them
	   at program exit. */
	for (int i = 0; crtcs[i] != NULL && i < state->crtc_count; i++) {
		state->crtcs[i].crtc = gnome_rr_crtc_get_id(crtcs[i]);

		/* Request size of gamma ramps
		   Request current gamma ramps */
		unsigned int ramp_size;
		unsigned short *gamma_r;
		unsigned short *gamma_g;
		unsigned short *gamma_b;

		state->crtcs[i].is_gamma_valid = TRUE;
		if (!gnome_rr_crtc_get_gamma(crtcs[i],
					    &ramp_size,
					    &gamma_r,
					    &gamma_g,
					    &gamma_b)) {
			g_printerr(_("`%s' cannot get gamma from crtc %d.\n"),
			"", state->crtcs[i].crtc);
			return -1;
		}

		state->crtcs[i].ramp_size = ramp_size;

		if (ramp_size == 0)
			state->crtcs[i].is_gamma_valid = FALSE;

		/* Allocate space for saved gamma ramps */
		state->crtcs[i].saved_ramps =
			malloc(3*ramp_size*sizeof(unsigned short));
		if (state->crtcs[i].saved_ramps == NULL) {
			perror("malloc");
			return -1;
		}

		/* Copy gamma ramps into CRTC state */
		memcpy(&state->crtcs[i].saved_ramps[0*ramp_size], gamma_r,
		       ramp_size*sizeof(unsigned short));
		memcpy(&state->crtcs[i].saved_ramps[1*ramp_size], gamma_g,
		       ramp_size*sizeof(unsigned short));
		memcpy(&state->crtcs[i].saved_ramps[2*ramp_size], gamma_b,
		       ramp_size*sizeof(unsigned short));

		g_free(gamma_r);
		g_free(gamma_b);
		g_free(gamma_g);
	}

	return 0;
}

static void
gnomerr_restore(gnomerr_state_t *state)
{
	/* Restore CRTC gamma ramps */
	for (int i = 0; i < state->crtc_count; i++) {
		if (state->crtcs[i].is_gamma_valid == FALSE)
			return;

		guint32 crtc_id = state->crtcs[i].crtc;
		GnomeRRCrtc *crtc = gnome_rr_screen_get_crtc_by_id(state->screen,
				crtc_id);
		unsigned int ramp_size = state->crtcs[i].ramp_size;
		unsigned short *gamma_r = &state->crtcs[i].saved_ramps[0*ramp_size];
		unsigned short *gamma_g = &state->crtcs[i].saved_ramps[1*ramp_size];
		unsigned short *gamma_b = &state->crtcs[i].saved_ramps[2*ramp_size];

		/* Set gamma ramps */
		if (!gnome_rr_crtc_set_gamma(crtc, ramp_size, gamma_r, gamma_g,
					     gamma_b)) {
			g_printerr(_("`%s' failed for crtc id %d\n"),
				"GNOMERR Set CRTC Gamma", crtc_id);
			g_printerr(_("Unable to restore CRTC %i\n"), i);
		}
	}
}

static void
gnomerr_free(gnomerr_state_t *state)
{
	/* Free CRTC state */
	for (int i = 0; i < state->crtc_count; i++) {
		free(state->crtcs[i].saved_ramps);
	}
	free(state->crtcs);
	free(state->crtc_num);

	if (state->screen != NULL) {
		g_object_unref(state->screen);
		state->screen = NULL;
	}

	free(state);
}

static void
gnomerr_print_help(FILE *f)
{
	fputs(_("Adjust gamma ramps with the GnomeRR.\n"), f);
	fputs("\n", f);

	/* TRANSLATORS: GNOMERR help output
	   left column must not be translated */
	fputs(_("  crtc=N\tList of comma separated CRTCs to apply adjustments to\n"
		"  preserve={0,1}\tWhether existing gamma should be"
		" preserved\n"),
	      f);
	fputs("\n", f);
}

static int
gnomerr_set_option(gnomerr_state_t *state, const char *key, const char *value)
{
	if (strcasecmp(key, "crtc") == 0) {
		char *tail;

		/* Check how many crtcs are configured */
		const char *local_value = value;
		while (1) {
			errno = 0;
			guint32 parsed = strtoul(local_value, &tail, 0);
			if (parsed == 0 && (errno != 0 ||
					    tail == local_value)) {
				g_printerr(_("Unable to read screen"
						  " number: `%s'.\n"), value);
				return -1;
			} else {
				state->crtc_num_count += 1;
			}
			local_value = tail;

			if (*local_value == ',') {
				local_value += 1;
			} else if (*local_value == '\0') {
				break;
			}
		}

		/* Configure all given crtcs */
		state->crtc_num = calloc(state->crtc_num_count, sizeof(int));
		local_value = value;
		for (int i = 0; i < state->crtc_num_count; i++) {
			errno = 0;
			guint32 parsed = strtoul(local_value, &tail, 0);
			if (parsed == 0 && (errno != 0 ||
					    tail == local_value)) {
				return -1;
			} else {
				state->crtc_num[i] = parsed;
			}
			local_value = tail;

			if (*local_value == ',') {
				local_value += 1;
			} else if (*local_value == '\0') {
				break;
			}
		}
	} else if (strcasecmp(key, "preserve") == 0) {
		state->preserve = atoi(value);
	} else {
		g_printerr(_("Unknown method parameter: `%s'.\n"), key);
		return -1;
	}

	return 0;
}

static int
gnomerr_set_temperature_for_crtc(gnomerr_state_t *state, int crtc_num,
			       const color_setting_t *setting)
{
	 GError *error;

	if (crtc_num >= state->crtc_count || crtc_num < 0) {
		g_printerr(_("CRTC %d does not exist. "),
			crtc_num);
		if (state->crtc_count > 1) {
			g_printerr(_("Valid CRTCs are [0-%d].\n"),
				state->crtc_count-1);
		} else {
			g_printerr(_("Only CRTC 0 exists.\n"));
		}

		return -1;
	}

	if (state->crtcs[crtc_num].is_gamma_valid == FALSE)
		return 0;

	guint32 crtc_id = state->crtcs[crtc_num].crtc;
	GnomeRRCrtc *crtc = gnome_rr_screen_get_crtc_by_id(state->screen,
							   crtc_id);
	unsigned int ramp_size = state->crtcs[crtc_num].ramp_size;

	/* Create new gamma ramps */
	unsigned short *gamma_ramps = malloc(3*ramp_size*sizeof(unsigned short));
	if (gamma_ramps == NULL) {
		perror("malloc");
		return -1;
	}

	unsigned short *gamma_r = &gamma_ramps[0*ramp_size];
	unsigned short *gamma_g = &gamma_ramps[1*ramp_size];
	unsigned short *gamma_b = &gamma_ramps[2*ramp_size];

	if (state->preserve) {
		/* Initialize gamma ramps from saved state */
		memcpy(gamma_ramps, state->crtcs[crtc_num].saved_ramps,
		       3*ramp_size*sizeof(uint16_t));
	} else {
		/* Initialize gamma ramps to pure state */
		for (int i = 0; i < ramp_size; i++) {
			unsigned short value = (double)i/ramp_size * (UINT16_MAX+1);
			gamma_r[i] = value;
			gamma_g[i] = value;
			gamma_b[i] = value;
		}
	}

	colorramp_fill(gamma_r, gamma_g, gamma_b, ramp_size,
		       setting);

	/* Set new gamma ramps */
	if (!gnome_rr_crtc_set_gamma(crtc, ramp_size, gamma_r, gamma_g,
				     gamma_b)) {
		g_printerr(_("`%s' failed for crtc id %d\n"),
			"GNOMERR Set CRTC Gamma", crtc_id);
		free(gamma_ramps);
		return -1;
	}

	free(gamma_ramps);

	return 0;
}

static int
gnomerr_set_temperature(gnomerr_state_t *state,
		      const color_setting_t *setting)
{
	int r;

	/* If no CRTC numbers have been specified,
	   set temperature on all CRTCs. */
	if (state->crtc_num_count == 0) {
		for (int i = 0; i < state->crtc_count; i++) {
			r = gnomerr_set_temperature_for_crtc(state, i,
							   setting);
			if (r < 0) return -1;
		}
	} else {
		for (int i = 0; i < state->crtc_num_count; ++i) {
			r = gnomerr_set_temperature_for_crtc(
				state, state->crtc_num[i], setting);
			if (r < 0) return -1;
		}
	}

	return 0;
}


const gamma_method_t gnomerr_gamma_method = {
	"gnomerr", 0,
	(gamma_method_init_func *)gnomerr_init,
	(gamma_method_start_func *)gnomerr_start,
	(gamma_method_free_func *)gnomerr_free,
	(gamma_method_print_help_func *)gnomerr_print_help,
	(gamma_method_set_option_func *)gnomerr_set_option,
	(gamma_method_restore_func *)gnomerr_restore,
	(gamma_method_set_temperature_func *)gnomerr_set_temperature
};
