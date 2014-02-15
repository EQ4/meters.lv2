/* dr14 meter LV2 GUI
 *
 * Copyright 2012-2014 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "src/uris.h"

#define MTR_URI "http://gareus.org/oss/lv2/meters#"
#define MTR_GUI "dr14meterui"

#define GM_MARGIN_X 5.f
#define GM_MARGIN_Y 5.f

#define GM_GIRTH  20.0f
#define GM_RANGE (450.0f)

#define GM_WIDTH  (GM_GIRTH + GM_MARGIN_X + GM_MARGIN_X)
#define GM_HEIGHT (GM_RANGE + GM_MARGIN_Y + GM_MARGIN_Y)

#define MA_WIDTH  ( 20.0f)

#define AN_WIDTH  (100.0f)
#define AN_HEIGHT (ui->num_meters * 80 + 100)

#define MAX_METERS 2

enum {
	DR_CONTROL = 0,
	DR_HOST_TRANSPORT,
	DR_RESET,
	DR_BLKCNT,

	DR_V_PEAK0 = 6,
	DR_M_PEAK0,
	DR_V_RMS0,
	DR_M_RMS0,
	DR_DR0,

	DR_V_PEAK1 = 13,
	DR_M_PEAK1,
	DR_V_RMS1,
	DR_M_RMS1,
	DR_DR1,
	DR_TOTAL
};

typedef struct {
	RobWidget *rw;

	LV2UI_Write_Function write;
	LV2UI_Controller     controller;

	LV2_Atom_Forge forge;
  EBULV2URIs uris;

  RobWidget* m0;
  RobWidget* m1;
  RobWidget* ctlbox;

	RobTkPBtn* btn_reset;
	RobTkCBtn* btn_transport;
	RobTkSep* sep0;
	RobTkLbl* lbl0;
	bool disable_signals;

	float rms_v[MAX_METERS];
	float rms_p[MAX_METERS];
	float dbtp_v[MAX_METERS];
	float dbtp_p[MAX_METERS];

	float dr14_v[MAX_METERS];
	float dr14_t;
	float integration_time;

	/* on screen pixel values, old/cur:0, new:1 */
	int px_rms_v[MAX_METERS][2];
	int px_rms_p[MAX_METERS][2];
	int px_dbtp_v[MAX_METERS][2];
	int px_dbtp_p[MAX_METERS][2];

#if 0
	cairo_surface_t* sf[MAX_METERS];
#endif
	cairo_surface_t* ma[2]; // meter annotations/scale/ticks left&right
	cairo_pattern_t* mpat;
	cairo_pattern_t* rpat;
	cairo_pattern_t* spat;
	PangoFontDescription *font[3];

	uint32_t num_meters;

	int width;
	int height;

	float c_txt[4];
	float c_bgr[4];
} DRUI;

/******************************************************************************
 * Drawing
 */

static int deflect(float val) {
	int lvl = rintf(GM_RANGE * (val + 70.f) / 73.f);
	if (lvl < 0) lvl = 0;
	if (lvl >= GM_RANGE) lvl = GM_RANGE;
	return lvl;
}

static void create_meter_pattern(DRUI* ui) {
	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, 0.0, GM_RANGE);
	cairo_pattern_add_color_stop_rgb (pat, .0, .0 , .0, .0);

