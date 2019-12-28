#include <pulse/stream.h>
#include <pulse/glib-mainloop.h>
#include <pulse/error.h>
#include <stdio.h>

#include "pulse.h"

pa_context *pa_ctx;
pa_stream *pas;
pa_glib_mainloop *paglib;


void stream_write_cb(pa_stream *stream, size_t length, void *userdata)
{
	void *data=0;

	if (pa_stream_begin_write(pas, &data, &length))
	{
		fprintf (stderr, "pa_stream_begin_write() failed\n");
		return;
	}

	mainwindow_getsound(data, length);

	if (pa_stream_write(pas, data, length, NULL, 0, PA_SEEK_RELATIVE) < 0)
	{
		fprintf (stderr, "sidpulse_write: pa_stream_write() failed\n");
	}
}

void start_stream (void)
{
	pa_buffer_attr bufattr;
	pa_sample_spec pa_c = {};

	fprintf (stderr, "sidpulse: start_stream\n");

	pa_c.channels = 2;
	pa_c.rate = 44100;
	pa_c.format = PA_SAMPLE_S16NE;

	bufattr.maxlength = (uint32_t)-1;
	bufattr.tlength = 44100/10;
	bufattr.prebuf = -1;
	bufattr.minreq = 2*44100/25;

	pas = pa_stream_new (pa_ctx,
	                     "SIDsynth",
        	             &pa_c,
                	     NULL);

	pa_stream_set_write_callback(pas, stream_write_cb, 0);

	if (pa_stream_connect_playback (pas, NULL, &bufattr, PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL))
	{
    		if (pa_stream_connect_playback (pas, NULL, 0, PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL))
		{
			fprintf (stderr, "sidpulse_prewrite: pa_stream_connect_playback() failed\n");

			pa_stream_unref (pas);
			pas = 0;
			return;
		}
	}
}

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void pa_state_cb(pa_context *c, void *userdata)
{
	pa_context_state_t state;
	state = pa_context_get_state(c);

	fprintf (stderr, "sidpulse: state=%d\n", state);

	switch  (state)
	{
		// These are just here for reference
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
		default:
			break;
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			break;
		case PA_CONTEXT_READY:
			start_stream();
			break;
	}
}


int sidpulse_init (void)
{
	int err = 0;

	paglib = pa_glib_mainloop_new (NULL);

	pa_ctx = pa_context_new (pa_glib_mainloop_get_api(paglib), "SIDsynth");

	if (pa_context_connect (pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL))
	{
		fprintf (stderr, "sidpulse_init: pa_context_connect() failed\n");
		pa_context_unref (pa_ctx);
		pa_ctx = NULL;
	}

	// This function defines a callback so the server will tell us it's state.
	// Our callback will wait for the state to be ready.
	pa_context_set_state_callback(pa_ctx, pa_state_cb, 0);

	return 0;
}

void sidpulse_done (void)
{
	if (pas)
	{
		if (pa_stream_disconnect (pas))
		{
			fprintf (stderr, "sidpulse_done: pa_stream_disconnect() failed\n");
		}

		pa_stream_unref (pas);
		pas = 0;
	}

	if (pa_ctx)
	{
		pa_context_disconnect (pa_ctx);
		pa_context_unref (pa_ctx);
		pa_ctx = NULL;
	}

	if (paglib)
	{
		pa_glib_mainloop_free (paglib);
		paglib = 0;
	}
}
