#ifndef LOGOFRAME_VIDEO_SOURCE_H
#define LOGOFRAME_VIDEO_SOURCE_H

#ifndef HAVE_AVISYNTH
#define HAVE_AVISYNTH 1
#endif
#ifndef HAVE_DTVINDEX
#define HAVE_DTVINDEX 1
#endif

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#if !defined(_WIN32)
#include <strings.h>
#endif

#if HAVE_AVISYNTH
#include "../avisynth/avs_internal.c"
#endif

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
  if (path_length < extension_length) {
    return false;
  }
#if defined(_WIN32)
  const int comparison =
      _stricmp(path + path_length - extension_length, extension);
#else
  const int comparison =
      strcasecmp(path + path_length - extension_length, extension);
#endif
  return comparison == 0;
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

#if HAVE_AVISYNTH
class AvsVideoSource : public VideoSource {
private:
  avs_hnd_t _avs;
  const AVS_VideoInfo *_video_info;
  AVS_VideoFrame *_frame;
  AVS_Value _result;
  bool _result_active;
  InputVideoInfo _info;

  static std::runtime_error make_avs_error(
      const std::string &operation,
      AVS_Value value) {
    const char *message = avs_as_error(value);
    return std::runtime_error(
        operation +
        (message != NULL ? std::string(": ") + message : ""));
  }

  void release_result() {
    if (_result_active && _avs.func.avs_release_value != NULL) {
      _avs.func.avs_release_value(_result);
    }
    _result = avs_void;
    _result_active = false;
  }

  void replace_clip(
      AVS_Value result,
      const std::string &operation) {
    if (avs_is_error(result)) {
      std::runtime_error error = make_avs_error(operation, result);
      _avs.func.avs_release_value(result);
      throw error;
    }
    if (!avs_is_clip(result)) {
      _avs.func.avs_release_value(result);
      throw std::runtime_error(
          operation + " did not return a video clip");
    }

    _result = internal_avs_update_clip(
        &_avs, &_video_info, result, _result);
    _result_active = true;
    if (_avs.clip == NULL || _video_info == NULL) {
      throw std::runtime_error(
          operation + " could not retain the resulting video clip");
    }
  }

