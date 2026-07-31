//
// logoframe by Yobi
//
//  AvisynthがGPLなので、このソフトもGPLにします。
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>
#include "logo.h"
#include "logo_generator.h"
#include "logoset.h"
#include "logoset_mul.h"
#include "video_source.h"

#include <exception>
#include <memory>

#ifndef _WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
typedef unsigned char BYTE;
#else
#define strnicmp _strnicmp
#endif

// logo function
extern void MultLogoInit(MLOGO_DATASET* pml);
extern void MultLogoFree(MLOGO_DATASET* pml);
extern int  MultLogoOptionAdd(MLOGO_DATASET* pml, const char* strcmd, const char* strval);
extern int  MultLogoOptionFile(MLOGO_DATASET* pml, const char* fname);
extern int  MultLogoOptionOrgFile(MLOGO_DATASET* pml);
extern int  MultLogoSetup(MLOGO_DATASET* pml, int num_frames);
extern void MultLogoDisplayParam(MLOGO_DATASET* pml);
extern void MultLogoCalc(MLOGO_DATASET* pml, const BYTE *data, int pitch, int nframe, int height);
extern void MultLogoFind(MLOGO_DATASET* pml);
extern int  MultLogoWrite(MLOGO_DATASET* pml);


#ifndef INT_MAX
#define INT_MAX 0x7fffffff
#endif

#define MY_VERSION "logoframe 1.21"
#define AVS_BUFSIZE 128*1024
//#define FILE_BUFSIZE 512

struct DetectionResult {
  int setup_error;
  double match_ratio;

  DetectionResult() : setup_error(0), match_ratio(0.0) {}
};

static bool option_needs_value(const char *option)
{
  return !stricmp(option, "-F") ||
      !stricmp(option, "-seek") ||
      !stricmp(option, "-frames") ||
      !stricmp(option, "-csp") ||
      !stricmp(option, "-channel") ||
      !stricmp(option, "-logo-dir") ||
      !stricmp(option, "-logo-area") ||
      !stricmp(option, "-logo-samples") ||
      !stricmp(option, "-logo-bg-threshold") ||
      !stricmp(option, "-logo-edge-threshold") ||
      !stricmp(option, "-logo-match");
}

static bool is_managed_logo_option(const char *option)
{
  return !stricmp(option, "-channel") ||
      !stricmp(option, "-logo-dir") ||
      !stricmp(option, "-logo-area") ||
      !stricmp(option, "-logo-samples") ||
      !stricmp(option, "-logo-bg-threshold") ||
      !stricmp(option, "-logo-edge-threshold") ||
      !stricmp(option, "-logo-match");
}

static bool is_general_option(const char *option)
{
  return !stricmp(option, "-F") ||
      !stricmp(option, "-nodisp") ||
      !stricmp(option, "-errforce") ||
      !stricmp(option, "-h") ||
      !stricmp(option, "-seek") ||
      !stricmp(option, "-frames") ||
      !stricmp(option, "-csp");
}

static bool is_explicit_logo_option(const char *option)
{
  if (!stricmp(option, "-logo")) {
    return true;
  }
  if (strnicmp(option, "-logo", 5) != 0 || option[5] == 0) {
    return false;
  }
  for (int i = 5; option[i] != 0; ++i) {
    if (option[i] < '0' || option[i] > '9') {
      return false;
    }
  }
  return true;
}

static bool parse_logo_rect(const char *value, LogoRect *rect)
{
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  char tail = 0;
  if (sscanf(value, "%d,%d,%d,%d%c",
          &x, &y, &width, &height, &tail) != 4 ||
      x < 0 || y < 0 || width <= 0 || height <= 0) {
    return false;
  }
  *rect = LogoRect(x, y, width, height);
  return true;
}

static void clear_logo_file_options(MLOGO_DATASET *logodata)
{
  if (logodata->opt_logofilename != NULL) {
    free(logodata->opt_logofilename);
    logodata->opt_logofilename = NULL;
  }
  for (int i = 0; i < LOGONUM_MAX; ++i) {
    if (logodata->all_logofilename[i] != NULL) {
      free(logodata->all_logofilename[i]);
      logodata->all_logofilename[i] = NULL;
    }
  }
}

