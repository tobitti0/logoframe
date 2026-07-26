//
// logoframe by Yobi
//
//  AvisynthがGPLなので、このソフトもGPLにします。
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

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
#include "logoset.h"
#include "logoset_mul.h"
#include "video_source.h"

#include <exception>
#include <memory>

#ifndef _WIN32
#define stricmp strcasecmp
typedef unsigned char BYTE;
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

#define MY_VERSION "logoframe 1.20"
#define AVS_BUFSIZE 128*1024
//#define FILE_BUFSIZE 512

static int csp_to_int(const char *arg)
{
  if(!strcasecmp(arg, "i420"))
    return CSP_I420;
  if(!strcasecmp(arg, "i422"))
    return CSP_I422;
  if(!strcasecmp(arg, "i444"))
    return CSP_I444;
  return 0;
}

const AVS_Linkage *AVS_linkage = nullptr;

int main(int argc, const char* argv[])
{
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
    fprintf(stderr, MY_VERSION "\n"
        "Usage: logoframe [options] input_video_or_avs [-logo data.lgd] [-oa outa.txt] [-o out.avs]\n"
        );
    return 2;
  }
  if (err_count > 0 && errforce > 0){
    fprintf(stderr, MY_VERSION "\n"
        "Forced Exit (error found in arguments)\n"
        );
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


    //--------------- start logoframe from here ---------------
    int errnum;
    int height;

    // for checking if logodata area is out of image data
    height = inf.height;

    // setup for logo detection
    errnum = MultLogoSetup( &logodata, inf.num_frames );  // for LOGO
    if (errnum != 0){
      retval = errnum == 3 ? 0 : 1;    // no logo definition found is success
      MultLogoFree( &logodata );
      return retval;
    }

    // display parameter
    if (nodisp == 0 && logodata.paramoff == 0){   // for LOGO
      MultLogoDisplayParam( &logodata );        // for LOGO
    }
    if (nodisp == 0){
      fprintf(stderr, "Total logodata : %d\n", logodata.num_deflogo);  // for LOGO
    }

    // read framedata
    if (nodisp == 0){
      fprintf(stderr, "checking %6d/%d start.\n", seek, end-1);
    }
    for(int frm = seek; frm < end; ++frm) {
      InputVideoFrame frame = source->read_frame(frm);

      // Existing logo detection receives the same luma-plane interface.
      MultLogoCalc( &logodata, frame.data, frame.pitch, frm, height );

      if (nodisp == 0){
        if (((frm % 5000 == 0) && (frm != 0)) ||
            (frm == 100) ||
            (frm == 1000)){
          fprintf(stderr, "checking %6d/%d ended.\n", frm, end-1);
        }
      }
    }
    if (nodisp == 0){
      fprintf(stderr, "checking %6d/%d ended.\n", end-1, end-1);
    }

    // find logo-on frames
    MultLogoFind( &logodata );     // for LOGO

    // output result
    MultLogoWrite( &logodata );    // for LOGO

    //--------------- end logoframe ---------------
    retval = 0;
  } catch (const std::exception &error) {
    fprintf(stderr, "error: %s\n", error.what());
  }

  MultLogoFree( &logodata );     // for LOGO
  return retval;
}
