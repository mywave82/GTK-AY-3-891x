#include <cairo.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "pulse.h"
#include "aylet/driver.h"
#include "aylet/sound.h"

const int voice_index[3] = {0, 1, 2};

GtkWidget *FREQ[3]; //
GtkWidget *NOISE;

GtkWidget *ENNOISE[3];
GtkWidget *ENTONE[3];

GtkWidget *VOLUME[3];

GtkWidget *EnvelopePeriod;
GtkWidget *EnvelopeShape;

GtkWidget *graph;

GtkListStore *EnvelopeShapeModel;
GtkListStore *VolumeModel;
const int envelope_shape_lookup[] = {0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

#define SAMPLES_N 512
int16_t samples[SAMPLES_N];

int clockpulses = 0;

static void write_reg(uint8_t reg, uint8_t value)
{
	fprintf (stderr, "[%02x] = 0x%02x\n", reg, value);
	sound_ay_write (reg, value, clockpulses++);
}

static void freq_value_changed (GtkSpinButton *spin_button,
                                gpointer       user_data)
{
	int *voice = user_data;
	int value;

	value = gtk_spin_button_get_value_as_int (spin_button);

	write_reg(*voice * 2 + 0, value & 0xff);
	write_reg(*voice * 2 + 1, (value >> 8) & 0x0f);
}

static void noise_value_changed (GtkSpinButton *spin_button,
                                 gpointer       user_data)
{
	int value;

	value = gtk_spin_button_get_value_as_int (spin_button);

	write_reg(0x06, value & 0x1f);
}

static void voice_enabled_changed (GtkToggleButton *toggle_button,
                                   gpointer         user_data)
{
	uint8_t value = 0;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(ENTONE[0])))     value |= 0x01;
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(ENTONE[1])))     value |= 0x02;
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(ENTONE[2])))     value |= 0x04;
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(ENNOISE[0])))    value |= 0x08;
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(ENNOISE[1])))    value |= 0x10;
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(ENNOISE[2])))    value |= 0x20;

	write_reg(0x07, value);
}

static void volume_changed (GtkComboBox *combo,
                            gpointer     user_data)
{
	int *voice = user_data;
	uint8_t value;

	value = gtk_combo_box_get_active (GTK_COMBO_BOX(VOLUME[*voice]));

	write_reg(*voice + 0x08, value);
}

static void envelope_period_changed (GtkSpinButton *spin_button,
                                     gpointer       user_data)
{
	int *voice = user_data;
	int value;

	value = gtk_spin_button_get_value_as_int (spin_button);

	write_reg(0x0b, value & 0xff);
	write_reg(0x0c, value >> 8);
}

static void envelope_shape_changed (GtkComboBox *combo,
                                    gpointer     user_data)
{
	uint8_t value = 0;

	value = envelope_shape_lookup[gtk_combo_box_get_active (combo)];

	write_reg(0x0d, value);
}

static gboolean graph_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	int i;

	//gdouble dx, dy;
	guint width, height;
	//GdkRectangle da;            /* GtkDrawingArea size */
	GtkAllocation clip;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	gtk_widget_get_clip (graph, &clip);

	/* Draw on a black background */
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_paint (cr);

	/* Change the transformation matrix */
	cairo_scale (cr, (gdouble)width/SAMPLES_N, ((gdouble)height)/-(256+5.0));
	cairo_translate (cr, 0, -128.0+2.5);

	/* Determine the data points to calculate (ie. those in the clipping zone */
	//cairo_device_to_user_distance (cr, &dx, &dy);
	//cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);

	//fprintf (stderr, "linewidth: %lf (%lf)\n", dx, dy);
	cairo_set_line_width (cr, 2.0);

	cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);

	cairo_move_to (cr, 0.0, (gdouble)samples[0]/SAMPLES_N);
	for (i=0; i < SAMPLES_N; i++)
	{
		cairo_line_to (cr, i, (gdouble)samples[i]/SAMPLES_N);
	}

	cairo_stroke (cr);

	return FALSE;
}

int inputlength = 0;
int16_t *input;

int16_t *lastbuf;
int lastbuflen;

void ay_driver_frame(int16_t *buf, int buflen)
{
	lastbuf = buf;
	lastbuflen = buflen / sizeof(int16_t) / 6;
}

