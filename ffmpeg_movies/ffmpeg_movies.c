#define inline _inline
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <windows.h>
#include <gl/glew.h>
#include <math.h>
#include <sys/timeb.h>
#include <dsound.h>

#include "types.h"

// 10 frames
#define VIDEO_BUFFER_SIZE 10

// 20 seconds
#define AUDIO_BUFFER_SIZE 20

inline double round(double x) { return floor(x + 0.5); }

#define LAG (((now - start_time) - (timer_freq / movie_fps) * movie_frame_counter) / (timer_freq / 1000))

uint texture_units = 1;

bool yuv_init_done = false;
bool yuv_fast_path = false;

AVFormatContext *format_ctx = 0;
AVCodecContext *codec_ctx = 0;
AVCodec *codec = 0;
AVCodecContext *acodec_ctx = 0;
AVCodec *acodec = 0;
AVFrame *movie_frame = 0;
struct SwsContext *sws_ctx = 0;

int videostream;
int audiostream;

bool use_bgra_texture;

struct video_frame
{
	GLuint bgra_texture;
	GLuint yuv_textures[3];
};

struct video_frame video_buffer[VIDEO_BUFFER_SIZE];
uint vbuffer_read = 0;
uint vbuffer_write = 0;

uint max_texture_size;

uint movie_frame_counter = 0;
uint movie_frames;
uint movie_width, movie_height;
double movie_fps;
double movie_duration;
uint movie_frames;

bool skip_frames;
bool skipping_frames;
uint skipped_frames;

bool movie_sync_debug;

IDirectSoundBuffer *sound_buffer = 0;
uint sound_buffer_size;
uint write_pointer = 0;

bool first_audio_packet;

time_t timer_freq;
time_t start_time;

void (*trace)(char *, ...);
void (*info)(char *, ...);
void (*glitch)(char *, ...);
void (*error)(char *, ...);

void (*draw_movie_quad_bgra)(GLuint, uint, uint);
void (*draw_movie_quad_yuv)(GLuint *, uint, uint, bool);

IDirectSound **directsound;

BOOL APIENTRY DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

__declspec(dllexport) void movie_init(void *plugin_trace, void *plugin_info, void *plugin_glitch, void *plugin_error, void *plugin_draw_movie_quad_bgra, void *plugin_draw_movie_quad_yuv, IDirectSound **plugin_directsound, bool plugin_skip_frames, bool plugin_movie_sync_debug)
{
	av_register_all();

	trace = plugin_trace;
	info = plugin_info;
	glitch = plugin_glitch;
	error = plugin_error;
	draw_movie_quad_bgra = plugin_draw_movie_quad_bgra;
	draw_movie_quad_yuv = plugin_draw_movie_quad_yuv;
	directsound = plugin_directsound;
	skip_frames = plugin_skip_frames;
	movie_sync_debug = plugin_movie_sync_debug;

	info("FFMpeg movie player plugin loaded\n");
	info("FFMpeg version SVN-r25886, Copyright (c) 2000-2010 Fabrice Bellard, et al.\n");

	glewInit();

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &texture_units);

	if(texture_units < 3) info("No multitexturing, codecs with YUV output will be slow. (texture units: %i)\n", texture_units);
	else yuv_fast_path = true;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

	QueryPerformanceFrequency((LARGE_INTEGER *)&timer_freq);
}

// clean up anything we have allocated
__declspec(dllexport) void release_movie_objects()
{
	uint i;

	if(codec_ctx) avcodec_close(codec_ctx);
	if(acodec_ctx) avcodec_close(acodec_ctx);
	if(format_ctx) av_close_input_file(format_ctx);
	if(sound_buffer && *directsound) IDirectSoundBuffer_Release(sound_buffer);

	codec_ctx = 0;
	acodec_ctx = 0;
	format_ctx = 0;
	sound_buffer = 0;

	if(skipped_frames > 0) info("skipped %i frames\n", skipped_frames);
	skipped_frames = 0;

	for(i = 0; i < VIDEO_BUFFER_SIZE; i++)
	{
		glDeleteTextures(1, &video_buffer[i].bgra_texture);
		video_buffer[i].bgra_texture = 0;
		glDeleteTextures(3, video_buffer[i].yuv_textures);
		memset(video_buffer[i].yuv_textures, 0, sizeof(video_buffer[i].yuv_textures));
	}
}