static int set_managed_logo(
    MLOGO_DATASET *logodata,
    const std::string &path)
{
  clear_logo_file_options(logodata);
  return MultLogoOptionAdd(logodata, "-logo", path.c_str()) < 0
      ? 2
      : 0;
}

static int reload_logo_options(
    MLOGO_DATASET *logodata,
    int argc,
    const char *argv[],
    const std::string &managed_logo_path)
{
  MultLogoFree(logodata);
  MultLogoInit(logodata);
  MultLogoOptionOrgFile(logodata);

  for (int i = 1; i < argc; ++i) {
    if (!stricmp(argv[i], "-F") && i + 1 < argc) {
      if (MultLogoOptionFile(logodata, argv[i + 1]) < 0) {
        return 2;
      }
      ++i;
    }
  }
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] != '-' || argv[i][1] == 0) {
      continue;
    }
    if (is_managed_logo_option(argv[i]) ||
        is_general_option(argv[i])) {
      if (option_needs_value(argv[i]) && i + 1 < argc) {
        ++i;
      }
      continue;
    }
    const char *value = i + 1 < argc ? argv[i + 1] : NULL;
    const int consumed = MultLogoOptionAdd(logodata, argv[i], value);
    if (consumed < 0) {
      return 2;
    }
    if (consumed > 0) {
      i += consumed - 1;
    }
  }
  return set_managed_logo(logodata, managed_logo_path);
}

static DetectionResult detect_logo(
    MLOGO_DATASET *logodata,
    VideoSource &source,
    int seek,
    int end,
    int height,
    int nodisp)
{
  DetectionResult result;
  const InputVideoInfo &info = source.info();
  result.setup_error = MultLogoSetup(logodata, info.num_frames);
  if (result.setup_error != 0) {
    return result;
  }

  if (nodisp == 0 && logodata->paramoff == 0) {
    MultLogoDisplayParam(logodata);
  }
  if (nodisp == 0) {
    fprintf(stderr, "Total logodata : %d\n", logodata->num_deflogo);
    fprintf(stderr, "checking %6d/%d start.\n", seek, end - 1);
  }
  for (int frame_number = seek; frame_number < end; ++frame_number) {
    InputVideoFrame frame = source.read_frame(frame_number);
    MultLogoCalc(
        logodata,
        frame.data,
        frame.pitch,
        frame_number,
        height);
    if (nodisp == 0 &&
        (((frame_number % 5000 == 0) && frame_number != 0) ||
         frame_number == 100 ||
         frame_number == 1000)) {
      fprintf(
          stderr,
          "checking %6d/%d ended.\n",
          frame_number,
          end - 1);
    }
  }
  if (nodisp == 0) {
    fprintf(stderr, "checking %6d/%d ended.\n", end - 1, end - 1);
  }

  MultLogoFind(logodata);
  const int checked_frames = std::max(1, end - seek);
  if (logodata->num_deflogo > 0) {
    result.match_ratio =
        logodata->total_frame[0] /
        static_cast<double>(checked_frames);
  }
  return result;
}

static int csp_to_int(const char *arg)
{
  if(!stricmp(arg, "i420"))
    return CSP_I420;
  if(!stricmp(arg, "i422"))
    return CSP_I422;
  if(!stricmp(arg, "i444"))
    return CSP_I444;
  return 0;
}