void mainwindow_getsound (void *data, size_t bytes)
{
	int i;
	int16_t *output = data;
	size_t size;

	int count = bytes/4;

	while (count)
	{
		if (!lastbuflen)
		{
			struct ay_driver_frame_state_t states;
			int smallestvalue=65536;
			int smallestat = 0;
			int prev = 65536;

			sound_frame (&states);
			clockpulses = 0;

			for (i=0; i < lastbuflen - SAMPLES_N; i++)
			{
				int temp = (lastbuf[i*6 + 0] + lastbuf[i*6 + 1] + lastbuf[i*6 + 2]);
				if (i && (prev != temp))
				{
					if (temp < smallestvalue)
					{
						smallestvalue = temp;
						smallestat = i;
					}
				}
				prev = temp;

				samples[i] = (lastbuf[i*6 + 0] + lastbuf[i*6 + 1] + lastbuf[i*6 + 2]);
			}

			for (i=0; i < SAMPLES_N; i++)
			{
				samples[i] = (lastbuf[(i+smallestat)*6 + 0] + lastbuf[(i+smallestat)*6 + 1] + lastbuf[(i+smallestat)*6 + 2]);
			}

			gtk_widget_queue_draw (graph);
		}

		output[0] = output[1] = ((int)lastbuf[0] + lastbuf[1] + lastbuf[2]) / 3;
		output+=2;
		lastbuf+=6;
		lastbuflen--;
		count--;
	}
}