#define PSTOP(DB, R, G, B) \
	cairo_pattern_add_color_stop_rgb (pat, 1.f - deflect(DB) / GM_RANGE, R, G, B);

	PSTOP(-70.0, 0.0, 0.2, 0.5)
	PSTOP(-65.0, 0.0, 0.5, 0.2)
	PSTOP(-18.0, 0.0, 0.7, 0.0)
	PSTOP(-14.0, 0.2, 0.7, 0.0)
	PSTOP( -8.3, 0.5, 0.7, 0.0)
	PSTOP( -8.0, 0.7, 0.7, 0.0)
	PSTOP( -1.3, 0.7, 0.7, 0.0)
	PSTOP( -1.0, 0.8, 0.5, 0.0)
	PSTOP( -0.3, 0.8, 0.5, 0.0)
	PSTOP(  0.0, 1.0, 0.0, 0.0)
	PSTOP(  6.0, 1.0, 0.0, 0.0)


	{
		cairo_pattern_t* shade_pattern = cairo_pattern_create_linear (0.0, 0.0, GM_GIRTH, 0.0);
		cairo_pattern_add_color_stop_rgba (shade_pattern,  .0, 0.0, 0.0, 0.0, 0.90);
		cairo_pattern_add_color_stop_rgba (shade_pattern, .26, 0.0, 0.0, 0.0, 0.55);
		cairo_pattern_add_color_stop_rgba (shade_pattern, .40, 1.0, 1.0, 1.0, 0.12);
		cairo_pattern_add_color_stop_rgba (shade_pattern, .53, 0.0, 0.0, 0.0, 0.05);
		cairo_pattern_add_color_stop_rgba (shade_pattern, .74, 0.0, 0.0, 0.0, 0.55);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 0.0, 0.0, 0.0, 0.90);

		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GM_GIRTH, GM_RANGE);
		tc = cairo_create (surface);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, GM_GIRTH, GM_RANGE);
		cairo_fill (tc);

		cairo_set_source (tc, shade_pattern);
		cairo_rectangle (tc, 0, 0, GM_GIRTH, GM_RANGE);
		cairo_fill (tc);
		cairo_pattern_destroy (shade_pattern);

		ui->mpat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}
	{
		cairo_pattern_t* shade_pattern = cairo_pattern_create_linear (0.0, 0.0, GM_GIRTH, 0.0);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 0.0, 0.0, 0.0, 0.0, 0.05);
		cairo_pattern_add_color_stop_rgba (shade_pattern, 1.0, 1.0, 1.0, 1.0, 0.08);

		cairo_surface_t* surface;
		cairo_t* tc = 0;
		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GM_GIRTH, GM_RANGE);
		tc = cairo_create (surface);
		cairo_set_source (tc, pat);
		cairo_rectangle (tc, 0, 0, GM_GIRTH, GM_RANGE);
		cairo_fill (tc);

		cairo_set_source (tc, shade_pattern);
		cairo_rectangle (tc, 0, 0, GM_GIRTH, GM_RANGE);
		cairo_fill (tc);
		ui->spat = shade_pattern;
		ui->rpat = cairo_pattern_create_for_surface (surface);
		cairo_destroy (tc);
		cairo_surface_destroy (surface);
	}

	cairo_pattern_destroy (pat);
}

static void create_surfaces(DRUI* ui) {
	cairo_t* cr;

#if 0
	for (uint32_t i=0; i < ui->num_meters ; ++i) {
		ui->sf[i] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GM_GIRTH, GM_RANGE);
		cr = cairo_create (ui->sf[i]);
		cairo_rectangle (cr, 0, 0, GM_GIRTH, GM_RANGE);
		cairo_set_source_rgba(cr, 0, 0, 0, 1);
		cairo_fill (cr);
		cairo_destroy (cr);
	}
#endif

	PangoFontDescription *font = pango_font_description_from_string("Mono 7");

	// MA_WIDTH
#define TICK(DB, LR) { \
	const int y0 = GM_MARGIN_Y + GM_RANGE - deflect(DB); \
	char txt[8]; snprintf(txt, 8, "%+.0f", DB); \
	cairo_move_to(cr, LR ? 0.5 : 19.5, y0 + .5); \
	cairo_close_path(cr); \
	cairo_stroke(cr); \
	write_text_full(cr, txt, font, LR ? 20. : 16., y0, 0, 1, ui->c_txt); \
	}

