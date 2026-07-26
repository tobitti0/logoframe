#ifndef LOGOFRAME_VIDEO_SOURCE_H
#define LOGOFRAME_VIDEO_SOURCE_H

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <avisynth.h>
#include <dlfcn.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

enum InputColorSpace {
  CSP_I420 = 1,
  CSP_I422 = 2,
  CSP_I444 = 3
};

struct InputVideoInfo {
  int width;
  int height;
  int fps_numerator;
  int fps_denominator;
  int num_frames;
  bool interlaced;
  bool top_field_first;
};

struct InputVideoFrame {
  const unsigned char *data;
  int pitch;
};

class VideoSource {
public:
  virtual ~VideoSource() {}
  virtual const InputVideoInfo &info() const = 0;
  virtual InputVideoFrame read_frame(int frame) = 0;
  virtual const char *source_name() const = 0;
};

inline bool input_has_extension(const char *path, const char *extension) {
  const size_t path_length = strlen(path);
  const size_t extension_length = strlen(extension);
  return path_length >= extension_length &&
         strcasecmp(path + path_length - extension_length, extension) == 0;
}

inline const char *input_csp_name(int csp) {
  switch (csp) {
    case CSP_I420:
      return "YV12";
    case CSP_I422:
      return "YV16";
    case CSP_I444:
      return "YV24";
    default:
      throw std::runtime_error("Unknown input color space");
  }
}

class AvsVideoSource : public VideoSource {
private:
  void *_library;
  IScriptEnvironment *_environment;
  AVSValue _result;
  PClip _clip;
  PVideoFrame _frame;
  InputVideoInfo _info;

  void close() {
    _frame = PVideoFrame();
    _clip = PClip();
    _result = AVSValue();
    if (_environment != NULL) {
      _environment->DeleteScriptEnvironment();
      _environment = NULL;
    }
    AVS_linkage = NULL;
    if (_library != NULL) {
      dlclose(_library);
      _library = NULL;
    }
  }

public:
  AvsVideoSource(const char *infile, int csp)
      : _library(NULL), _environment(NULL), _result(), _clip(), _frame() {
    memset(&_info, 0, sizeof(_info));
    try {
      _library = dlopen("libavisynth.so", RTLD_LAZY);
      if (_library == NULL) {
        throw std::runtime_error("Cannot load libavisynth.so");
      }

      void *symbol = dlsym(_library, "CreateScriptEnvironment");
      if (symbol == NULL) {
        throw std::runtime_error("Cannot find CreateScriptEnvironment");
      }

      typedef IScriptEnvironment *(*CreateScriptEnvironmentFunc)(int);
      CreateScriptEnvironmentFunc create_environment =
          reinterpret_cast<CreateScriptEnvironmentFunc>(symbol);
      _environment = create_environment(AVISYNTH_INTERFACE_VERSION);
      if (_environment == NULL) {
        throw std::runtime_error(
            "Could not create an AviSynth script environment");
      }
      AVS_linkage = _environment->GetAVSLinkage();

      try {
        _result = _environment->Invoke("Import", AVSValue(infile));
      } catch (const AvisynthError &error) {
        throw std::runtime_error(
            std::string("AviSynth error: ") + error.msg);
      }
      if (!_result.IsClip()) {
        throw std::runtime_error(
            std::string("'") + infile + "' did not return a video clip");
      }

      _clip = _result.AsClip();
      VideoInfo avs_info = _clip->GetVideoInfo();
      if (!avs_info.HasVideo()) {
        throw std::runtime_error(
            std::string("'") + infile + "' has no video data");
      }

      bool interlaced = false;
      bool top_field_first = false;
      if (avs_info.IsFieldBased()) {
        fprintf(stderr,
                "detected fieldbased (separated) input, weaving to frames\n");
        try {
          _result = _environment->Invoke("Weave", _result);
        } catch (const AvisynthError &) {
          throw std::runtime_error(
              "Could not weave the AviSynth input fields");
        }
        _clip = _result.AsClip();
        avs_info = _clip->GetVideoInfo();
        interlaced = true;
        top_field_first = avs_info.IsTFF();
      }

      const int expected_bits =
          csp == CSP_I420 ? 12 : csp == CSP_I422 ? 16 : 24;
      if (avs_info.BitsPerPixel() != expected_bits) {
        const char *csp_name = input_csp_name(csp);
        fprintf(stderr, "converting input clip to %s\n", csp_name);
        if (csp < CSP_I444 && (avs_info.width & 1)) {
          throw std::runtime_error(
              "Input clip width is not divisible by 2");
        }
        if (csp == CSP_I420 && interlaced && (avs_info.height & 3)) {
          throw std::runtime_error(
              "Interlaced I420 input height is not divisible by 4");
        }
        if ((csp == CSP_I420 || interlaced) &&
            (avs_info.height & 1)) {
          throw std::runtime_error(
              "Input clip height is not divisible by 2");
        }

        AVSValue arguments[2] = {_result, AVSValue(interlaced)};
        const char *argument_names[2] = {NULL, "interlaced"};
        const std::string function_name =
            std::string("ConvertTo") + csp_name;
        try {
          _result = _environment->Invoke(
              function_name.c_str(), AVSValue(arguments, 2), argument_names);
        } catch (const AvisynthError &) {
          throw std::runtime_error(
              std::string("Could not convert the AviSynth input to ") +
              csp_name);
        }
        _clip = _result.AsClip();
        avs_info = _clip->GetVideoInfo();
      }

      _info.width = avs_info.width;
      _info.height = avs_info.height;
      _info.fps_numerator = avs_info.fps_numerator;
      _info.fps_denominator = avs_info.fps_denominator;
      _info.num_frames = avs_info.num_frames;
      _info.interlaced = interlaced;
      _info.top_field_first = top_field_first;
    } catch (...) {
      close();
      throw;
    }
  }