// prepare a movie for playback
__declspec(dllexport) uint prepare_movie(char *name)
{
	uint i;
	WAVEFORMATEX sound_format;
	DSBUFFERDESC1 sbdesc;
	uint ret;

	if(ret = av_open_input_file(&format_ctx, name, NULL, 0, NULL))
	{
		error("couldn't open movie file: %s\n", name);
		release_movie_objects();
		goto exit;
	}

	if(av_find_stream_info(format_ctx) < 0)
	{
		error("couldn't find stream info\n");
		release_movie_objects();
		goto exit;
	}

	videostream = -1;
	audiostream = -1;
	for(i = 0; i < format_ctx->nb_streams; i++)
	{
		if(format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO && videostream < 0) videostream = i;
		if(format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO && audiostream < 0) audiostream = i;
	}

	if(videostream == -1)
	{
		error("no video stream found\n");
		release_movie_objects();
		goto exit;
	}

	if(audiostream == -1) trace("no audio stream found\n");

	codec_ctx = format_ctx->streams[videostream]->codec;

	codec = avcodec_find_decoder(codec_ctx->codec_id);
	if(!codec)
	{
		error("no video codec found\n");
		codec_ctx = 0;
		release_movie_objects();
		goto exit;
	}

	if(avcodec_open(codec_ctx, codec) < 0)
	{
		error("couldn't open video codec\n");
		release_movie_objects();
		goto exit;
	}

	if(audiostream != -1)
	{
		acodec_ctx = format_ctx->streams[audiostream]->codec;
		acodec = avcodec_find_decoder(acodec_ctx->codec_id);
		if(!acodec)
		{
			error("no audio codec found\n");
			release_movie_objects();
			goto exit;
		}

		if(avcodec_open(acodec_ctx, acodec) < 0)
		{
			error("couldn't open audio codec\n");
			release_movie_objects();
			goto exit;
		}
	}

	movie_width = codec_ctx->width;
	movie_height = codec_ctx->height;
	movie_fps = 1.0 / (av_q2d(codec_ctx->time_base) * codec_ctx->ticks_per_frame);
	movie_duration = (double)format_ctx->duration / (double)AV_TIME_BASE;
	movie_frames = (uint)round(movie_fps * movie_duration);

	if(movie_fps < 100.0) info("%s; %s/%s %ix%i, %f FPS, duration: %f, frames: %i\n", name, codec->name, acodec_ctx ? acodec->name : "null", movie_width, movie_height, movie_fps, movie_duration, movie_frames);
	// bogus FPS value, assume the codec provides frame limiting
	else info("%s; %s/%s %ix%i, duration: %f\n", name, codec->name, acodec_ctx ? acodec->name : "null", movie_width, movie_height, movie_duration);

	if(movie_width > max_texture_size || movie_height > max_texture_size)
	{
		error("movie dimensions exceed max texture size, skipping\n");
		release_movie_objects();
		goto exit;
	}

	if(!movie_frame) movie_frame = avcodec_alloc_frame();

	if(sws_ctx) sws_freeContext(sws_ctx);

	if(codec_ctx->pix_fmt == PIX_FMT_YUV420P && yuv_fast_path) use_bgra_texture = false;
	else use_bgra_texture = true;

	vbuffer_read = 0;
	vbuffer_write = 0;

	if(codec_ctx->pix_fmt != PIX_FMT_BGRA && codec_ctx->pix_fmt != PIX_FMT_BGR24 && (codec_ctx->pix_fmt != PIX_FMT_YUV420P || !yuv_fast_path))
	{
		sws_ctx = sws_getContext(movie_width, movie_height, codec_ctx->pix_fmt, movie_width, movie_height, PIX_FMT_BGR24, SWS_FAST_BILINEAR | SWS_ACCURATE_RND, NULL, NULL, NULL);
		info("slow output format from video codec %s; %i\n", codec->name, codec_ctx->pix_fmt);
	}
	else sws_ctx = 0;

	if(audiostream != -1)
	{
		if(acodec_ctx->sample_fmt != SAMPLE_FMT_U8 && acodec_ctx->sample_fmt != SAMPLE_FMT_S16) error("unsupported sample format, expect garbled audio output\n");

		sound_format.cbSize = sizeof(sound_format);
		sound_format.wBitsPerSample = acodec_ctx->sample_fmt == SAMPLE_FMT_U8 ? 8 : 16;
		sound_format.nChannels = acodec_ctx->channels;
		sound_format.nSamplesPerSec = acodec_ctx->sample_rate;
		sound_format.nBlockAlign = sound_format.nChannels * sound_format.wBitsPerSample / 8;
		sound_format.nAvgBytesPerSec = sound_format.nSamplesPerSec * sound_format.nBlockAlign;
		sound_format.wFormatTag = WAVE_FORMAT_PCM;

		sound_buffer_size = sound_format.nAvgBytesPerSec * AUDIO_BUFFER_SIZE;

		sbdesc.dwSize = sizeof(sbdesc);
		sbdesc.lpwfxFormat = &sound_format;
		sbdesc.dwFlags = 0;
		sbdesc.dwReserved = 0;
		sbdesc.dwBufferBytes = sound_buffer_size;

		if(ret = IDirectSound_CreateSoundBuffer(*directsound, (LPCDSBUFFERDESC)&sbdesc, &sound_buffer, 0))
		{
			error("couldn't create sound buffer (%i, %i, %i, %i)\n", acodec_ctx->sample_fmt, acodec_ctx->bit_rate, acodec_ctx->sample_rate, acodec_ctx->channels);
			sound_buffer = 0;
		}

		first_audio_packet = true;
		write_pointer = 0;
	}

exit:
	movie_frame_counter = 0;
	skipped_frames = 0;

	return movie_frames;
}

