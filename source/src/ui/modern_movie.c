#include "ui/modern_movie.h"
#include <tig/tig.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>

typedef struct Movie {
    AVFormatContext* format;
    AVCodecContext* video;
    AVCodecContext* audio;
    AVFrame* frame;
    AVPacket* packet;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_AudioStream* sound;
    int video_index, audio_index;
    bool started, stopped, ignore_keys, silent;
    SDL_Scancode skip_key;
    Uint64 start;
    double end_time;
} Movie;

static bool movie_quit;

int modern_movie_check(void)
{
    return av_find_input_format("bink") && avcodec_find_decoder(AV_CODEC_ID_BINKVIDEO)
        && avcodec_find_decoder(AV_CODEC_ID_BINKAUDIO_RDFT)
        && avcodec_find_decoder(AV_CODEC_ID_BINKAUDIO_DCT) ? 0 : 1;
}

static void movie_events(Movie* movie)
{
    tig_ping();
    TigMessage message;
    while (tig_message_dequeue(&message) == TIG_OK) {
        if (message.type == TIG_MESSAGE_QUIT) movie_quit = movie->stopped = true;
        if (!movie->ignore_keys && message.type == TIG_MESSAGE_KEYBOARD
            && message.data.keyboard.pressed && movie->skip_key == SDL_SCANCODE_UNKNOWN) {
            movie->skip_key = message.data.keyboard.scancode;
            movie->stopped = true;
        }
    }
}

static void movie_wait(Movie* movie, double seconds)
{
    while (!movie->stopped && (SDL_GetTicksNS() - movie->start) / 1e9 < seconds) {
        movie_events(movie);
        SDL_Delay(2);
    }
}

static AVCodecContext* movie_decoder(AVStream* stream)
{
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) return NULL;
    AVCodecContext* context = avcodec_alloc_context3(codec);
    if (!context) return NULL;
    if (avcodec_parameters_to_context(context, stream->codecpar) < 0
        || avcodec_open2(context, codec, NULL) < 0) avcodec_free_context(&context);
    return context;
}

static bool movie_audio(Movie* movie, AVFrame* frame)
{
    if (movie->silent) return true;
    int channels = frame->ch_layout.nb_channels;
    if (channels < 1 || channels > 8 || frame->nb_samples < 0 || frame->nb_samples > 1048576) return false;
    if (!movie->sound) {
        SDL_AudioSpec spec = {SDL_AUDIO_F32, channels, frame->sample_rate};
        movie->sound = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (!movie->sound) {
            SDL_Log("Movie audio unavailable: %s", SDL_GetError());
            movie->silent = true;
            return true;
        }
        if (movie->started) SDL_ResumeAudioStreamDevice(movie->sound);
    }
    if (frame->format == AV_SAMPLE_FMT_FLT)
        return SDL_PutAudioStreamData(movie->sound, frame->data[0], frame->nb_samples * channels * sizeof(float));
    if (frame->format != AV_SAMPLE_FMT_FLTP) return false;
    size_t size = (size_t)frame->nb_samples * channels * sizeof(float);
    float* samples = SDL_malloc(size);
    if (!samples) return false;
    for (int n = 0; n < frame->nb_samples; n++)
        for (int c = 0; c < channels; c++)
            samples[n * channels + c] = ((const float*)frame->extended_data[c])[n];
    bool ok = SDL_PutAudioStreamData(movie->sound, samples, (int)size);
    SDL_free(samples);
    return ok;
}

static bool movie_video(Movie* movie, AVFrame* frame)
{
    if (frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVA420P) return false;
    if (!movie->texture) {
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_IYUV);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STREAMING);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, frame->width);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, frame->height);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
            frame->color_range == AVCOL_RANGE_JPEG ? SDL_COLORSPACE_BT601_FULL : SDL_COLORSPACE_BT601_LIMITED);
        movie->texture = SDL_CreateTextureWithProperties(movie->renderer, props);
        SDL_DestroyProperties(props);
        if (!movie->texture) return false;
        SDL_SetTextureScaleMode(movie->texture, SDL_SCALEMODE_LINEAR);
    }
    AVStream* stream = movie->format->streams[movie->video_index];
    double timestamp = frame->best_effort_timestamp == AV_NOPTS_VALUE ? movie->end_time
        : frame->best_effort_timestamp * av_q2d(stream->time_base);
    double duration = frame->duration > 0 ? frame->duration * av_q2d(stream->time_base)
        : stream->avg_frame_rate.num > 0 ? av_q2d(av_inv_q(stream->avg_frame_rate)) : 1.0 / 30;
    if (movie->started) movie_wait(movie, timestamp);
    if (movie->stopped) return true;
    if (!SDL_UpdateYUVTexture(movie->texture, NULL, frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2])) return false;
    SDL_SetRenderLogicalPresentation(movie->renderer, frame->width, frame->height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawColor(movie->renderer, 0, 0, 0, 255);
    SDL_RenderClear(movie->renderer);
    if (!SDL_RenderTexture(movie->renderer, movie->texture, NULL, NULL)
        || !SDL_RenderPresent(movie->renderer)) return false;
    if (!movie->started) {
        movie->start = SDL_GetTicksNS();
        movie->started = true;
        if (movie->sound) SDL_ResumeAudioStreamDevice(movie->sound);
    }
    movie->end_time = timestamp + duration;
    return true;
}