  ~AvsVideoSource() { close(); }

  const InputVideoInfo &info() const { return _info; }

  InputVideoFrame read_frame(int frame) {
    try {
      _frame = _clip->GetFrame(frame, _environment);
    } catch (const AvisynthError &error) {
      throw std::runtime_error(
          std::string(error.msg) + " occurred while reading frame " +
          std::to_string(frame));
    }
    InputVideoFrame result = {
        _frame->GetReadPtr(PLANAR_Y), _frame->GetPitch(PLANAR_Y)};
    return result;
  }

  const char *source_name() const { return "AvsSource"; }
};

class FFmpegVideoSource : public VideoSource {
private:
  AVFormatContext *_format_context;
  AVCodecContext *_codec_context;
  AVPacket *_packet;
  AVFrame *_frame;
  SwsContext *_sws_context;
  int _stream_index;
  bool _demux_eof;
  bool _flush_sent;
  int _decode_warning_count;
  bool _timeline_initialized;
  bool _first_decoded_frame_seen;
  bool _pending_frame_valid;
  int _last_requested_frame;
  int _current_timeline_index;
  int _pending_timeline_index;
  int _last_decoded_timeline_index;
  AVPixelFormat _output_pixel_format;
  InputVideoInfo _info;
  std::vector<unsigned char> _current_luma;
  std::vector<unsigned char> _pending_luma;
  std::vector<unsigned char> _converted_buffer;
  std::vector<int64_t> _predecode_packet_timestamps;

