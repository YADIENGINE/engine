/* miniaudio and stb_vorbis, compiled once and unmodified. Only Audio/Internals may use them. */
#define MA_NO_JACK
#define MA_COINIT_VALUE 2 /* Apartment threaded: the window's file dialogs and drag and drop need the same on this thread */
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