#define XTICKS(LR) \
	CairoSetSouerceRGBA(ui->c_txt); \
	cairo_set_line_width(cr, 1.5);  \
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); \
	TICK(  3.f, LR) \
	TICK(  0.f, LR) \
	TICK( -3.f, LR) \
	TICK( -8.f, LR) \
	TICK(-14.f, LR) \
	TICK(-18.f, LR) \
	TICK(-20.f, LR) \
	TICK(-30.f, LR) \
	TICK(-40.f, LR) \
	TICK(-50.f, LR) \
	TICK(-60.f, LR) \
	cairo_set_line_width(cr, 1.0);  \
	for (int i = -69; i < 3; ++i) { \
		const int y0 = GM_MARGIN_Y + GM_RANGE - deflect(i); \
		cairo_move_to(cr, LR ? 0.5 : 19.5, y0 + .5); \
		cairo_close_path(cr); \
		cairo_stroke(cr); \
	} \
	write_text_full(cr, LR ? "-\u221E " : "-\u221E", font, LR ? 20. : 16., GM_MARGIN_Y + GM_RANGE -2, 0, 1, ui->c_txt);

	ui->ma[0] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, MA_WIDTH, GM_HEIGHT);
	cr = cairo_create (ui->ma[0]);
	cairo_rectangle (cr, 0, 0, MA_WIDTH, GM_HEIGHT);
	CairoSetSouerceRGBA(ui->c_bgr);
	cairo_fill (cr);
	XTICKS(false);
	cairo_destroy (cr);

	ui->ma[1] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, MA_WIDTH, GM_HEIGHT);
	cr = cairo_create (ui->ma[1]);
	cairo_rectangle (cr, 0, 0, MA_WIDTH, GM_HEIGHT);
	CairoSetSouerceRGBA(ui->c_bgr);
	cairo_fill (cr);
	XTICKS(true);
	cairo_destroy (cr);
}

/******************************************************************************
 * main drawing
 */

