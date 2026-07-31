#ifndef LOGOFRAME_DTVINDEX_VIDEO_SOURCE_H
#define LOGOFRAME_DTVINDEX_VIDEO_SOURCE_H

#include "dtvindex/dtvindex.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <climits>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

class DtvIndexVideoSource : public VideoSource {
private:
  std::string _media_path;
  std::string _index_path;
  dtvindex::Index _index;
  std::unique_ptr<dtvindex::VideoReader> _reader;
  dtvindex::DecodedLumaFrame _frame;
  InputVideoInfo _info;

public:
  DtvIndexVideoSource(const char *infile, int csp)
      : _media_path(infile), _index(), _reader(), _frame(), _info() {
    _index_path = dtvindex::Index::default_index_path(_media_path);
    bool created = false;
    _index = dtvindex::Index::load_or_build(
        _media_path, _index_path, &created);

    const dtvindex::StreamInfo &stream = _index.stream();
    if (stream.width <= 0 || stream.height <= 0 ||
        stream.frame_rate.numerator <= 0 ||
        stream.frame_rate.denominator <= 0) {
      throw std::runtime_error(
          "dtvindex contains invalid video metadata");
    }

    const AVFieldOrder field_order =
        static_cast<AVFieldOrder>(stream.field_order);
    _info.width = stream.width;
    _info.height = stream.height;
    _info.fps_numerator = stream.frame_rate.numerator;
    _info.fps_denominator = stream.frame_rate.denominator;
    _info.num_frames = static_cast<int>(
        std::min<std::uint64_t>(_index.frames().size(), INT_MAX));
    _info.interlaced =
        field_order != AV_FIELD_UNKNOWN &&
        field_order != AV_FIELD_PROGRESSIVE;
    _info.top_field_first =
        field_order == AV_FIELD_TT ||
        field_order == AV_FIELD_TB;

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

    _reader.reset(new dtvindex::VideoReader(_media_path, _index));
    fprintf(stderr,
            "dtvindex: %s %s\n",
            created ? "created" : "reused",
            _index_path.c_str());
  }

  const InputVideoInfo &info() const {
    return _info;
  }

  InputVideoFrame read_frame(int frame) {
    if (frame < 0 || frame >= _info.num_frames) {
      throw std::out_of_range(
          "DtvIndexSource frame is outside the index");
    }
    _frame = _reader->read_luma(
        static_cast<std::uint64_t>(frame));
    InputVideoFrame result = {
        _frame.pixels.data(), _frame.width};
    return result;
  }

  const char *source_name() const {
    return "DtvIndexSource";
  }
};

#endif