// stop movie playback, no video updates will be requested after this so all we have to do is stop the audio
__declspec(dllexport) void stop_movie()
{
	if(sound_buffer && *directsound) IDirectSoundBuffer_Stop(sound_buffer);
}

void buffer_bgra_frame(char *data, int upload_stride)
{
	uint upload_width = codec_ctx->pix_fmt == PIX_FMT_BGRA ? upload_stride / 4 : upload_stride / 3;

	if(upload_stride < 0) return;

	if(video_buffer[vbuffer_write].bgra_texture) glDeleteTextures(1, &video_buffer[vbuffer_write].bgra_texture);

	glGenTextures(1, &video_buffer[vbuffer_write].bgra_texture);
	glBindTexture(GL_TEXTURE_2D, video_buffer[vbuffer_write].bgra_texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, movie_width, movie_height, 0, GL_BGR, GL_UNSIGNED_BYTE, 0);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, upload_width);

	if(codec_ctx->pix_fmt == PIX_FMT_BGRA) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, movie_width, movie_height, GL_BGRA, GL_UNSIGNED_BYTE, data);
	else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, movie_width, movie_height, GL_BGR, GL_UNSIGNED_BYTE, data);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	vbuffer_write = (vbuffer_write + 1) % VIDEO_BUFFER_SIZE;
}

void draw_bgra_frame(uint buffer_index)
{
	draw_movie_quad_bgra(video_buffer[buffer_index].bgra_texture, movie_width, movie_height);
}