  void close() {
    if (_frame != NULL && _avs.func.avs_release_video_frame != NULL) {
      _avs.func.avs_release_video_frame(_frame);
      _frame = NULL;
    }
    release_result();
    if (_avs.library != NULL) {
      internal_avs_close_library(&_avs);
    }
    _video_info = NULL;
  }

public:
  AvsVideoSource(const char *infile, int csp)
      : _video_info(NULL),
        _frame(NULL),
        _result(avs_void),
        _result_active(false) {
    memset(&_avs, 0, sizeof(_avs));
    memset(&_info, 0, sizeof(_info));
    try {
      if (internal_avs_load_library(&_avs) < 0) {
#if defined(_WIN32)
        throw std::runtime_error("Cannot load avisynth.dll");
#else
        throw std::runtime_error("Cannot load the AviSynth shared library");
#endif
      }

      _avs.env =
          _avs.func.avs_create_script_environment(AVS_INTERFACE_25);
      if (_avs.env == NULL) {
        throw std::runtime_error(
            "Could not create an AviSynth script environment");
      }
      if (_avs.func.avs_get_error != NULL) {
        const char *error = _avs.func.avs_get_error(_avs.env);
        if (error != NULL) {
          throw std::runtime_error(
              std::string("AviSynth environment error: ") + error);
        }
      }

      _result = _avs.func.avs_invoke(
          _avs.env,
          "Import",
          avs_new_value_string(infile),
          NULL);
      _result_active = true;
      if (avs_is_error(_result)) {
        throw make_avs_error("Could not import the AviSynth script", _result);
      }
      if (!avs_is_clip(_result)) {
        throw std::runtime_error(
            std::string("'") + infile + "' did not return a video clip");
      }

      AVS_Value mt_test = _avs.func.avs_invoke(
          _avs.env,
          "GetMTMode",
          avs_new_value_bool(0),
          NULL);
      const int mt_mode =
          avs_is_int(mt_test) ? avs_as_int(mt_test) : 0;
      _avs.func.avs_release_value(mt_test);
      if (mt_mode > 0 && mt_mode < 5) {
        AVS_Value distributed = _avs.func.avs_invoke(
            _avs.env, "Distributor", _result, NULL);
        if (avs_is_error(distributed)) {
          std::runtime_error error = make_avs_error(
              "Could not apply AviSynth Distributor", distributed);
          _avs.func.avs_release_value(distributed);
          throw error;
        }
        if (!avs_is_clip(distributed)) {
          _avs.func.avs_release_value(distributed);
          throw std::runtime_error(
              "AviSynth Distributor did not return a video clip");
        }
        release_result();
        _result = distributed;
        _result_active = true;
      }

      _avs.clip = _avs.func.avs_take_clip(_result, _avs.env);
      if (_avs.clip == NULL) {
        throw std::runtime_error(
            "Could not retain the AviSynth video clip");
      }
      _video_info = _avs.func.avs_get_video_info(_avs.clip);
      if (!avs_has_video(_video_info)) {
        throw std::runtime_error(
            std::string("'") + infile + "' has no video data");
      }

      bool interlaced = false;
      bool top_field_first = false;
      if (avs_is_field_based(_video_info)) {
        fprintf(stderr,
                "detected fieldbased (separated) input, weaving to frames\n");
        AVS_Value woven = _avs.func.avs_invoke(
            _avs.env, "Weave", _result, NULL);
        replace_clip(woven, "Could not weave the AviSynth input fields");
        interlaced = true;
        top_field_first = avs_is_tff(_video_info);
      }

      const bool expected_color_space =
          csp == CSP_I420 ? avs_is_yv12(_video_info) :
          csp == CSP_I422 ? avs_is_yv16(_video_info) :
          csp == CSP_I444 ? avs_is_yv24(_video_info) :
          false;
      if (!expected_color_space) {
        const char *csp_name = input_csp_name(csp);
        fprintf(stderr, "converting input clip to %s\n", csp_name);
        if (csp < CSP_I444 && (_video_info->width & 1)) {
          throw std::runtime_error(
              "Input clip width is not divisible by 2");
        }
        if (csp == CSP_I420 &&
            interlaced &&
            (_video_info->height & 3)) {
          throw std::runtime_error(
              "Interlaced I420 input height is not divisible by 4");
        }
        if ((csp == CSP_I420 || interlaced) &&
            (_video_info->height & 1)) {
          throw std::runtime_error(
              "Input clip height is not divisible by 2");
        }

        AVS_Value arguments[2] = {
            _result, avs_new_value_bool(interlaced)};
        const char *argument_names[2] = {NULL, "interlaced"};
        const std::string function_name =
            std::string("ConvertTo") + csp_name;
        AVS_Value converted = _avs.func.avs_invoke(
            _avs.env,
            function_name.c_str(),
            avs_new_value_array(arguments, 2),
            argument_names);
        replace_clip(
            converted,
            std::string("Could not convert the AviSynth input to ") +
                csp_name);
      }

      _info.width = _video_info->width;
      _info.height = _video_info->height;
      _info.fps_numerator = _video_info->fps_numerator;
      _info.fps_denominator = _video_info->fps_denominator;
      _info.num_frames = _video_info->num_frames;
      _info.interlaced = interlaced;
      _info.top_field_first = top_field_first;
      release_result();
    } catch (...) {
      close();
      throw;
    }
  }

  ~AvsVideoSource() { close(); }

  const InputVideoInfo &info() const { return _info; }

  InputVideoFrame read_frame(int frame) {
    if (_frame != NULL) {
      _avs.func.avs_release_video_frame(_frame);
      _frame = NULL;
    }
    _frame = _avs.func.avs_get_frame(_avs.clip, frame);
    const char *error = _avs.func.avs_clip_get_error(_avs.clip);
    if (error != NULL) {
      throw std::runtime_error(
          std::string(error) + " occurred while reading frame " +
          std::to_string(frame));
    }
    if (_frame == NULL) {
      throw std::runtime_error(
          "AviSynth returned no frame for frame " +
          std::to_string(frame));
    }
    InputVideoFrame result = {
        avs_get_read_ptr_p(_frame, AVS_PLANAR_Y),
        avs_get_pitch_p(_frame, AVS_PLANAR_Y)};
    return result;
  }

  const char *source_name() const { return "AvsSource"; }
};
#endif

#if HAVE_DTVINDEX
#include "dtvindex_video_source.h"
#endif

inline std::unique_ptr<VideoSource> open_video_source(
    const char *infile, int csp) {
  if (input_has_extension(infile, ".avs")) {
#if HAVE_AVISYNTH
    return std::unique_ptr<VideoSource>(
        new AvsVideoSource(infile, csp));
#else
    throw std::runtime_error(
        "AviSynth input is not enabled in this build");
#endif
  }
#if HAVE_DTVINDEX
  return std::unique_ptr<VideoSource>(
      new DtvIndexVideoSource(infile, csp));
#else
  throw std::runtime_error(
      "dtvindex input is not enabled in this build");
#endif
}

#endif
