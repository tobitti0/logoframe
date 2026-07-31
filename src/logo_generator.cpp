/*
 * Lightweight logo data generator for logoframe.
 *
 * The per-pixel estimation follows the method used by MakKi's AviUtl
 * "Logo analysis plugin" (logoscan): frames with a nearly uniform border
 * are selected, the border color is used as the background sample, and
 * foreground/background regression is solved in both directions.
 *
 * Original project:
 *   MakKi, "Logo analysis plugin" / "Transparent logo filter plugin"
 *   https://github.com/makiuchi-d/delogo-aviutl
 *
 * This port removes the AviUtl/Windows UI and keeps the analysis model.
 * Frame samples are accumulated as sufficient statistics instead of
 * retaining one allocation per pixel and sample.
 *
 * Copyright (C) 2003-2008 MakKi
 * Copyright (C) 2026 tobitti0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#include "logo_generator.h"

#include "logo.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#define LOGOFRAME_ACCESS _access
#define LOGOFRAME_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define LOGOFRAME_ACCESS access
#define LOGOFRAME_MKDIR(path) mkdir(path, 0755)
#endif

namespace {

const int kLogoFileNameSize = 1024;
const int kLogoDpRange = 0x3fff;

// MakKi版ScanPixelの輝度解析部分。
// 元実装のサンプル配列は、同じ回帰結果を得られる集計値に置き換える。
struct ScanPixel {
  double sum_foreground;
  double sum_background;
  double sum_foreground2;
  double sum_background2;
  double sum_foreground_background;
  int count;

  ScanPixel()
      : sum_foreground(0.0),
        sum_background(0.0),
        sum_foreground2(0.0),
        sum_background2(0.0),
        sum_foreground_background(0.0),
        count(0) {}

  int AddSample(double foreground, double background) {
    sum_foreground += foreground;
    sum_background += background;
    sum_foreground2 += foreground * foreground;
    sum_background2 += background * background;
    sum_foreground_background += foreground * background;
    return ++count;
  }
};

struct LogoPixelEstimate {
  double alpha;
  double color;
  LOGO_PIXEL pixel;

  LogoPixelEstimate() : alpha(0.0), color(0.0), pixel() {
    memset(&pixel, 0, sizeof(pixel));
  }
};

struct EdgeComponent {
  int min_x;
  int min_y;
  int max_x;
  int max_y;
  int pixels;
  long score;

  EdgeComponent()
      : min_x(0),
        min_y(0),
        max_x(0),
        max_y(0),
        pixels(0),
        score(0) {}
};

bool file_exists(const std::string &path) {
  return LOGOFRAME_ACCESS(path.c_str(), 0) == 0;
}

bool replace_file(const std::string &source,
                  const std::string &destination) {
#ifdef _WIN32
  return MoveFileExA(
      source.c_str(),
      destination.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(source.c_str(), destination.c_str()) == 0;
#endif
}

std::string join_path(const std::string &directory,
                      const std::string &name) {
  if (directory.empty() || directory == ".") {
    return directory.empty() ? name : directory + "/" + name;
  }
  const char last = directory[directory.size() - 1];
  if (last == '/' || last == '\\') {
    return directory + name;
  }
  return directory + "/" + name;
}

std::string base_name(const std::string &path) {
  const std::string::size_type slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

void ensure_directory(const std::string &directory) {
  if (directory.empty() || directory == ".") {
    return;
  }
  std::string current;
  size_t position = 0;
#ifdef _WIN32
  if (directory.size() >= 2 && directory[1] == ':') {
    current = directory.substr(0, 2);
    position = 2;
  }
#else
  if (directory[0] == '/') {
    current = "/";
    position = 1;
  }
#endif
  while (position <= directory.size()) {
    const size_t separator = directory.find_first_of("/\\", position);
    const std::string part = directory.substr(
        position,
        separator == std::string::npos
            ? std::string::npos
            : separator - position);
    if (!part.empty() && part != ".") {
      if (!current.empty() && current[current.size() - 1] != '/' &&
          current[current.size() - 1] != '\\') {
        current += "/";
      }
      current += part;
      if (!file_exists(current) &&
          LOGOFRAME_MKDIR(current.c_str()) != 0 &&
          errno != EEXIST) {
        throw std::runtime_error(
            std::string("Could not create logo directory: ") + current);
      }
    }
    if (separator == std::string::npos) {
      break;
    }
    position = separator + 1;
  }
}

std::string encode_channel_key(const std::string &channel) {
  static const char hex[] = "0123456789ABCDEF";
  std::string encoded;
  for (size_t i = 0; i < channel.size(); ++i) {
    const unsigned char c =
        static_cast<unsigned char>(channel[i]);
    const bool safe =
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.';
    if (safe) {
      encoded.push_back(static_cast<char>(c));
    } else {
      encoded.push_back('_');
      encoded.push_back(hex[c >> 4]);
      encoded.push_back(hex[c & 15]);
    }
  }
  return encoded.empty() ? "channel" : encoded;
}

std::string version_file_name(
    const std::string &channel_key,
    int version) {
  char buffer[kLogoFileNameSize];
  snprintf(
      buffer,
      sizeof(buffer),
      "%s-v%04d.lgd",
      channel_key.c_str(),
      version);
  return buffer;
}

std::string manifest_file_name(const std::string &channel_key) {
  return channel_key + ".latest";
}

std::vector<int> make_sample_frames(
    int first_frame,
    int end_frame,
    int requested) {
  const int available = std::max(0, end_frame - first_frame);
  const int count = std::min(std::max(1, requested), available);
  std::vector<int> frames;
  frames.reserve(count);
  for (int i = 0; i < count; ++i) {
    const int64_t numerator =
        static_cast<int64_t>(2 * i + 1) * available;
    int frame = first_frame +
        static_cast<int>(numerator / (2 * count));
    frame = std::max(first_frame, std::min(end_frame - 1, frame));
    if (frames.empty() || frames.back() != frame) {
      frames.push_back(frame);
    }
  }
  return frames;
}

int range_gap(int first_min, int first_max,
              int second_min, int second_max) {
  if (first_max < second_min) {
    return second_min - first_max - 1;
  }
  if (second_max < first_min) {
    return first_min - second_max - 1;
  }
  return 0;
}

bool should_merge_components(
    const EdgeComponent &first,
    const EdgeComponent &second) {
  const int gap_x = range_gap(
      first.min_x, first.max_x, second.min_x, second.max_x);
  const int gap_y = range_gap(
      first.min_y, first.max_y, second.min_y, second.max_y);
  return (gap_x <= 10 && gap_y <= 4) ||
         (gap_x <= 4 && gap_y <= 7);
}

void merge_component(
    EdgeComponent *destination,
    const EdgeComponent &source) {
  destination->min_x = std::min(destination->min_x, source.min_x);
  destination->min_y = std::min(destination->min_y, source.min_y);
  destination->max_x = std::max(destination->max_x, source.max_x);
  destination->max_y = std::max(destination->max_y, source.max_y);
  destination->pixels += source.pixels;
  destination->score += source.score;
}

LogoRect find_logo_rect(
    VideoSource &source,
    const ManagedLogoOptions &options,
    int first_frame,
    int end_frame) {
  const InputVideoInfo &info = source.info();
  if (options.has_manual_rect) {
    const LogoRect &rect = options.manual_rect;
    if (rect.x < 0 || rect.y < 0 ||
        rect.width <= 0 || rect.height <= 0 ||
        rect.x + rect.width > info.width ||
        rect.y + rect.height > info.height) {
      throw std::runtime_error(
          "The specified logo area is outside the video");
    }
    return rect;
  }

  const int step = 2;
  const int roi_x = info.width * 2 / 3;
  const int roi_y = 0;
  const int roi_width = info.width - roi_x;
  const int roi_height = std::max(1, info.height / 4);
  const int grid_width = roi_width / step;
  const int grid_height = roi_height / step;
  if (grid_width < 8 || grid_height < 8) {
    throw std::runtime_error(
        "The video is too small for automatic logo search");
  }

  const std::vector<int> frames = make_sample_frames(
      first_frame,
      end_frame,
      std::min(options.sample_frames, 240));
  std::vector<unsigned short> edge_count(
      static_cast<size_t>(grid_width) * grid_height, 0);
  for (size_t sample = 0; sample < frames.size(); ++sample) {
    const InputVideoFrame frame = source.read_frame(frames[sample]);
    for (int gy = 1; gy < grid_height - 1; ++gy) {
      const int y = roi_y + gy * step;
      const unsigned char *row = frame.data +
          static_cast<ptrdiff_t>(y) * frame.pitch;
      const unsigned char *previous = row - frame.pitch;
      const unsigned char *next = row + frame.pitch;
      for (int gx = 1; gx < grid_width - 1; ++gx) {
        const int x = roi_x + gx * step;
        const int gradient =
            abs(static_cast<int>(row[x + 1]) - row[x - 1]) +
            abs(static_cast<int>(next[x]) - previous[x]);
        if (gradient >= options.edge_threshold) {
          unsigned short &count =
              edge_count[static_cast<size_t>(gy) * grid_width + gx];
          if (count != 0xffff) {
            ++count;
          }
        }
      }
    }
  }

  const int persistent_threshold = std::max(
      5,
      static_cast<int>(
          std::ceil(frames.size() * 0.18)));
  std::vector<unsigned char> binary(edge_count.size(), 0);
  for (size_t i = 0; i < edge_count.size(); ++i) {
    binary[i] =
        edge_count[i] >= persistent_threshold ? 1 : 0;
  }

  for (int iteration = 0; iteration < 3; ++iteration) {
    std::vector<unsigned char> expanded = binary;
    for (int y = 1; y < grid_height - 1; ++y) {
      for (int x = 1; x < grid_width - 1; ++x) {
        if (!binary[static_cast<size_t>(y) * grid_width + x]) {
          continue;
        }
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            expanded[
                static_cast<size_t>(y + dy) * grid_width +
                x + dx] = 1;
          }
        }
      }
    }
    binary.swap(expanded);
  }

  std::vector<unsigned char> visited(binary.size(), 0);
  std::vector<EdgeComponent> components;
  const int directions[8][2] = {
      {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
      {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
  for (int start_y = 1; start_y < grid_height - 1; ++start_y) {
    for (int start_x = 1; start_x < grid_width - 1; ++start_x) {
      const size_t start_offset =
          static_cast<size_t>(start_y) * grid_width + start_x;
      if (!binary[start_offset] || visited[start_offset]) {
        continue;
      }
      EdgeComponent component;
      component.min_x = component.max_x = start_x;
      component.min_y = component.max_y = start_y;
      std::queue<int> pending;
      pending.push(start_y * grid_width + start_x);
      visited[start_offset] = 1;
      while (!pending.empty()) {
        const int offset = pending.front();
        pending.pop();
        const int x = offset % grid_width;
        const int y = offset / grid_width;
        component.min_x = std::min(component.min_x, x);
        component.min_y = std::min(component.min_y, y);
        component.max_x = std::max(component.max_x, x);
        component.max_y = std::max(component.max_y, y);
        if (edge_count[offset] >= persistent_threshold) {
          ++component.pixels;
          component.score += edge_count[offset];
        }
        for (int direction = 0; direction < 8; ++direction) {
          const int nx = x + directions[direction][0];
          const int ny = y + directions[direction][1];
          if (nx <= 0 || nx >= grid_width - 1 ||
              ny <= 0 || ny >= grid_height - 1) {
            continue;
          }
          const size_t next_offset =
              static_cast<size_t>(ny) * grid_width + nx;
          if (binary[next_offset] && !visited[next_offset]) {
            visited[next_offset] = 1;
            pending.push(ny * grid_width + nx);
          }
        }
      }
      const int width = component.max_x - component.min_x + 1;
      const int height = component.max_y - component.min_y + 1;
      if (component.pixels >= 3 &&
          width < grid_width * 3 / 4 &&
          height < grid_height * 3 / 4) {
        components.push_back(component);
      }
    }
  }

  bool merged = true;
  while (merged) {
    merged = false;
    for (size_t i = 0; i < components.size() && !merged; ++i) {
      for (size_t j = i + 1; j < components.size(); ++j) {
        if (should_merge_components(components[i], components[j])) {
          merge_component(&components[i], components[j]);
          components.erase(components.begin() + j);
          merged = true;
          break;
        }
      }
    }
  }

  if (components.empty()) {
    throw std::runtime_error(
        "Automatic logo position detection found no persistent edge; "
        "specify -logo-area x,y,w,h");
  }
  const EdgeComponent *best = &components[0];
  double best_score = -1.0;
  for (size_t i = 0; i < components.size(); ++i) {
    const int width = components[i].max_x -
        components[i].min_x + 1;
    const int height = components[i].max_y -
        components[i].min_y + 1;
    if (width < 3 || height < 3) {
      continue;
    }
    const double shape =
        std::sqrt(static_cast<double>(width * height));
    const double score =
        components[i].score * shape;
    if (score > best_score) {
      best_score = score;
      best = &components[i];
    }
  }

  const int margin = 6;
  int x = roi_x + best->min_x * step - margin;
  int y = roi_y + best->min_y * step - margin;
  int right = roi_x + (best->max_x + 1) * step + margin;
  int bottom = roi_y + (best->max_y + 1) * step + margin;
  x = std::max(0, x);
  y = std::max(0, y);
  right = std::min(info.width, right);
  bottom = std::min(info.height, bottom);
  if (right - x < 8 || bottom - y < 8) {
    throw std::runtime_error(
        "Automatic logo position detection produced a small area");
  }

  fprintf(
      stderr,
      "logo generator: persistent edge rect=%d,%d,%d,%d "
      "(%zu samples, threshold=%d)\n",
      x,
      y,
      right - x,
      bottom - y,
      frames.size(),
      persistent_threshold);
  return LogoRect(x, y, right - x, bottom - y);
}

bool estimate_background(
    const InputVideoFrame &frame,
    const LogoRect &rect,
    int threshold,
    int *background) {
  std::vector<int> border;
  border.reserve((rect.width + rect.height - 2) * 2);
  const unsigned char *top = frame.data +
      static_cast<ptrdiff_t>(rect.y) * frame.pitch + rect.x;
  const unsigned char *bottom = frame.data +
      static_cast<ptrdiff_t>(rect.y + rect.height - 1) *
          frame.pitch + rect.x;
  for (int x = 0; x < rect.width; ++x) {
    border.push_back(top[x]);
    border.push_back(bottom[x]);
  }
  for (int y = 1; y < rect.height - 1; ++y) {
    const unsigned char *row = frame.data +
        static_cast<ptrdiff_t>(rect.y + y) * frame.pitch + rect.x;
    border.push_back(row[0]);
    border.push_back(row[rect.width - 1]);
  }
  if (border.size() < 8) {
    return false;
  }
  std::sort(border.begin(), border.end());
  if (border.back() - border.front() > threshold) {
    return false;
  }
  const size_t first = border.size() / 4;
  const size_t last = border.size() - border.size() / 4;
  long sum = 0;
  for (size_t i = first; i < last; ++i) {
    sum += border[i];
  }
  *background = static_cast<int>(
      (sum + static_cast<long>((last - first) / 2)) /
      static_cast<long>(last - first));
  return true;
}

void add_frame_statistics(
    const InputVideoFrame &frame,
    const LogoRect &rect,
    int background,
    std::vector<ScanPixel> *statistics) {
  size_t offset = 0;
  for (int y = 0; y < rect.height; ++y) {
    const unsigned char *row = frame.data +
        static_cast<ptrdiff_t>(rect.y + y) * frame.pitch + rect.x;
    for (int x = 0; x < rect.width; ++x, ++offset) {
      (*statistics)[offset].AddSample(row[x], background);
    }
  }
}

bool approximate_line(
    int count,
    double sum_x,
    double sum_y,
    double sum_x2,
    double sum_xy,
    double *a,
    double *b) {
  const double denominator =
      count * sum_x2 - sum_x * sum_x;
  if (std::abs(denominator) < 1.0e-9) {
    return false;
  }
  *a = (count * sum_xy - sum_x * sum_y) / denominator;
  *b = (sum_x2 * sum_y - sum_x * sum_xy) / denominator;
  return std::isfinite(*a) && std::isfinite(*b);
}

short limited_luma_to_logo_y(double value) {
  const double converted =
      (value - 16.0) * 4096.0 / 219.0;
  const long rounded = lround(converted);
  return static_cast<short>(
      std::max<long>(-32768, std::min<long>(32767, rounded)));
}

LogoPixelEstimate estimate_pixel(
    const ScanPixel &statistics) {
  LogoPixelEstimate result;
  if (statistics.count <= 1) {
    return result;
  }
  double a1 = 0.0;
  double b1 = 0.0;
  double a2 = 0.0;
  double b2 = 0.0;
  if (!approximate_line(
          statistics.count,
          statistics.sum_background,
          statistics.sum_foreground,
          statistics.sum_background2,
          statistics.sum_foreground_background,
          &a1,
          &b1) ||
      !approximate_line(
          statistics.count,
          statistics.sum_foreground,
          statistics.sum_background,
          statistics.sum_foreground2,
          statistics.sum_foreground_background,
          &a2,
          &b2) ||
      std::abs(a2) < 1.0e-9) {
    return result;
  }

  const double a = (a1 + 1.0 / a2) / 2.0;
  const double b = (b1 - b2 / a2) / 2.0;
  const double alpha = 1.0 - a;
  if (!std::isfinite(alpha) || !std::isfinite(b) ||
      std::abs(alpha) < 1.0e-9) {
    return result;
  }
  const double color = b / alpha;
  const long dp = lround(alpha * LOGO_MAX_DP);
  if (!std::isfinite(color) ||
      std::abs(dp) > kLogoDpRange ||
      dp == 0) {
    return result;
  }

  result.alpha = alpha;
  result.color = color;
  result.pixel.dp_y = static_cast<short>(dp);
  result.pixel.y = limited_luma_to_logo_y(color);
  result.pixel.dp_cb = 0;
  result.pixel.cb = 0;
  result.pixel.dp_cr = 0;
  result.pixel.cr = 0;
  return result;
}

std::vector<LogoPixelEstimate> estimate_logo(
    const std::vector<ScanPixel> &statistics,
    int *active_pixels) {
  std::vector<LogoPixelEstimate> estimates(statistics.size());
  *active_pixels = 0;
  for (size_t i = 0; i < statistics.size(); ++i) {
    estimates[i] = estimate_pixel(statistics[i]);
    if (estimates[i].pixel.dp_y >= 50 &&
        estimates[i].pixel.dp_y <= 1400) {
      ++*active_pixels;
    }
  }
  return estimates;
}

bool logo_is_present(
    const InputVideoFrame &frame,
    const LogoRect &rect,
    int background,
    const std::vector<LogoPixelEstimate> &logo,
    double *fade) {
  double dot = 0.0;
  double logo_energy = 0.0;
  double no_logo_error = 0.0;
  int active = 0;
  size_t offset = 0;
  for (int y = 0; y < rect.height; ++y) {
    const unsigned char *row = frame.data +
        static_cast<ptrdiff_t>(rect.y + y) * frame.pitch + rect.x;
    for (int x = 0; x < rect.width; ++x, ++offset) {
      const LogoPixelEstimate &pixel = logo[offset];
      if (pixel.alpha < 0.05 || pixel.alpha > 1.4 ||
          pixel.color < -64.0 || pixel.color > 320.0) {
        continue;
      }
      const double logo_delta =
          pixel.alpha * (pixel.color - background);
      if (std::abs(logo_delta) < 1.0) {
        continue;
      }
      const double observed_delta = row[x] - background;
      dot += logo_delta * observed_delta;
      logo_energy += logo_delta * logo_delta;
      no_logo_error += observed_delta * observed_delta;
      ++active;
    }
  }
  if (active < 8 || logo_energy < 1.0) {
    return false;
  }
  *fade = dot / logo_energy;
  double logo_error = 0.0;
  offset = 0;
  for (int y = 0; y < rect.height; ++y) {
    const unsigned char *row = frame.data +
        static_cast<ptrdiff_t>(rect.y + y) * frame.pitch + rect.x;
    for (int x = 0; x < rect.width; ++x, ++offset) {
      const LogoPixelEstimate &pixel = logo[offset];
      if (pixel.alpha < 0.05 || pixel.alpha > 1.4 ||
          pixel.color < -64.0 || pixel.color > 320.0) {
        continue;
      }
      const double logo_delta =
          pixel.alpha * (pixel.color - background);
      if (std::abs(logo_delta) < 1.0) {
        continue;
      }
      const double difference =
          (row[x] - background) - *fade * logo_delta;
      logo_error += difference * difference;
    }
  }
  return *fade >= 0.65 && *fade <= 1.5 &&
         logo_error < no_logo_error * 0.88;
}

void write_logo_file(
    const std::string &path,
    const std::string &channel,
    const LogoRect &rect,
    const std::vector<LogoPixelEstimate> &logo) {
  const std::string temporary = path + ".tmp";
  FILE *file = fopen(temporary.c_str(), "wb");
  if (file == NULL) {
    throw std::runtime_error(
        std::string("Could not create logo file: ") + temporary);
  }
  bool success = false;
  do {
    LOGO_FILE_HEADER file_header;
    memset(&file_header, 0, sizeof(file_header));
    memcpy(
        file_header.str,
        LOGO_FILE_HEADER_STR,
        LOGO_FILE_HEADER_STR_SIZE);
    file_header.logonum.c[0] = 0;
    file_header.logonum.c[1] = 0;
    file_header.logonum.c[2] = 0;
    file_header.logonum.c[3] = 1;

    LOGO_HEADER logo_header;
    memset(&logo_header, 0, sizeof(logo_header));
    strncpy(
        logo_header.name,
        channel.c_str(),
        sizeof(logo_header.name) - 1);
    logo_header.x = static_cast<short>(rect.x);
    logo_header.y = static_cast<short>(rect.y);
    logo_header.h = static_cast<short>(rect.height);
    logo_header.w = static_cast<short>(rect.width);

    if (fwrite(&file_header, sizeof(file_header), 1, file) != 1 ||
        fwrite(&logo_header, sizeof(logo_header), 1, file) != 1) {
      break;
    }
    for (size_t i = 0; i < logo.size(); ++i) {
      if (fwrite(&logo[i].pixel, sizeof(LOGO_PIXEL), 1, file) != 1) {
        break;
      }
      if (i + 1 == logo.size()) {
        success = true;
      }
    }
  } while (false);
  if (fclose(file) != 0) {
    success = false;
  }
  if (!success) {
    remove(temporary.c_str());
    throw std::runtime_error(
        std::string("Could not write logo file: ") + path);
  }
  if (!replace_file(temporary, path)) {
    remove(temporary.c_str());
    throw std::runtime_error(
        std::string("Could not install logo file: ") + path);
  }
}

}  // namespace

ManagedLogoOptions::ManagedLogoOptions()
    : channel(),
      directory("logo"),
      sample_frames(600),
      background_threshold(12),
      edge_threshold(40),
      match_threshold_percent(10),
      has_manual_rect(false),
      manual_rect() {}

LogoRepository::LogoRepository(const ManagedLogoOptions &options)
    : directory_(options.directory),
      channel_key_(encode_channel_key(options.channel)) {}

bool LogoRepository::find_latest(
    std::string *path,
    int *version) const {
  const std::string manifest = join_path(
      directory_, manifest_file_name(channel_key_));
  FILE *file = fopen(manifest.c_str(), "r");
  if (file != NULL) {
    int stored_version = 0;
    char stored_name[kLogoFileNameSize] = {0};
    const int read = fscanf(
        file, "%d\n%1023[^\r\n]", &stored_version, stored_name);
    fclose(file);
    if (read == 2 && stored_version > 0) {
      const std::string candidate =
          join_path(directory_, base_name(stored_name));
      if (file_exists(candidate)) {
        *path = candidate;
        *version = stored_version;
        return true;
      }
    }
  }

  int latest = 0;
  for (int candidate = 1; candidate <= 9999; ++candidate) {
    const std::string candidate_path = join_path(
        directory_, version_file_name(channel_key_, candidate));
    if (!file_exists(candidate_path)) {
      break;
    }
    latest = candidate;
  }
  if (latest == 0) {
    return false;
  }
  *path = join_path(
      directory_, version_file_name(channel_key_, latest));
  *version = latest;
  return true;
}

std::string LogoRepository::next_path(int *version) const {
  ensure_directory(directory_);
  std::string current_path;
  int current_version = 0;
  find_latest(&current_path, &current_version);
  int next = current_version + 1;
  while (next <= 9999 &&
         file_exists(join_path(
             directory_, version_file_name(channel_key_, next)))) {
    ++next;
  }
  if (next > 9999) {
    throw std::runtime_error(
        "Logo version limit reached for this channel");
  }
  *version = next;
  return join_path(
      directory_, version_file_name(channel_key_, next));
}

void LogoRepository::set_latest(
    const std::string &path,
    int version) const {
  ensure_directory(directory_);
  const std::string manifest = join_path(
      directory_, manifest_file_name(channel_key_));
  const std::string temporary = manifest + ".tmp";
  FILE *file = fopen(temporary.c_str(), "wb");
  if (file == NULL) {
    throw std::runtime_error(
        std::string("Could not create logo manifest: ") + temporary);
  }
  const int written = fprintf(
      file, "%d\n%s\n", version, base_name(path).c_str());
  const bool closed = fclose(file) == 0;
  if (written <= 0 || !closed ||
      !replace_file(temporary, manifest)) {
    remove(temporary.c_str());
    throw std::runtime_error(
        std::string("Could not update logo manifest: ") + manifest);
  }
}

LogoGenerationResult generate_logo(
    VideoSource &source,
    const ManagedLogoOptions &options,
    int first_frame,
    int end_frame,
    const std::string &output_path) {
  if (options.channel.empty()) {
    throw std::runtime_error(
        "A channel name is required to generate a logo");
  }
  if (first_frame < 0 || end_frame <= first_frame ||
      end_frame > source.info().num_frames) {
    throw std::runtime_error(
        "The logo generation frame range is invalid");
  }

  LogoGenerationResult result;
  result.rect = find_logo_rect(
      source, options, first_frame, end_frame);
  const std::vector<int> frames = make_sample_frames(
      first_frame, end_frame, options.sample_frames);
  result.sampled_frames = static_cast<int>(frames.size());
  result.uniform_frames = 0;
  result.logo_frames = 0;
  result.active_pixels = 0;

  const size_t pixel_count =
      static_cast<size_t>(result.rect.width) * result.rect.height;
  std::vector<ScanPixel> initial_statistics(pixel_count);
  std::vector<int> uniform_frames;
  uniform_frames.reserve(frames.size());
  for (size_t i = 0; i < frames.size(); ++i) {
    const InputVideoFrame frame = source.read_frame(frames[i]);
    int background = 0;
    if (!estimate_background(
            frame,
            result.rect,
            options.background_threshold,
            &background)) {
      continue;
    }
    add_frame_statistics(
        frame, result.rect, background, &initial_statistics);
    uniform_frames.push_back(frames[i]);
  }
  result.uniform_frames =
      static_cast<int>(uniform_frames.size());
  if (uniform_frames.size() < 8) {
    throw std::runtime_error(
        "Insufficient frames with a uniform background around the logo");
  }

  int initial_active_pixels = 0;
  const std::vector<LogoPixelEstimate> initial_logo =
      estimate_logo(initial_statistics, &initial_active_pixels);
  if (initial_active_pixels < 8) {
    throw std::runtime_error(
        "The initial logo estimate contains too few active pixels");
  }

  std::vector<ScanPixel> final_statistics(pixel_count);
  for (size_t i = 0; i < uniform_frames.size(); ++i) {
    const InputVideoFrame frame =
        source.read_frame(uniform_frames[i]);
    int background = 0;
    if (!estimate_background(
            frame,
            result.rect,
            options.background_threshold,
            &background)) {
      continue;
    }
    double fade = 0.0;
    if (!logo_is_present(
            frame,
            result.rect,
            background,
            initial_logo,
            &fade)) {
      continue;
    }
    add_frame_statistics(
        frame, result.rect, background, &final_statistics);
    ++result.logo_frames;
  }
  if (result.logo_frames < 8) {
    throw std::runtime_error(
        "Insufficient full-strength logo frames after the second pass");
  }

  const std::vector<LogoPixelEstimate> final_logo =
      estimate_logo(final_statistics, &result.active_pixels);
  if (result.active_pixels < 8) {
    throw std::runtime_error(
        "The generated logo contains too few active pixels");
  }

  write_logo_file(
      output_path,
      options.channel,
      result.rect,
      final_logo);
  fprintf(
      stderr,
      "logo generator: wrote %s rect=%d,%d,%d,%d "
      "samples=%d uniform=%d logo=%d active=%d\n",
      output_path.c_str(),
      result.rect.x,
      result.rect.y,
      result.rect.width,
      result.rect.height,
      result.sampled_frames,
      result.uniform_frames,
      result.logo_frames,
      result.active_pixels);
  return result;
}