int main(int argc, const char* argv[])
{
  fprintf(stderr,
      MY_VERSION "\n"
      "  Input : AviSynth=%s, dtvindex=%s\n\n",
      HAVE_AVISYNTH ? "enabled" : "disabled",
      HAVE_DTVINDEX ? "enabled" : "disabled");
  fflush(stderr);

  const char* infile   = NULL;
  int err_count = 0;
  int errforce = 0;
  int nopt_fromini = 0;
  int nopt_fromfile = 0;
  int nodisp = 0;
  int usage = 0;
  int seek = 0;
  int end = 0;
  int interlaced = 0;
  int tff = 0;
  int csp = CSP_I420;
  bool explicit_logo = false;
  ManagedLogoOptions managed_logo;
  // logo data
  MLOGO_DATASET logodata;     // for LOGO


  //--- initialize ---
  MultLogoInit( &logodata );  // for LOGO

  //--- read options from .ini file ---
  nopt_fromini = MultLogoOptionOrgFile( &logodata );  // for LOGO
  if (nopt_fromini < 0){
    nopt_fromini = 0;
  }
  //--- read options from file by "-F" ---
  for(int i = 1; i < argc; i++) {
    if (!stricmp(argv[i], "-F")){
      if(i > argc-2) {
        fprintf(stderr, "-F needs an argument\n");
        return 2;
      }
      int nopt = MultLogoOptionFile( &logodata, argv[++i] );  // for LOGO
      if (nopt >= 0){
        nopt_fromfile += nopt;
      }
      else{
        fprintf(stderr, "warning:not found '%s'\n", argv[i]);
        err_count ++;
      }
    }
  }
  //--- options ---
  for(int i = 1; i < argc; i++) {
    if(argv[i][0] == '-' && argv[i][1] != 0) {
      const char* strval;
      int   ncnt;

      if (i > argc-2){
        strval = NULL;
      }
      else{
        strval = argv[i+1];
      }
      if (!stricmp(argv[i], "-channel")) {
        if (strval == NULL) {
          fprintf(stderr, "-channel needs an argument\n");
          return 2;
        }
        managed_logo.channel = strval;
        ++i;
        continue;
      } else if (!stricmp(argv[i], "-logo-dir")) {
        if (strval == NULL) {
          fprintf(stderr, "-logo-dir needs an argument\n");
          return 2;
        }
        managed_logo.directory = strval;
        ++i;
        continue;
      } else if (!stricmp(argv[i], "-logo-area")) {
        if (strval == NULL ||
            !parse_logo_rect(strval, &managed_logo.manual_rect)) {
          fprintf(
              stderr,
              "-logo-area needs x,y,width,height\n");
          return 2;
        }
        managed_logo.has_manual_rect = true;
        ++i;
        continue;
      } else if (!stricmp(argv[i], "-logo-samples")) {
        if (strval == NULL ||
            (managed_logo.sample_frames = atoi(strval)) < 8) {
          fprintf(stderr, "-logo-samples must be 8 or greater\n");
          return 2;
        }
        ++i;
        continue;
      } else if (!stricmp(argv[i], "-logo-bg-threshold")) {
        if (strval == NULL ||
            (managed_logo.background_threshold = atoi(strval)) < 1 ||
            managed_logo.background_threshold > 255) {
          fprintf(
              stderr,
              "-logo-bg-threshold must be between 1 and 255\n");
          return 2;
        }
        ++i;
        continue;
      } else if (!stricmp(argv[i], "-logo-edge-threshold")) {
        if (strval == NULL ||
            (managed_logo.edge_threshold = atoi(strval)) < 1 ||
            managed_logo.edge_threshold > 510) {
          fprintf(
              stderr,
              "-logo-edge-threshold must be between 1 and 510\n");
          return 2;
        }
        ++i;
        continue;
      } else if (!stricmp(argv[i], "-logo-match")) {
        if (strval == NULL ||
            (managed_logo.match_threshold_percent = atoi(strval)) < 0 ||
            managed_logo.match_threshold_percent > 100) {
          fprintf(
              stderr,
              "-logo-match must be between 0 and 100\n");
          return 2;
        }
        ++i;
        continue;
      }
      if (is_explicit_logo_option(argv[i])) {
        explicit_logo = true;
      }
      ncnt = MultLogoOptionAdd( &logodata, argv[i], strval );  // for LOGO
      if (ncnt > 0){
        i += (ncnt - 1);
      } else if (ncnt < 0){
        return 2;
      } else if (!stricmp(argv[i], "-F")) {
        i ++;
      } else if (!stricmp(argv[i], "-nodisp")) {
        nodisp = 0;
      } else if(!stricmp(argv[i], "-errforce")) {
        errforce = 1;
      } else if (!stricmp(argv[i], "-h")) {
        usage = 1;
      } else if(!stricmp(argv[i], "-seek")) {
        if(i > argc-2) {
          fprintf(stderr, "-seek needs an argument\n");
          return 2;
        }
        seek = atoi(argv[++i]);
        if(seek < 0) usage = 1;
      } else if(!stricmp(argv[i], "-frames")) {
        if(i > argc-2) {
          fprintf(stderr, "-frames needs an argument\n");
          return 2;
        }
        end = atoi(argv[++i]);
      } else if(!stricmp(argv[i], "-csp")) {
        if(i > argc-2) {
          fprintf(stderr, "-csp needs an argument\n");
          return 2;
        }
        csp = csp_to_int(argv[++i]);
        if(!csp) {
          fprintf(stderr, "-csp '%s' is unknown\n", argv[i]);
          return 2;
        }
      } else {
        fprintf(stderr, "Warning: no such option: %s\n", argv[i]);
        err_count ++;
        if (i <= argc-2){
          const char *dot = strrchr(argv[i+1], '.');
          if (!dot){
            dot = "";
          }
          if (argv[i+1][0] != '-' && strcmp(".avs", dot)){
            i ++;
          }
        }
      }
    } else if(!infile) {
      infile = argv[i];
    } else {
      fprintf(stderr, "too many input files %s\n", argv[i]);
      return 2;
    }
  }

  if(usage || !infile){
    fprintf(stderr,
        "Usage: logoframe [options] input_video_or_avs [-logo data.lgd | -channel name] [-oa outa.txt] [-o out.avs]\n"
        "Managed logo options:\n"
        "  -channel name             use or generate the latest channel logo\n"
        "  -logo-dir directory       logo repository directory [logo]\n"
        "  -logo-area x,y,w,h        override automatic top-right search\n"
        "  -logo-samples number      generation sample count [600]\n"
        "  -logo-bg-threshold value  uniform background threshold [12]\n"
        "  -logo-edge-threshold value persistent edge threshold [40]\n"
        "  -logo-match percent       regenerate below this match rate [10]\n"
        );
    return 2;
  }
  if (!managed_logo.channel.empty() && explicit_logo) {
    fprintf(
        stderr,
        "-channel cannot be combined with -logo or -logoN\n");
    return 2;
  }
  if (err_count > 0 && errforce > 0){
    fprintf(stderr, "Forced Exit (error found in arguments)\n");
    return 2;
  }
  if (logodata.dispoff > 0){  // for LOGO
    nodisp = 1;
  }
  if (nopt_fromini > 0 && nodisp == 0){
    fprintf(stderr, "Info: read %d options from logoframe.ini\n", nopt_fromini);
  }
  if (nopt_fromfile > 0 && nodisp == 0){
    fprintf(stderr, "Info: read %d options from -F file\n", nopt_fromfile);
  }


  int retval = 1;
  try {
    std::unique_ptr<VideoSource> source =
        open_video_source(infile, csp);
    const InputVideoInfo &inf = source->info();
    interlaced = inf.interlaced ? 1 : 0;
    tff = inf.top_field_first ? 1 : 0;

    fprintf(stderr, "%s: %dx%d, ", infile, inf.width, inf.height);
    if(inf.fps_denominator == 1)
      fprintf(stderr, "%d fps, ", inf.fps_numerator);
    else
      fprintf(stderr, "%d/%d fps, ", inf.fps_numerator, inf.fps_denominator);
    fprintf(stderr, "%d frames [%s]\n",
        inf.num_frames, source->source_name());

    const char *interlace_type = interlaced ? tff ? "t" : "b" : "p";
    const char *csp_type = csp == CSP_I444 ? "444" :
      csp == CSP_I422 ? "422" :
      "420mpeg2";
    if (nodisp == 0){
      fprintf(stderr, "YUV4MPEG2 W%d H%d F%u:%u I%s A0:0 C%s\n",
          inf.width, inf.height, inf.fps_numerator, inf.fps_denominator,
          interlace_type, csp_type);
    }

    end += seek;
    if(end <= seek || end > inf.num_frames)
      end = inf.num_frames;

    bool loaded_existing_logo = false;
    std::string managed_logo_path;
    int managed_logo_version = 0;
    std::unique_ptr<LogoRepository> logo_repository;
    if (!managed_logo.channel.empty()) {
      logo_repository.reset(new LogoRepository(managed_logo));
      loaded_existing_logo = logo_repository->find_latest(
          &managed_logo_path, &managed_logo_version);
      if (loaded_existing_logo) {
        fprintf(
            stderr,
            "managed logo: channel=%s latest=v%04d %s\n",
            managed_logo.channel.c_str(),
            managed_logo_version,
            managed_logo_path.c_str());
      } else {
        managed_logo_path =
            logo_repository->next_path(&managed_logo_version);
        fprintf(
            stderr,
            "managed logo: channel=%s has no logo; generating v%04d\n",
            managed_logo.channel.c_str(),
            managed_logo_version);
        generate_logo(
            *source,
            managed_logo,
            seek,
            end,
            managed_logo_path);
        logo_repository->set_latest(
            managed_logo_path, managed_logo_version);
      }
      if (set_managed_logo(&logodata, managed_logo_path) != 0) {
        throw std::runtime_error(
            "Could not select the managed logo");
      }
    }

    //--------------- start logoframe from here ---------------
    DetectionResult detection = detect_logo(
        &logodata, *source, seek, end, inf.height, nodisp);
    if (detection.setup_error != 0) {
      retval = detection.setup_error == 3 ? 0 : 1;
      MultLogoFree(&logodata);
      return retval;
    }

    if (!managed_logo.channel.empty()) {
      fprintf(
          stderr,
          "managed logo: v%04d match=%.2f%% threshold=%d%%\n",
          managed_logo_version,
          detection.match_ratio * 100.0,
          managed_logo.match_threshold_percent);
      if (loaded_existing_logo &&
          detection.match_ratio * 100.0 <
              managed_logo.match_threshold_percent) {
        const std::string previous_logo_path = managed_logo_path;
        const int previous_logo_version = managed_logo_version;
        const double previous_match_ratio = detection.match_ratio;
        int candidate_version = 0;
        const std::string candidate_path =
            logo_repository->next_path(&candidate_version);
        fprintf(
            stderr,
            "managed logo: match is low; generating and retrying v%04d\n",
            candidate_version);
        DetectionResult candidate_detection;
        bool candidate_analyzed = false;
        try {
          generate_logo(
              *source,
              managed_logo,
              seek,
              end,
              candidate_path);
          if (reload_logo_options(
                  &logodata,
                  argc,
                  argv,
                  candidate_path) != 0) {
            throw std::runtime_error(
                "Could not reload options for the generated logo");
          }
          candidate_detection = detect_logo(
              &logodata, *source, seek, end, inf.height, nodisp);
          if (candidate_detection.setup_error != 0) {
            throw std::runtime_error(
                "Could not analyze the regenerated logo");
          }
          candidate_analyzed = true;
          fprintf(
              stderr,
              "managed logo: v%04d retry match=%.2f%%\n",
              candidate_version,
              candidate_detection.match_ratio * 100.0);
        } catch (const std::exception &error) {
          fprintf(
              stderr,
              "managed logo: v%04d generation/retry failed: %s\n",
              candidate_version,
              error.what());
        }

        // 新しい候補で一致率が改善した場合だけ最新版に昇格する。
        if (candidate_analyzed &&
            candidate_detection.match_ratio >
                previous_match_ratio + 0.001) {
          logo_repository->set_latest(
              candidate_path, candidate_version);
          managed_logo_path = candidate_path;
          managed_logo_version = candidate_version;
          detection = candidate_detection;
          fprintf(
              stderr,
              "managed logo: promoted v%04d as latest\n",
              managed_logo_version);
        } else {
          logo_repository->set_latest(
              previous_logo_path, previous_logo_version);
          if (candidate_analyzed) {
            fprintf(
                stderr,
                "managed logo: v%04d did not improve the match; "
                "keeping v%04d as latest\n",
                candidate_version,
                previous_logo_version);
          } else {
            fprintf(
                stderr,
                "managed logo: keeping v%04d as latest\n",
                previous_logo_version);
          }
          if (reload_logo_options(
                  &logodata,
                  argc,
                  argv,
                  previous_logo_path) != 0) {
            throw std::runtime_error(
                "Could not restore options for the previous logo");
          }
          detection = detect_logo(
              &logodata, *source, seek, end, inf.height, nodisp);
          if (detection.setup_error != 0) {
            throw std::runtime_error(
                "Could not restore analysis with the previous logo");
          }
          managed_logo_path = previous_logo_path;
          managed_logo_version = previous_logo_version;
        }
      }
    }

    // output result
    MultLogoWrite(&logodata);

    //--------------- end logoframe ---------------
    retval = 0;
  } catch (const std::exception &error) {
    fprintf(stderr, "error: %s\n", error.what());
  }

  MultLogoFree( &logodata );     // for LOGO
  return retval;
}
