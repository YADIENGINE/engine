#pragma once
// msf_gif (ThirdParty/msf_gif) under the editor's own names. raylib compiles its own copy for GIF recording with the
// library's names, and two definitions of one name do not link; renaming at include keeps both copies unmodified.
#define msf_gif_begin EditorMsfGifBegin
#define msf_gif_frame EditorMsfGifFrame
#define msf_gif_end EditorMsfGifEnd
#define msf_gif_free EditorMsfGifFree
#define msf_gif_begin_to_file EditorMsfGifBeginToFile
#define msf_gif_frame_to_file EditorMsfGifFrameToFile
#define msf_gif_end_to_file EditorMsfGifEndToFile
#define msf_gif_alpha_threshold EditorMsfGifAlphaThreshold
#define msf_gif_bgra_flag EditorMsfGifBgraFlag
#include <msf_gif.h>