static bool m0_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	DRUI* ui = (DRUI*)GET_HANDLE(handle);

	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	/* metric areas */
	cairo_set_source_surface(cr, ui->ma[0], 0, 0);
	cairo_paint (cr);
	cairo_set_source_surface(cr, ui->ma[1], MA_WIDTH + GM_WIDTH * ui->num_meters, 0);
	cairo_paint (cr);

	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

	for (uint32_t i = 0; i < ui->num_meters ; ++i) {
		const float x0 = MA_WIDTH + GM_WIDTH * i;
		const float xm = x0 + GM_MARGIN_X;

		if (!rect_intersect_a(ev, x0, 0, GM_WIDTH, GM_HEIGHT)) continue;

		cairo_save(cr);

		/* background */
		cairo_rectangle (cr, x0, 0, GM_WIDTH, GM_HEIGHT);
		CairoSetSouerceRGBA(ui->c_bgr);
		cairo_fill (cr);

#ifdef DRAW_BOUNDS
		rounded_rectangle (cr, xm-1.5, GM_MARGIN_Y-0.5, GM_GIRTH+3, GM_RANGE+1, 6);
#else
		rounded_rectangle (cr, xm-.5, GM_MARGIN_Y-0.5, GM_GIRTH+1, GM_RANGE+1, 6);
#endif
		CairoSetSouerceRGBA(c_gry);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke (cr);

#ifdef DRAW_BOUNDS
		rounded_rectangle (cr, xm-1, GM_MARGIN_Y, GM_GIRTH+2, GM_RANGE, 6);
#else
		rounded_rectangle (cr, xm, GM_MARGIN_Y, GM_GIRTH, GM_RANGE, 6);
#endif
		cairo_set_source_rgba(cr, 0, 0, 0, 1);
		cairo_fill_preserve(cr);
		cairo_clip(cr);

		cairo_translate(cr, xm, GM_MARGIN_Y);

		// TODO partial redraw, px_* range only,
		// draw on surface, blit here

		const int rms_v  = ui->px_rms_v[i][0] = ui->px_rms_v[i][1];
		const int rms_p  = ui->px_rms_p[i][0] = ui->px_rms_p[i][1];
		const int dbtp_v = ui->px_dbtp_v[i][0] = ui->px_dbtp_v[i][1];
		const int dbtp_p = ui->px_dbtp_p[i][0] = ui->px_dbtp_p[i][1];
		const int peak_p = deflect(ui->rms_p[i] + ui->dr14_v[i]);

#define MTRYOFF(Y) (GM_RANGE - (Y))

		if (ui->rms_v[i] > -80) {
			cairo_rectangle (cr, 0, MTRYOFF(rms_v), GM_GIRTH, rms_v);
			cairo_set_source(cr, ui->rpat);
			cairo_fill_preserve(cr);
			cairo_set_source_rgba(cr, .0, .0, .0, 0.5);
			cairo_fill(cr);
		}

		if (ui->rms_p[i] > -80 && ui->dr14_v[i] < 21) {

			if (ui->dr14_v[i] < 7.5) {
				cairo_set_source_rgba(cr, .9, .3, .3, 0.33);
			} else if (ui->dr14_v[i] < 13.5) {
				cairo_set_source_rgba(cr, .9, .9, .0, 0.33);
			} else {
				cairo_set_source_rgba(cr, .3, .9, .3, 0.33);
			}
			rounded_rectangle (cr, 0, MTRYOFF(peak_p), GM_GIRTH, peak_p- rms_p, 6);
			cairo_fill_preserve(cr);
			cairo_set_source(cr, ui->spat);
			cairo_fill(cr);
#ifdef DRAW_BOUNDS
			/* DR in on the side */
			cairo_set_line_width(cr, 1.0);
			cairo_set_source_rgba(cr, .5, .5, .9, 1.0);
			cairo_move_to(cr, GM_GIRTH + .5, MTRYOFF(peak_p) + 0.5);
			cairo_line_to(cr, GM_GIRTH + .5, MTRYOFF(rms_p) + 0.5);
			cairo_stroke(cr);
			cairo_move_to(cr, -.5 , MTRYOFF(peak_p) + 0.5);
			cairo_line_to(cr, -.5 , MTRYOFF(rms_p) + 0.5);
			cairo_stroke(cr);
#endif
		}

		if (ui->dbtp_v[i] > -80) {
			cairo_set_source(cr, ui->mpat);
			rounded_rectangle (cr, GM_GIRTH / 4.0, MTRYOFF(dbtp_v), GM_GIRTH / 2.0, dbtp_v, 3);
			cairo_fill(cr);
		}

#ifdef DRAW_BOUNDS
		if (ui->rms_p[i] > -80) {
			cairo_set_line_width(cr, 1.0);
			cairo_move_to(cr, 0, MTRYOFF(rms_p) + 0.5);
			cairo_line_to(cr, GM_GIRTH, MTRYOFF(rms_p) + 0.5);
			cairo_set_source(cr, ui->mpat);
			cairo_stroke_preserve(cr);
			cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
			cairo_stroke(cr);
		}

		if (ui->dr14_v[i] < 21) {
			cairo_set_line_width(cr, 1.0);
			cairo_move_to(cr, 0, MTRYOFF(peak_p) + 0.5);
			cairo_line_to(cr, GM_GIRTH, MTRYOFF(peak_p) + 0.5);
			cairo_set_source(cr, ui->mpat);
			cairo_stroke_preserve(cr);
			cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
			cairo_stroke(cr);
		}
#endif

		if (ui->dbtp_p[i] > -80) {
			cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
#ifdef DRAW_BOUNDS
			const double dashed[] = {2.0, 2.0};
			cairo_set_dash(cr, dashed, 2, 3);
			cairo_set_line_width(cr, 1.5);
#else
			cairo_set_line_width(cr, 1.0);
#endif
			cairo_move_to(cr, 0, MTRYOFF(dbtp_p) + 0.5);
			cairo_line_to(cr, GM_GIRTH, MTRYOFF(dbtp_p) + 0.5);
			cairo_set_source(cr, ui->mpat);
			cairo_stroke_preserve(cr);
			cairo_set_source_rgba(cr, 1, .7, .5, 0.3);
			cairo_stroke(cr);
		}

		cairo_restore(cr);
	}

	return TRUE;
}