static gboolean window_delete (GtkWidget *object,
                               gpointer   userpointer)
{
	sidpulse_done ();
	return FALSE;
}

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
	GtkWidget *window, *thegrid;
	int i;

	sidpulse_init ();

	sound_init ();

	sound_ay_reset ();

	EnvelopeShapeModel = gtk_list_store_new (1, G_TYPE_STRING);
	{
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "\\\\\\\\\\\\\\\\", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "\\_______", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "\\/\\/\\/\\/", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "\\-------", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "////////", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "/-------", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "/\\/\\/\\/\\", -1);
		gtk_list_store_append (GTK_LIST_STORE (EnvelopeShapeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (EnvelopeShapeModel), &iter, 0, "/_______", -1);
	}

	VolumeModel = gtk_list_store_new (1, G_TYPE_STRING);
	{
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "0", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "1", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "2", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "3", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "4", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "5", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "6", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "7", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "8", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "9", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "10", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "11", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "12", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "13", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "14", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "15", -1);
		gtk_list_store_append (GTK_LIST_STORE (VolumeModel), &iter); gtk_list_store_set (GTK_LIST_STORE (VolumeModel), &iter, 0, "Global Envelope", -1);
	}

	window = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (window), "Window");
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
	g_signal_connect (window, "delete-event", G_CALLBACK(window_delete), 0);

	thegrid = gtk_grid_new ();
	gtk_container_add (GTK_CONTAINER (window), thegrid);

	for (i=0; i < 3; i++)
	{
		GtkWidget *temp, *frame, *subgrid;
		char text[64];
		snprintf (text, sizeof (text), "Voice %d", i + 1);

		frame = gtk_frame_new (text);
		gtk_widget_set_margin_top (frame, 10.0);
		gtk_widget_set_margin_bottom (frame, 10.0);
		gtk_widget_set_margin_start (frame, 10.0);
		gtk_widget_set_margin_end (frame, 10.0);

		gtk_grid_attach (GTK_GRID (thegrid), frame, 0, i, 1, 1);

		subgrid = gtk_grid_new ();
		gtk_container_add (GTK_CONTAINER (frame), subgrid);
		
		temp = gtk_label_new ("Frequency"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 0, 1, 1);
		FREQ[i] = gtk_spin_button_new_with_range (0x0000, 0x0fff, 0x0001);
		gtk_grid_attach (GTK_GRID (subgrid), FREQ[i], 1, 0, 1, 1);
		g_signal_connect (FREQ[i], "value-changed", G_CALLBACK (freq_value_changed), (gpointer)(voice_index + i));

		ENTONE[i] = gtk_toggle_button_new_with_label ("Tone");
		gtk_grid_attach (GTK_GRID (subgrid), ENTONE[i], 2, 0, 1, 1);
		g_signal_connect (ENTONE[i], "toggled", G_CALLBACK (voice_enabled_changed), (gpointer)(voice_index + i));

		ENNOISE[i] = gtk_toggle_button_new_with_label ("Noise");
		gtk_grid_attach (GTK_GRID (subgrid), ENNOISE[i], 3, 0, 1, 1);
		g_signal_connect (ENNOISE[i], "toggled", G_CALLBACK (voice_enabled_changed), (gpointer)(voice_index + i));

		temp = gtk_label_new ("Volume"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 4, 0, 1, 1);
		VOLUME[i] = gtk_combo_box_new_with_model (GTK_TREE_MODEL (VolumeModel));
		gtk_grid_attach (GTK_GRID (subgrid), VOLUME[i], 5, 0, 1, 1);
		{
			GtkCellRenderer *renderer;
			GtkTreePath *path;
			GtkTreeIter iter;
			renderer = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (VOLUME[i]), renderer, TRUE);
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (VOLUME[i]), renderer, "text", 0, NULL);
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (VolumeModel), &iter, path);
			gtk_tree_path_free (path);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (VOLUME[i]), &iter);
		}
		g_signal_connect(VOLUME[i], "changed", G_CALLBACK(volume_changed), (gpointer)(voice_index + i));
	}

	{
		GtkWidget *temp, *frame, *subgrid;

		frame = gtk_frame_new ("Noise");
		gtk_widget_set_margin_top (frame, 10.0);
		gtk_widget_set_margin_bottom (frame, 10.0);
		gtk_widget_set_margin_start (frame, 10.0);
		gtk_widget_set_margin_end (frame, 10.0);

		gtk_grid_attach (GTK_GRID (thegrid), frame, 0, 3, 1, 1);

		subgrid = gtk_grid_new ();
		gtk_container_add (GTK_CONTAINER (frame), subgrid);
		
		temp = gtk_label_new ("Period"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 0, 1, 1);
		NOISE = gtk_spin_button_new_with_range (0x0000, 0x001f, 0x0001);
		gtk_grid_attach (GTK_GRID (subgrid), NOISE, 1, 0, 1, 1);
		g_signal_connect (NOISE, "value-changed", G_CALLBACK (noise_value_changed), 0);
	}


	{
		GtkWidget *temp, *frame, *subgrid;

		frame = gtk_frame_new ("Global Envelope");
		gtk_widget_set_margin_top (frame, 10.0);
		gtk_widget_set_margin_bottom (frame, 10.0);
		gtk_widget_set_margin_start (frame, 10.0);
		gtk_widget_set_margin_end (frame, 10.0);

		gtk_grid_attach (GTK_GRID (thegrid), frame, 0, 4, 1, 1);

		subgrid = gtk_grid_new ();
		gtk_container_add (GTK_CONTAINER (frame), subgrid);
		
		temp = gtk_label_new ("Period"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 0, 1, 1);
		EnvelopePeriod = gtk_spin_button_new_with_range (0x0000, 0xffff, 0x0001);
		gtk_grid_attach (GTK_GRID (subgrid), EnvelopePeriod, 1, 0, 1, 1);
		g_signal_connect (EnvelopePeriod, "value-changed", G_CALLBACK (envelope_period_changed), 0);

		temp = gtk_label_new ("Shape"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 2, 0, 1, 1);
		EnvelopeShape = gtk_combo_box_new_with_model (GTK_TREE_MODEL (EnvelopeShapeModel));
		gtk_grid_attach (GTK_GRID (subgrid), EnvelopeShape, 3, 0, 1, 1);
		{
			GtkCellRenderer *renderer;
			GtkTreePath *path;
			GtkTreeIter iter;
			renderer = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (EnvelopeShape), renderer, TRUE);
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (EnvelopeShape), renderer, "text", 0, NULL);
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (EnvelopeShapeModel), &iter, path);
			gtk_tree_path_free (path);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (EnvelopeShape), &iter);
		}
		g_signal_connect(EnvelopeShape, "changed", G_CALLBACK(envelope_shape_changed), 0);
	}

	graph = gtk_drawing_area_new ();
	gtk_grid_attach (GTK_GRID (thegrid), graph, 2, 0, 1, 3);
	g_signal_connect (graph, "draw", G_CALLBACK (graph_draw), NULL);

	gtk_widget_set_size_request (graph, 256, 256);
	
	gtk_widget_show_all (window);

	write_reg (0x07, 0x3f);

	for (i=0; i < 3; i++)
	{
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (FREQ[i]), 200);
		gtk_combo_box_set_active (GTK_COMBO_BOX(VOLUME[i]), i?0:6);
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX(EnvelopeShape), 2);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (EnvelopePeriod), 400);
}

int
main (int    argc,
      char **argv)
{
	GtkApplication *app;
	int status, i;

	app = gtk_application_new ("mywave.sidsynth", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	sound_end ();

	sidpulse_done ();

	return status;
}