void upload_yuv_texture(char **planes, uint *strides, uint num, uint buffer_index)
{
	uint upload_width = strides[num];
	uint tex_width = num == 0 ? movie_width : movie_width / 2;
	uint tex_height = num == 0 ? movie_height : movie_height / 2;

	glActiveTexture(GL_TEXTURE0 + num);

	glBindTexture(GL_TEXTURE_2D, video_buffer[buffer_index].yuv_textures[num]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width, tex_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, upload_width);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height, GL_LUMINANCE, GL_UNSIGNED_BYTE, planes[num]);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void buffer_yuv_frame(char **planes, uint *strides)
{
	if(video_buffer[vbuffer_write].yuv_textures[0]) glDeleteTextures(3, video_buffer[vbuffer_write].yuv_textures);

	glGenTextures(3, video_buffer[vbuffer_write].yuv_textures);
	
	upload_yuv_texture(planes, strides, 2, vbuffer_write);
	upload_yuv_texture(planes, strides, 1, vbuffer_write);
	upload_yuv_texture(planes, strides, 0, vbuffer_write);

	vbuffer_write = (vbuffer_write + 1) % VIDEO_BUFFER_SIZE;
}

void draw_yuv_frame(uint buffer_index, bool full_range)
{
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, video_buffer[buffer_index].yuv_textures[2]);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, video_buffer[buffer_index].yuv_textures[1]);
	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, video_buffer[buffer_index].yuv_textures[0]);

	draw_movie_quad_yuv(video_buffer[buffer_index].yuv_textures, movie_width, movie_height, full_range);
}