static bool m1_expose_event(RobWidget* handle, cairo_t* cr, cairo_rectangle_t *ev) {
	DRUI* ui = (DRUI*)GET_HANDLE(handle);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip (cr);

	cairo_rectangle (cr, 0, 0, AN_WIDTH, AN_HEIGHT);
	CairoSetSouerceRGBA(ui->c_bgr);
	cairo_fill (cr);

	rounded_rectangle (cr, 2, 2, AN_WIDTH-4, AN_HEIGHT-4, C_RAD);
	CairoSetSouerceRGBA(c_blk);
	cairo_fill(cr);
	rounded_rectangle (cr, 2.5, 2.5, AN_WIDTH-5, AN_HEIGHT-5, C_RAD);
	CairoSetSouerceRGBA(c_gry);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	char txt[32];

#if 0
	snprintf(txt, 32, "%.0f", ui->dr);
	write_text_full(cr, txt, ui->font[1], AN_WIDTH/2, 20, 0, 2, ui->c_txt);
#endif

	for (uint32_t i = 0; i < ui->num_meters ; ++i) {
		if (ui->dbtp_p[i] > -80)
			snprintf(txt, 32, "P:%+6.2f", ui->dbtp_p[i]);
		else
			snprintf(txt, 32, "P: ---- ");

		write_text_full(cr, txt, ui->font[1], AN_WIDTH/2, 35 + 80*i, 0, 2, c_wht);

		if (ui->rms_p[i] > -80)
			snprintf(txt, 32, "R:%+6.2f", ui->rms_p[i]);
		else
			snprintf(txt, 32, "R: ---- ");
		write_text_full(cr, txt, ui->font[1], AN_WIDTH/2, 55 + 80*i, 0, 2, c_wht);

		if (ui->dr14_v[i] < 21)
			snprintf(txt, 32, "DR%6.2f", ui->dr14_v[i]);
		else
			snprintf(txt, 32, "DR ---- ");
		write_text_full(cr, txt, ui->font[1], AN_WIDTH/2, 75 + 80*i, 0, 2, c_wht);
	}

	if (ui->num_meters == 2) {
		write_text_full(cr, "Left",  ui->font[2], AN_WIDTH/2, 20, 0, 2, c_wht);
		write_text_full(cr, "Right", ui->font[2], AN_WIDTH/2, 20 + 80, 0, 2, c_wht);
	}

	float dr;
	if (ui->num_meters > 1) dr = ui->dr14_t;
	else dr = ui->dr14_v[0];

	const float y0 = ui->num_meters * 80 + 30;
	if (dr < 21) {
		write_text_full(cr, "DR", ui->font[1], AN_WIDTH/2, y0, 0, 2, c_wht);
		snprintf(txt, 32, "%.0f", rintf(dr));
		float const * cl = c_grn;
		if (dr < 7.5) cl = c_red;
		else if (dr < 13.5) cl = c_nyl;
		write_text_full(cr, txt, ui->font[0], AN_WIDTH/2, y0+50, 0, 5, cl);
	}

	if (ui->integration_time > 0) {
		if (ui->integration_time < 60) {
			snprintf(txt, 32, "(%02d sec)",
					(int)floorf(ui->integration_time));
		} else if (ui->integration_time < 3600) {
			snprintf(txt, 32, "(%02d'%02d\")",
					(int)floorf(ui->integration_time/60) % 60,
					(int)floorf(ui->integration_time) % 60);
		} else {
			snprintf(txt, 32, "(%dh%02d'%02d\")",
					(int)floorf(ui->integration_time/3600),
					(int)floorf(ui->integration_time/60) % 60,
					(int)floorf(ui->integration_time) % 60);
		}
		write_text_full(cr, txt, ui->font[2], AN_WIDTH/2, y0 + 55, 0, 2, c_wht);
	}

	return TRUE;
}

/******************************************************************************
 * UI callbacks
 */
static bool cb_set_transport (RobWidget* handle, void *data) {
	DRUI* ui = (DRUI*) (data);
	if (ui->disable_signals) return TRUE;
	float val = robtk_cbtn_get_active(ui->btn_transport) ? 1.0 : 0.0;
	ui->write(ui->controller, DR_HOST_TRANSPORT, sizeof(float), 0, (const void*) &val);
	return TRUE;
}