  static std::runtime_error make_error(const char *operation,
                                       int error_code) {
    char error_text[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(error_code, error_text, sizeof(error_text));
    return std::runtime_error(
        std::string(operation) + ": " + error_text);
  }

  static int64_t count_video_packets(const char *infile) {
    AVFormatContext *format_context = NULL;
    AVPacket *packet = NULL;
    int64_t packet_count = 0;

    int ret = avformat_open_input(
        &format_context, infile, NULL, NULL);
    if (ret < 0) {
      return 0;
    }
    ret = avformat_find_stream_info(format_context, NULL);
    if (ret < 0) {
      avformat_close_input(&format_context);
      return 0;
    }
    const int stream_index = av_find_best_stream(
        format_context,
        AVMEDIA_TYPE_VIDEO,
        -1,
        -1,
        NULL,
        0);
    if (stream_index < 0) {
      avformat_close_input(&format_context);
      return 0;
    }

    packet = av_packet_alloc();
    if (packet == NULL) {
      avformat_close_input(&format_context);
      return 0;
    }
    while (av_read_frame(format_context, packet) >= 0) {
      if (packet->stream_index == stream_index) {
        ++packet_count;
      }
      av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avformat_close_input(&format_context);
    return packet_count;
  }

  void report_recoverable_error(int error_code) {
    if (_decode_warning_count < 10) {
      char error_text[AV_ERROR_MAX_STRING_SIZE] = {0};
      av_strerror(error_code, error_text, sizeof(error_text));
      fprintf(stderr,
              "warning: FFmpeg skipped invalid video data: %s\n",
              error_text);
      if (_decode_warning_count == 9) {
        fprintf(stderr,
                "warning: further FFmpeg decode warnings are suppressed\n");
      }
    }
    ++_decode_warning_count;
  }

  int64_t estimate_frame_count(AVStream *stream,
                               AVRational frame_rate) const {
    if (stream->nb_frames > 0) {
      return stream->nb_frames;
    }
    const AVRational frame_duration = av_inv_q(frame_rate);
    if (stream->duration > 0 &&
        stream->duration != AV_NOPTS_VALUE) {
      return av_rescale_q(
          stream->duration, stream->time_base, frame_duration);
    }
    if (_format_context->duration > 0 &&
        _format_context->duration != AV_NOPTS_VALUE) {
      return av_rescale_q(
          _format_context->duration, AV_TIME_BASE_Q, frame_duration);
    }
    return 0;
  }

  bool decode_next_frame() {
    av_frame_unref(_frame);
    for (;;) {
      int ret = avcodec_receive_frame(_codec_context, _frame);
      if (ret == 0) {
        return true;
      }
      if (ret == AVERROR_EOF) {
        return false;
      }
      if (ret == AVERROR_INVALIDDATA) {
        report_recoverable_error(ret);
        continue;
      }
      if (ret != AVERROR(EAGAIN)) {
        throw make_error("FFmpeg video decoding failed", ret);
      }

      if (_demux_eof) {
        if (_flush_sent) {
          return false;
        }
        ret = avcodec_send_packet(_codec_context, NULL);
        if (ret < 0 && ret != AVERROR_EOF) {
          throw make_error("FFmpeg video decoder flush failed", ret);
        }
        _flush_sent = true;
        continue;
      }

      for (;;) {
        ret = av_read_frame(_format_context, _packet);
        if (ret < 0) {
          if (ret == AVERROR_INVALIDDATA) {
            report_recoverable_error(ret);
            continue;
          }
          if (ret != AVERROR_EOF) {
            throw make_error("FFmpeg video packet reading failed", ret);
          }
          _demux_eof = true;
          break;
        }
        if (_packet->stream_index != _stream_index) {
          av_packet_unref(_packet);
          continue;
        }
        if (!_first_decoded_frame_seen &&
            _packet->pts != AV_NOPTS_VALUE) {
          _predecode_packet_timestamps.push_back(_packet->pts);
        }

        ret = avcodec_send_packet(_codec_context, _packet);
        av_packet_unref(_packet);
        if (ret == AVERROR_INVALIDDATA) {
          report_recoverable_error(ret);
          continue;
        }
        if (ret < 0) {
          throw make_error(
              "FFmpeg video packet submission failed", ret);
        }
        break;
      }
    }
  }

  bool can_copy_luma_directly() const {
    const AVPixelFormat pixel_format =
        static_cast<AVPixelFormat>(_frame->format);
    const AVPixFmtDescriptor *descriptor =
        av_pix_fmt_desc_get(pixel_format);
    return descriptor != NULL &&
           (descriptor->flags &
            (AV_PIX_FMT_FLAG_RGB | AV_PIX_FMT_FLAG_PAL |
             AV_PIX_FMT_FLAG_BAYER | AV_PIX_FMT_FLAG_HWACCEL)) == 0 &&
           descriptor->nb_components > 0 &&
           descriptor->comp[0].plane == 0 &&
           descriptor->comp[0].step == 1 &&
           descriptor->comp[0].offset == 0 &&
           descriptor->comp[0].depth == 8;
  }

  void copy_plane(const unsigned char *source,
                  int source_pitch,
                  std::vector<unsigned char> &output) {
    output.resize(static_cast<size_t>(_info.width) * _info.height);
    for (int y = 0; y < _info.height; ++y) {
      memcpy(&output[static_cast<size_t>(y) * _info.width],
             source + static_cast<ptrdiff_t>(y) * source_pitch,
             _info.width);
    }
  }

  void convert_frame(std::vector<unsigned char> &output) {
    if (_frame->width != _info.width ||
        _frame->height != _info.height) {
      throw std::runtime_error(
          "FFmpeg video dimensions changed while decoding");
    }
    if (can_copy_luma_directly()) {
      copy_plane(_frame->data[0], _frame->linesize[0], output);
      return;
    }

    const AVPixelFormat input_pixel_format =
        static_cast<AVPixelFormat>(_frame->format);
    _sws_context = sws_getCachedContext(
        _sws_context,
        _frame->width,
        _frame->height,
        input_pixel_format,
        _info.width,
        _info.height,
        _output_pixel_format,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);
    if (_sws_context == NULL) {
      throw std::runtime_error(
          "FFmpeg could not create the video conversion context");
    }

    const int buffer_size = av_image_get_buffer_size(
        _output_pixel_format, _info.width, _info.height, 1);
    if (buffer_size < 0) {
      throw make_error(
          "FFmpeg could not calculate the converted frame size",
          buffer_size);
    }
    _converted_buffer.resize(static_cast<size_t>(buffer_size));
    uint8_t *destination_data[4] = {NULL, NULL, NULL, NULL};
    int destination_linesize[4] = {0, 0, 0, 0};
    int ret = av_image_fill_arrays(
        destination_data,
        destination_linesize,
        _converted_buffer.data(),
        _output_pixel_format,
        _info.width,
        _info.height,
        1);
    if (ret < 0) {
      throw make_error(
          "FFmpeg could not prepare the converted frame", ret);
    }
    ret = sws_scale(
        _sws_context,
        _frame->data,
        _frame->linesize,
        0,
        _frame->height,
        destination_data,
        destination_linesize);
    if (ret != _info.height) {
      throw std::runtime_error("FFmpeg video conversion failed");
    }
    copy_plane(destination_data[0], destination_linesize[0], output);
  }

  bool decode_timeline_frame(std::vector<unsigned char> &output,
                             int *timeline_index) {
    if (!decode_next_frame()) {
      return false;
    }
    convert_frame(output);

    int64_t timestamp = _frame->best_effort_timestamp;
    if (timestamp == AV_NOPTS_VALUE) {
      timestamp = _frame->pts;
    }
    if (!_first_decoded_frame_seen) {
      int leading_samples = 0;
      if (timestamp != AV_NOPTS_VALUE) {
        for (size_t i = 0;
             i < _predecode_packet_timestamps.size();
             ++i) {
          if (_predecode_packet_timestamps[i] < timestamp) {
            ++leading_samples;
          }
        }
      }
      *timeline_index = leading_samples;
      _first_decoded_frame_seen = true;
      _predecode_packet_timestamps.clear();
    } else {
      *timeline_index = _last_decoded_timeline_index + 1;
    }
    _last_decoded_timeline_index = *timeline_index;
    return true;
  }

  void close() {
    sws_freeContext(_sws_context);
    _sws_context = NULL;
    av_frame_free(&_frame);
    av_packet_free(&_packet);
    avcodec_free_context(&_codec_context);
    avformat_close_input(&_format_context);
  }

public:
  FFmpegVideoSource(const char *infile, int csp)
      : _format_context(NULL),
        _codec_context(NULL),
        _packet(NULL),
        _frame(NULL),
        _sws_context(NULL),
        _stream_index(-1),
        _demux_eof(false),
        _flush_sent(false),
        _decode_warning_count(0),
        _timeline_initialized(false),
        _first_decoded_frame_seen(false),
        _pending_frame_valid(false),
        _last_requested_frame(-1),
        _current_timeline_index(-1),
        _pending_timeline_index(-1),
        _last_decoded_timeline_index(-1),
        _output_pixel_format(AV_PIX_FMT_YUV420P) {
    memset(&_info, 0, sizeof(_info));
    try {
      const int64_t packet_count = count_video_packets(infile);
      int ret = avformat_open_input(
          &_format_context, infile, NULL, NULL);
      if (ret < 0) {
        throw make_error("FFmpeg could not open input", ret);
      }
      ret = avformat_find_stream_info(_format_context, NULL);
      if (ret < 0) {
        throw make_error(
            "FFmpeg could not read stream information", ret);
      }

      const AVCodec *decoder = NULL;
      ret = av_find_best_stream(
          _format_context,
          AVMEDIA_TYPE_VIDEO,
          -1,
          -1,
          &decoder,
          0);
      if (ret < 0) {
        throw make_error("FFmpeg could not find a video stream", ret);
      }
      _stream_index = ret;
      AVStream *stream = _format_context->streams[_stream_index];
      if ((stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0) {
        throw std::runtime_error(
            "FFmpeg selected an attached picture instead of video");
      }

      _codec_context = avcodec_alloc_context3(decoder);
      if (_codec_context == NULL) {
        throw std::runtime_error(
            "FFmpeg could not allocate a decoder context");
      }
      ret = avcodec_parameters_to_context(
          _codec_context, stream->codecpar);
      if (ret < 0) {
        throw make_error(
            "FFmpeg could not copy decoder parameters", ret);
      }
      ret = avcodec_open2(_codec_context, decoder, NULL);
      if (ret < 0) {
        throw make_error("FFmpeg could not open the decoder", ret);
      }

      AVRational frame_rate =
          av_guess_frame_rate(_format_context, stream, NULL);
      if (frame_rate.num <= 0 || frame_rate.den <= 0) {
        frame_rate = stream->avg_frame_rate;
      }
      if (frame_rate.num <= 0 || frame_rate.den <= 0) {
        frame_rate = stream->r_frame_rate;
      }
      if (frame_rate.num <= 0 || frame_rate.den <= 0) {
        throw std::runtime_error(
            "FFmpeg could not determine the video frame rate");
      }

      const int64_t duration_frame_count =
          estimate_frame_count(stream, frame_rate);
      int64_t frame_count = duration_frame_count;
      if (stream->nb_frames <= 0 &&
          packet_count > 0 &&
          duration_frame_count > 0) {
        const int64_t difference =
            packet_count > duration_frame_count
                ? packet_count - duration_frame_count
                : duration_frame_count - packet_count;
        const int64_t tolerance =
            std::max<int64_t>(10, duration_frame_count / 100);
        if (difference <= tolerance) {
          frame_count = packet_count;
        }
      }
      if (frame_count <= 0) {
        throw std::runtime_error(
            "FFmpeg could not determine the video frame count");
      }
      if (_codec_context->width <= 0 ||
          _codec_context->height <= 0) {
        throw std::runtime_error(
            "FFmpeg reported invalid video dimensions");
      }

      _info.width = _codec_context->width;
      _info.height = _codec_context->height;
      _info.fps_numerator = frame_rate.num;
      _info.fps_denominator = frame_rate.den;
      _info.num_frames =
          static_cast<int>(std::min<int64_t>(frame_count, INT_MAX));
      _info.interlaced =
          _codec_context->field_order != AV_FIELD_UNKNOWN &&
          _codec_context->field_order != AV_FIELD_PROGRESSIVE;
      _info.top_field_first =
          _codec_context->field_order == AV_FIELD_TT ||
          _codec_context->field_order == AV_FIELD_TB;

      if (csp == CSP_I422) {
        _output_pixel_format = AV_PIX_FMT_YUV422P;
      } else if (csp == CSP_I444) {
        _output_pixel_format = AV_PIX_FMT_YUV444P;
      }
      if (csp < CSP_I444 && (_info.width & 1)) {
        throw std::runtime_error(
            "Input video width is not divisible by 2");
      }
      if (csp == CSP_I420 && _info.interlaced &&
          (_info.height & 3)) {
        throw std::runtime_error(
            "Interlaced I420 input height is not divisible by 4");
      }
      if ((csp == CSP_I420 || _info.interlaced) &&
          (_info.height & 1)) {
        throw std::runtime_error(
            "Input video height is not divisible by 2");
      }

      _packet = av_packet_alloc();
      _frame = av_frame_alloc();
      if (_packet == NULL || _frame == NULL) {
        throw std::runtime_error(
            "FFmpeg could not allocate video decode buffers");
      }
    } catch (...) {
      close();
      throw;
    }
  }

  ~FFmpegVideoSource() { close(); }

  const InputVideoInfo &info() const { return _info; }

  InputVideoFrame read_frame(int frame) {
    if (frame < _last_requested_frame) {
      throw std::runtime_error(
          "FFmpegSource only supports forward frame access");
    }
    _last_requested_frame = frame;

    if (!_timeline_initialized) {
      if (!decode_timeline_frame(
              _current_luma, &_current_timeline_index)) {
        throw std::runtime_error(
            "FFmpeg reached end of input before decoding a frame");
      }
      _pending_frame_valid = decode_timeline_frame(
          _pending_luma, &_pending_timeline_index);
      _timeline_initialized = true;
    }

    while (_pending_frame_valid &&
           _pending_timeline_index <= frame) {
      _current_luma.swap(_pending_luma);
      _current_timeline_index = _pending_timeline_index;
      _pending_frame_valid = decode_timeline_frame(
          _pending_luma, &_pending_timeline_index);
    }

    InputVideoFrame result = {
        _current_luma.data(), _info.width};
    return result;
  }

  const char *source_name() const { return "FFmpegSource"; }
};

#include "dtvindex_video_source.h"

inline std::unique_ptr<VideoSource> open_video_source(
    const char *infile, int csp) {
  if (input_has_extension(infile, ".avs")) {
    return std::unique_ptr<VideoSource>(
        new AvsVideoSource(infile, csp));
  }
  return std::unique_ptr<VideoSource>(
      new DtvIndexVideoSource(infile, csp));
}

#endif