static bool movie_decode(Movie* movie, AVCodecContext* decoder, const AVPacket* packet, bool video)
{
    int result = avcodec_send_packet(decoder, packet);
    if (result < 0) return false;
    while ((result = avcodec_receive_frame(decoder, movie->frame)) >= 0) {
        bool ok = video ? movie_video(movie, movie->frame) : movie_audio(movie, movie->frame);
        av_frame_unref(movie->frame);
        if (!ok) return false;
        if (movie->stopped) return true;
    }
    return result == AVERROR(EAGAIN) || result == AVERROR_EOF;
}

static int movie_play(const char* path, TigMovieFlags flags, int sound_track)
{
    movie_quit = false;
    Movie movie = {0};
    movie.video_index = movie.audio_index = -1;
    movie.ignore_keys = (flags & TIG_MOVIE_IGNORE_KEYBOARD) != 0;
    if (!path || tig_video_renderer_get(&movie.renderer) != TIG_OK) return TIG_ERR_INVALID_PARAM;
    int result = TIG_ERR_IO;
    if (avformat_open_input(&movie.format, path, NULL, NULL) < 0) goto cleanup;
    if (avformat_find_stream_info(movie.format, NULL) < 0) goto cleanup;
    for (unsigned i = 0; i < movie.format->nb_streams; i++) {
        AVStream* stream = movie.format->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && movie.video_index < 0) movie.video_index = i;
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && stream->id == sound_track) movie.audio_index = i;
    }
    if (movie.video_index < 0) goto cleanup;
    movie.video = movie_decoder(movie.format->streams[movie.video_index]);
    if (movie.audio_index >= 0) movie.audio = movie_decoder(movie.format->streams[movie.audio_index]);
    movie.frame = av_frame_alloc(); movie.packet = av_packet_alloc();
    if (!movie.video || (movie.audio_index >= 0 && !movie.audio) || !movie.frame || !movie.packet) goto cleanup;

    SDL_Texture* target = SDL_GetRenderTarget(movie.renderer);
    int old_width, old_height;
    SDL_RendererLogicalPresentation old_mode;
    SDL_SetRenderTarget(movie.renderer, NULL);
    SDL_GetRenderLogicalPresentation(movie.renderer, &old_width, &old_height, &old_mode);
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(movie.renderer, &r, &g, &b, &a);
    bool cursor = SDL_CursorVisible();
    SDL_HideCursor();
    if (flags & TIG_MOVIE_FADE_IN) tig_video_fade(0, 64, 3.0f, 0);
    tig_video_fade(0, 0, 0, 1);
    TigMessage message;
    while (tig_message_dequeue(&message) == TIG_OK)
        if (message.type == TIG_MESSAGE_QUIT) movie_quit = movie.stopped = true;
    int read_result = 0;
    bool ok = true;
    while (!movie.stopped && (read_result = av_read_frame(movie.format, movie.packet)) >= 0) {
        movie_events(&movie);
        if (!movie.stopped && movie.packet->stream_index == movie.video_index)
            ok = movie_decode(&movie, movie.video, movie.packet, true);
        else if (!movie.stopped && movie.audio && movie.packet->stream_index == movie.audio_index)
            ok = movie_decode(&movie, movie.audio, movie.packet, false);
        av_packet_unref(movie.packet);
        if (!ok) break;
    }
    if (ok && !movie.stopped && read_result == AVERROR_EOF) {
        ok = movie_decode(&movie, movie.video, NULL, true);
        if (ok && movie.audio) ok = movie_decode(&movie, movie.audio, NULL, false);
        if (movie.started) movie_wait(&movie, movie.end_time);
    }
    SDL_DestroyAudioStream(movie.sound); movie.sound = NULL;
    while (!movie_quit && movie.skip_key != SDL_SCANCODE_UNKNOWN && tig_kb_is_key_pressed(movie.skip_key)) {
        movie_events(&movie); SDL_Delay(5);
    }
    if (movie_quit) tig_message_post_quit(0);
    if (!(flags & TIG_MOVIE_NO_FINAL_FLIP)) {
        SDL_SetRenderDrawColor(movie.renderer, 0, 0, 0, 255);
        SDL_RenderClear(movie.renderer); SDL_RenderPresent(movie.renderer);
    }
    if (flags & TIG_MOVIE_FADE_OUT) tig_video_fade(0, 0, 0, 0);
    SDL_SetRenderLogicalPresentation(movie.renderer, old_width, old_height, old_mode);
    SDL_SetRenderDrawColor(movie.renderer, r, g, b, a);
    SDL_SetRenderTarget(movie.renderer, target);
    if (cursor) SDL_ShowCursor();
    tig_window_invalidate_rect(NULL);
    if (ok && (movie.stopped || read_result == AVERROR_EOF)) result = TIG_OK;
cleanup:
    if (result != TIG_OK) SDL_Log("Cannot play movie %s: %s", path, SDL_GetError());
    SDL_DestroyAudioStream(movie.sound);
    SDL_DestroyTexture(movie.texture);
    av_packet_free(&movie.packet); av_frame_free(&movie.frame);
    avcodec_free_context(&movie.video); avcodec_free_context(&movie.audio);
    avformat_close_input(&movie.format);
    return result;
}

void modern_movie_init(void)
{
    movie_quit = false;
    tig_movie_set_player(movie_play);
}