static bool cb_reset (RobWidget* handle, void *data) {
	DRUI* ui = (DRUI*) (data);
	// TODO -- forge message || toggle DR_RESET
	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&ui->forge, obj_buf, 128);
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time(&ui->forge, 0);
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_blank(&ui->forge, &frame, 1, ui->uris.mtr_dr14reset);
	lv2_atom_forge_pop(&ui->forge, &frame);
	ui->write(ui->controller, 0, lv2_atom_total_size(msg), ui->uris.atom_eventTransfer, msg);
	return TRUE;
}

/******************************************************************************
 * widget hackery
 */

static void
m0_size_request(RobWidget* handle, int *w, int *h) {
	DRUI* ui = (DRUI*)GET_HANDLE(handle);
	*w = ui->width;
	*h = ui->height;
}

static void
m1_size_request(RobWidget* handle, int *w, int *h) {
	DRUI* ui = (DRUI*)GET_HANDLE(handle);
	*w = AN_WIDTH;
	*h = AN_HEIGHT;
}


static RobWidget * toplevel(DRUI* ui, void * const top)
{
	/* main widget: layout */
	ui->rw = rob_hbox_new(FALSE, 2);
	robwidget_make_toplevel(ui->rw, top);

	/* DPM main drawing area */
	ui->m0 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m0, "dr14 (m0)");
	robwidget_set_expose_event(ui->m0, m0_expose_event);
	robwidget_set_size_request(ui->m0, m0_size_request);

	ui->m1 = robwidget_new(ui);
	ROBWIDGET_SETNAME(ui->m1, "dr14 (m1)");
	robwidget_set_expose_event(ui->m1, m1_expose_event);
	robwidget_set_size_request(ui->m1, m1_size_request);

	ui->ctlbox = rob_vbox_new(FALSE, 0);

	ui->btn_transport = robtk_cbtn_new("Use Host\nTransport", GBT_LED_LEFT, false);
	robtk_cbtn_set_active(ui->btn_transport, false);
	robtk_cbtn_set_callback(ui->btn_transport, cb_set_transport, ui);

	ui->btn_reset = robtk_pbtn_new("Reset");
	robtk_pbtn_set_alignment(ui->btn_reset, .5, .5);
	robtk_pbtn_set_callback(ui->btn_reset, cb_reset, ui);

	ui->sep0 = robtk_sep_new(false);
	robtk_sep_set_linewidth(ui->sep0, 0);

	ui->lbl0 = robtk_lbl_new(" P: dBTP (max)\n R: Top20 RMS/3s\n"
#if 1
			"\nNote:\n The DR value is\n not suitable for\n any professional\n work. Prefer the\n EBU-R128 meter.\n"
#endif
			);

	rob_vbox_child_pack(ui->ctlbox, ui->m1, FALSE, FALSE);
	rob_vbox_child_pack(ui->ctlbox, robtk_lbl_widget(ui->lbl0), FALSE, FALSE);
	rob_vbox_child_pack(ui->ctlbox, robtk_sep_widget(ui->sep0), TRUE, FALSE);
	rob_vbox_child_pack(ui->ctlbox, robtk_cbtn_widget(ui->btn_transport), FALSE, FALSE);
	rob_vbox_child_pack(ui->ctlbox, robtk_pbtn_widget(ui->btn_reset), FALSE, FALSE);

	rob_hbox_child_pack(ui->rw, ui->m0, FALSE, FALSE);
	rob_hbox_child_pack(ui->rw, ui->ctlbox, FALSE, FALSE);
	return ui->rw;
}

/******************************************************************************
 * LV2 callbacks
 */

static void ui_enable(LV2UI_Handle handle) { }
static void ui_disable(LV2UI_Handle handle) { }

