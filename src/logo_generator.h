#ifndef LOGOFRAME_LOGO_GENERATOR_H
#define LOGOFRAME_LOGO_GENERATOR_H

#include "video_source.h"

#include <string>

struct LogoRect {
  int x;
  int y;
  int width;
  int height;

  LogoRect() : x(0), y(0), width(0), height(0) {}
  LogoRect(int x_value, int y_value, int width_value, int height_value)
      : x(x_value),
        y(y_value),
        width(width_value),
        height(height_value) {}
};

struct ManagedLogoOptions {
  std::string channel;
  std::string directory;
  int sample_frames;
  int background_threshold;
  int edge_threshold;
  int match_threshold_percent;
  bool has_manual_rect;
  LogoRect manual_rect;

  ManagedLogoOptions();
};

struct LogoGenerationResult {
  LogoRect rect;
  int sampled_frames;
  int uniform_frames;
  int logo_frames;
  int active_pixels;
};

class LogoRepository {
public:
  explicit LogoRepository(const ManagedLogoOptions &options);

  bool find_latest(std::string *path, int *version) const;
  std::string next_path(int *version) const;
  void set_latest(const std::string &path, int version) const;

private:
  std::string directory_;
  std::string channel_key_;
};

LogoGenerationResult generate_logo(
    VideoSource &source,
    const ManagedLogoOptions &options,
    int first_frame,
    int end_frame,
    const std::string &output_path);

#endif