// display the next frame
__declspec(dllexport) bool update_movie_sample()
{
	AVPacket packet;
	bool frame_finished;
	int ret;
	time_t now;

	// no playable movie loaded, skip it
	if(!format_ctx) return false;

	// keep track of when we started playing this movie
	if(movie_frame_counter == 0) QueryPerformanceCounter((LARGE_INTEGER *)&start_time);

	while((ret = av_read_frame(format_ctx, &packet)) >= 0)
	{
		if(packet.stream_index == videostream)
		{
			avcodec_decode_video2(codec_ctx, movie_frame, &frame_finished, &packet);

			if(frame_finished)
			{
				QueryPerformanceCounter((LARGE_INTEGER *)&now);

				// check if we are falling behind
				if(skip_frames && movie_fps < 100.0 && LAG > 100.0) skipping_frames = true;

				if(skipping_frames && LAG > 0.0)
				{
					skipped_frames++;
					if(((skipped_frames - 1) & skipped_frames) == 0) glitch("video playback is lagging behind, skipping frames (frame #: %i, skipped: %i, lag: %f)\n", movie_frame_counter, skipped_frames, LAG);
					av_free_packet(&packet);
					if(use_bgra_texture) draw_bgra_frame(vbuffer_read);
					else draw_yuv_frame(vbuffer_read, codec_ctx->color_range == AVCOL_RANGE_JPEG);
					break;
				}
				else skipping_frames = false;

				if(movie_sync_debug) info("video: DTS %f PTS %f (timebase %f) placed in video buffer at real time %f (play %f)\n", (double)packet.dts, (double)packet.pts, av_q2d(codec_ctx->time_base), (double)(now - start_time) / (double)timer_freq, (double)movie_frame_counter / (double)movie_fps);
				
				if(sws_ctx)
				{
					char *planes[3] = {0, 0, 0};
					int strides[3] = {0, 0, 0};
					char *data = calloc(movie_width * movie_height, 3);

					planes[0] = data;
					strides[0] = movie_width * 3;

					sws_scale(sws_ctx, movie_frame->data, movie_frame->linesize, 0, movie_height, planes, strides);

					buffer_bgra_frame(data, movie_width * 3);

					free(data);
				}
				else if(use_bgra_texture) buffer_bgra_frame(movie_frame->data[0], movie_frame->linesize[0]);
				else buffer_yuv_frame(movie_frame->data, movie_frame->linesize);

				av_free_packet(&packet);

				if(vbuffer_write == vbuffer_read)
				{
					if(use_bgra_texture) draw_bgra_frame(vbuffer_read);
					else draw_yuv_frame(vbuffer_read, codec_ctx->color_range == AVCOL_RANGE_JPEG);

					vbuffer_read = (vbuffer_read + 1) % VIDEO_BUFFER_SIZE;

					break;
				}
			}
		}

		if(packet.stream_index == audiostream)
		{
			char buffer_storage[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
			char *buffer = (char *)(((((uint)buffer_storage) + 15) / 16) * 16);
			uint size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
			int used_bytes;
			char *packet_data = packet.data;
			uint packet_size = packet.size;
			uint playcursor;
			uint writecursor;
			uint bytespersec = (acodec_ctx->sample_fmt == SAMPLE_FMT_U8 ? 1 : 2) * acodec_ctx->channels * acodec_ctx->sample_rate;

			QueryPerformanceCounter((LARGE_INTEGER *)&now);

			if(movie_sync_debug)
			{
				IDirectSoundBuffer_GetCurrentPosition(sound_buffer, &playcursor, &writecursor);
				info("audio: DTS %f PTS %f (timebase %f) placed in sound buffer at real time %f (play %f write %f)\n", (double)packet.dts, (double)packet.pts, av_q2d(acodec_ctx->time_base), (double)(now - start_time) / (double)timer_freq, (double)playcursor / (double)bytespersec, (double)write_pointer / (double)bytespersec);
			}

			while((used_bytes = avcodec_decode_audio2(acodec_ctx, (int16_t *)buffer, &size, packet_data, packet_size)) > 0)
			{
				char *ptr1;
				char *ptr2;
				uint bytes1;
				uint bytes2;

				if(sound_buffer && size)
				{
					if(IDirectSoundBuffer_Lock(sound_buffer, write_pointer, size, &ptr1, &bytes1, &ptr2, &bytes2, 0)) error("couldn't lock sound buffer\n");

					memcpy(ptr1, buffer, bytes1);
					memcpy(ptr2, &buffer[bytes1], bytes2);

					if(IDirectSoundBuffer_Unlock(sound_buffer, ptr1, bytes1, ptr2, bytes2)) error("couldn't unlock sound buffer\n");

					write_pointer = (write_pointer + bytes1 + bytes2) % sound_buffer_size;

					packet_data += used_bytes;
					packet_size -= used_bytes;
					size = sizeof(buffer);
				}
			}
		}

		av_free_packet(&packet);
	}

	if(sound_buffer && first_audio_packet)
	{
		if(movie_sync_debug) info("audio start\n");

		// reset start time so video syncs up properly
		QueryPerformanceCounter((LARGE_INTEGER *)&start_time);
		if(IDirectSoundBuffer_Play(sound_buffer, 0, 0, DSBPLAY_LOOPING)) error("couldn't play sound buffer\n");
		first_audio_packet = false;
	}

	movie_frame_counter++;

	// could not read any more frames, exhaust video buffer then end movie
	if(ret < 0)
	{
		if(vbuffer_write != vbuffer_read)
		{
			if(use_bgra_texture) draw_bgra_frame(vbuffer_read);
			else draw_yuv_frame(vbuffer_read, codec_ctx->color_range == AVCOL_RANGE_JPEG);

			vbuffer_read = (vbuffer_read + 1) % VIDEO_BUFFER_SIZE;
		}
		
		if(vbuffer_write == vbuffer_read) return false;
	}

	// wait for the next frame
	do
	{
		QueryPerformanceCounter((LARGE_INTEGER *)&now);
	} while(LAG < 0.0);

	// keep going
	return true;
}

// draw the current frame, don't update anything
__declspec(dllexport) void draw_current_frame()
{
	if(use_bgra_texture) draw_bgra_frame((vbuffer_read - 1) % VIDEO_BUFFER_SIZE);
	else draw_yuv_frame((vbuffer_read - 1) % VIDEO_BUFFER_SIZE, codec_ctx->color_range == AVCOL_RANGE_JPEG);
}

// loop back to the beginning of the movie
__declspec(dllexport) void loop()
{
	if(format_ctx) avformat_seek_file(format_ctx, -1, 0, 0, 0, 0);
}

// get the current frame number
__declspec(dllexport) uint get_movie_frame()
{
	if(movie_fps != 15.0 && movie_fps < 100.0) return (uint)ceil(movie_frame_counter * 15.0 / movie_fps);
	else return movie_frame_counter;
}