static LV2UI_Handle
instantiate(
		void* const               ui_toplevel,
		const LV2UI_Descriptor*   descriptor,
		const char*               plugin_uri,
		const char*               bundle_path,
		LV2UI_Write_Function      write_function,
		LV2UI_Controller          controller,
		RobWidget**               widget,
		const LV2_Feature* const* features)
{
	DRUI* ui = (DRUI*) calloc(1,sizeof(DRUI));
	*widget = NULL;

	LV2_URID_Map* map = NULL;
	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!map) {
		fprintf(stderr, "UI: Host does not support urid:map\n");
		free(ui);
		return NULL;
	}

	if      (!strncmp(plugin_uri, MTR_URI "dr14mono", 33 + 8))    { ui->num_meters = 1;}
	else if (!strncmp(plugin_uri, MTR_URI "dr14stereo", 33 + 10)) { ui->num_meters = 2;}
	else {
		free(ui);
		return NULL;
	}
	ui->write      = write_function;
	ui->controller = controller;

	ui->disable_signals = false;
	map_eburlv2_uris(map, &ui->uris);
	lv2_atom_forge_init(&ui->forge, map);

	get_color_from_theme(0, ui->c_txt);
	get_color_from_theme(1, ui->c_bgr);

#if 0
	ui->c_bgr[0] = 84/255.0;
	ui->c_bgr[1] = 85/255.0;
	ui->c_bgr[2] = 93/255.0;
	ui->c_bgr[3] = 1.0;
#endif

	for (uint32_t i=0; i < ui->num_meters ; ++i) {
		ui->rms_v[i]  = ui->rms_p[i]  = -81.0;
		ui->dbtp_v[i] = ui->dbtp_p[i] = -81.0;
		ui->px_rms_v[i][0]  = ui->px_rms_v[i][1]  = 0;
		ui->px_rms_p[i][0]  = ui->px_rms_p[i][1]  = 0;
		ui->px_dbtp_v[i][0] = ui->px_dbtp_v[i][1] = 0;
		ui->px_dbtp_p[i][0] = ui->px_dbtp_p[i][1] = 0;
		ui->dr14_v[i] = 21;
	}
	ui->dr14_t = 21;

	ui->width = 2.0 * MA_WIDTH + ui->num_meters * GM_WIDTH;
	ui->height = GM_HEIGHT;

	create_meter_pattern(ui);
	create_surfaces(ui);
	ui->font[0] = pango_font_description_from_string("Mono 28");
	ui->font[1] = pango_font_description_from_string("Mono 11");
	ui->font[2] = pango_font_description_from_string("Sans 8");

	*widget = toplevel(ui, ui_toplevel);
	return ui;
}

static enum LVGLResize
plugin_scale_mode(LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static void
cleanup(LV2UI_Handle handle)
{
	DRUI* ui = (DRUI*)handle;
#if 0
	for (uint32_t i=0; i < ui->num_meters ; ++i) {
		cairo_surface_destroy(ui->sf[i]);
	}
#endif
	cairo_pattern_destroy(ui->mpat);
	cairo_pattern_destroy(ui->rpat);
	cairo_pattern_destroy(ui->spat);
	cairo_surface_destroy(ui->ma[0]);
	cairo_surface_destroy(ui->ma[1]);
	pango_font_description_free(ui->font[0]);
	pango_font_description_free(ui->font[1]);
	pango_font_description_free(ui->font[2]);

	robtk_pbtn_destroy(ui->btn_reset);
	robtk_cbtn_destroy(ui->btn_transport);
	robtk_sep_destroy(ui->sep0);
	robtk_lbl_destroy(ui->lbl0);

	robwidget_destroy(ui->m1);
	robwidget_destroy(ui->m0);
	rob_box_destroy(ui->ctlbox);
	rob_box_destroy(ui->rw);
	free(ui);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

/******************************************************************************
 * backend communication
 */

#define INVALIDATE_RECT(XX,YY,WW,HH) queue_tiny_area(ui->m0, XX, YY, WW, HH);
#define VCMP(A,B) (rintf(100.f * (A)) != rintf(100.f * (B)))

static void invalidate_meter(DRUI* ui, int mtr) {
	INVALIDATE_RECT(
			MA_WIDTH + GM_WIDTH * mtr, GM_MARGIN_Y,
			GM_WIDTH, GM_RANGE+1);
}

static void invalidate_dbtp_v(DRUI* ui, int mtr, float val) {
	int px = deflect(val);
	if (ui->px_dbtp_v[mtr][0] != px) invalidate_meter(ui, mtr);
	ui->px_dbtp_v[mtr][1] = px;
	ui->dbtp_v[mtr] = val;
}

static void invalidate_dbtp_p(DRUI* ui, int mtr, float val) {
	int px = deflect(val);
	if (ui->px_dbtp_p[mtr][0] != px) invalidate_meter(ui, mtr);
	if (VCMP(ui->dbtp_p[mtr], val)) queue_draw(ui->m1);
	ui->px_dbtp_p[mtr][1] = px;
	ui->dbtp_p[mtr] = val;
}

static void invalidate_rms_v(DRUI* ui, int mtr, float val) {
	int px = deflect(val);
	if (ui->px_rms_v[mtr][0] != px) invalidate_meter(ui, mtr);
	ui->px_rms_v[mtr][1] = px;
	ui->rms_v[mtr] = val;
}

static void invalidate_rms_p(DRUI* ui, int mtr, float val) {
	int px = deflect(val);
	if (ui->px_rms_p[mtr][0] != px) invalidate_meter(ui, mtr);
	if (VCMP(ui->rms_p[mtr], val)) queue_draw(ui->m1);
	ui->px_rms_p[mtr][1] = px;
	ui->rms_p[mtr] = val;
}

/******************************************************************************
 * handle data from backend
 */
static inline void handle_meter_value(DRUI* ui, uint32_t port_index, float v) {
	if (port_index == DR_V_PEAK0) {
		invalidate_dbtp_v(ui, 0, v);
	}
	else if (port_index == DR_M_PEAK0) {
		invalidate_dbtp_p(ui, 0, v);
	}
	else if (port_index == DR_V_RMS0) {
		invalidate_rms_v(ui, 0, v);
	}
	else if (port_index == DR_M_RMS0) {
		invalidate_rms_p(ui, 0, v);
	}
	else if (port_index == DR_V_PEAK1 && ui->num_meters == 2) {
		invalidate_dbtp_v(ui, 1, v);
	}
	else if (port_index == DR_M_PEAK1 && ui->num_meters == 2) {
		invalidate_dbtp_p(ui, 1, v);
	}
	else if (port_index == DR_V_RMS1 && ui->num_meters == 2) {
		invalidate_rms_v(ui, 1, v);
	}
	else if (port_index == DR_M_RMS1 && ui->num_meters == 2) {
		invalidate_rms_p(ui, 1, v);
	}
}

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer)
{
	DRUI* ui = (DRUI*)handle;
	if (format != 0) return;

	switch(port_index) {
		case DR_BLKCNT:
			if (ui->integration_time != (*(float *)buffer)) queue_draw(ui->m1);
			ui->integration_time = (*(float *)buffer);
			break;
		case DR_HOST_TRANSPORT:
			ui->disable_signals = true;
			robtk_cbtn_set_active(ui->btn_transport, (*(float *)buffer) != 0);
			ui->disable_signals = false;
			break;
		case DR_TOTAL:
			if (VCMP(ui->dr14_t, *(float *)buffer)) queue_draw(ui->m1);
			ui->dr14_t = (*(float *)buffer);
			break;
		case DR_DR0:
			if (VCMP(ui->dr14_v[0], *(float *)buffer)) queue_draw(ui->m1);
			ui->dr14_v[0] = (*(float *)buffer);
			break;
		case DR_DR1:
			if (VCMP(ui->dr14_v[1], *(float *)buffer)) queue_draw(ui->m1);
			ui->dr14_v[1] = (*(float *)buffer);
			break;
		default:
			handle_meter_value(ui, port_index, *(float *)buffer);
			break;
	}
}