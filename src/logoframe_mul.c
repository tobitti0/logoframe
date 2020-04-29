//====================================================================
// �������S�����p�̊֐�
//====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include "logo.h"
#include "logoset.h"
#include "logoset_mul.h"

// logo function of logoframe
extern void LogoInit(LOGO_DATASET *pl);
extern void LogoFree(LOGO_DATASET *pl);
extern int  LogoRead(const char *logofname, LOGO_DATASET *pl);
extern int  LogoFrameInit(LOGO_DATASET *pl, long num_frames);
extern void LogoCalc(LOGO_DATASET *pl, const BYTE *data, int pitch, int nframe);
extern void LogoWriteFrameParam(LOGO_DATASET *pl, int nframe, FILE *fpo_ana2);
extern void LogoFind(LOGO_DATASET *pl);
extern long LogoGetTotalFrame(LOGO_DATASET *pl);
extern long LogoGetTotalFrameWithClear(LOGO_DATASET *pl);
extern void LogoWriteFind(LOGO_DATASET *pl, FILE *fpo_ana);
extern void LogoWriteOutput(LOGO_DATASET *pl, const char *logofname, FILE *fpout, int outform);
extern void LogoResultWrite(LOGO_RESULTOUTREC *prs, FILE *fpo_ana);
extern void LogoResultInit(LOGO_RESULTOUTREC *prs);
extern int  LogoResultAdd(LOGO_RESULTOUTREC *prs, LOGO_DATASET *pl, int autosel);



//---------------------------------------------------------------------
// Shift-JIS�̂Q�o�C�g�����`�F�b�N
//---------------------------------------------------------------------
int sjis_multibyte(char *str){
	unsigned char code;

	code = (unsigned char)*str;
	if ((code >= 0x81 && code <= 0x9F) ||
		(code >= 0xE0 && code <= 0xFC)){		// Shift-JIS 1st-byte
		code = (unsigned char)*(str+1);
		if ((code >= 0x40 && code <= 0x7E) ||
			(code >= 0x80 && code <= 0xFC)){	// Shift-JIS 2nd-byte
			return 1;
		}
	}
	return 0;
}

//---------------------------------------------------------------------
// Shift-JIS�R�[�h�l����strrchr
//---------------------------------------------------------------------
char *strrchrj(char *str, char ch){
	char *dst;

	dst = NULL;
	if (str != NULL){
		while(*str != '\0'){
			if (*str == ch){
				dst = str;
			}
			if (sjis_multibyte(str) > 0){
				str ++;
			}
			str ++;
		}
	}
	return dst;
}
//---------------------------------------------------------------------
// Shift-JIS�R�[�h�l����strchr
//---------------------------------------------------------------------
//char *strchrj(char *str, char ch){
//	char *dst;
//
//	dst = NULL;
//	if (str != NULL){
//		while(dst == NULL && *str != '\0'){
//			if (*str == ch){
//				dst = str;
//			}
//			if (sjis_multibyte(str) > 0){
//				str ++;
//			}
//			str ++;
//		}
//	}
//	return dst;
//}


//---------------------------------------------------------------------
// ���s�t�@�C���̃p�X���擾
// ***** windows API���g�p *****
//
// ���s���͕Ԃ�l=0
//---------------------------------------------------------------------
int MultLogo_GetModuleFileName(char *str, int maxlen){
	return GetModuleFileName(NULL, str, maxlen);
}

//---------------------------------------------------------------------
// �g���q�O�܂ł����S�Ɋ܂݁A�g���q�������t�@�C����W�J
// ***** windows API���g�p *****
//
// get filename (using WIN32API)
// input:  filename_src      filename before adding wildcard
// output: *baselist[]       matched list of filename (malloc string)
// return:                   0:normal 1:error by filename 2:error by malloc
//---------------------------------------------------------------------
int MultLogo_FileListGet(char *baselist[], const char *filename_src)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFile;
	char *strtmp;
	char *strext;
	char *newstr;      // for serach (add asterisk from filename_src)
	char *newstr2;     // for folder (extract folder from filename_src)
	int n, endf, nlen, nlen2;
	int errnum;

	errnum = 0;
	if (filename_src == NULL){
		return 1;
	}
	else if (strlen(filename_src) < 1){
		return 1;
	}

	// set filename to find
	// newstr  : search string. malloc=( filename_src + "\*" + EXTNAME_LOGODATA )
	// newstr2 : folder string. malloc=( filename_src + "\" )
	newstr = (char *)malloc( (strlen(filename_src)+strlen(EXTNAME_LOGODATA)+3) * sizeof(char) );
	if (newstr == NULL){
		fprintf(stderr, "error:failed in memory allocation.\n");
		errnum = 2;
	}
	newstr2 = (char *)malloc( (strlen(filename_src)+2) * sizeof(char) );
	if (newstr2 == NULL){
		fprintf(stderr, "error:failed in memory allocation.\n");
		errnum = 2;
	}
	if (errnum == 0){
		strcpy(newstr,  filename_src);
		strcpy(newstr2, filename_src);

		// add asterisk
		strext = strrchr(filename_src, '.');      // get ext location
		if (strext != NULL){
			if (strrchrj(strext, DELIMITER_DIR) != NULL){   // check if '.' is folder name
				strext = NULL;
			}
		}
		if (strext != NULL){                      // add '*' before ext
			strtmp = strrchr(newstr, '.');
			strcpy(strtmp, "*");
			strcat(strtmp, strext);
		}
		else{
			// check if folder name
			hFile = FindFirstFile( newstr, &FindFileData );
			if (hFile != INVALID_HANDLE_VALUE){
				if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
					strcat(newstr,  DELIMITER_STRDIR);
					strcat(newstr2, DELIMITER_STRDIR);
				}
				FindClose(hFile);
			}
			strcat(newstr, "*");
			strcat(newstr, EXTNAME_LOGODATA);   // �g���q���Ȃ��ꍇ�̒ǉ�
		}

		// get folder (newstr2 is folder)
		strtmp = strrchrj(newstr2, DELIMITER_DIR);
		if (strtmp != NULL){
			strcpy(strtmp, DELIMITER_STRDIR);
		}
		else{
			strcpy(newstr2, "");
		}
		nlen2 = strlen(newstr2);

		// free if data already exist
		for(n=0; n<LOGONUM_MAX; n++){
			if (baselist[n] != NULL){
				free(baselist[n]);
			}
		}
	}

	// get filename
	if (errnum == 0){
		n = 0;
		hFile = FindFirstFile( newstr, &FindFileData );
		if (hFile != INVALID_HANDLE_VALUE) {
			endf = 0;
			while(n < FILELISTNUM_MAX && endf == 0){
				nlen = strlen(FindFileData.cFileName);
				baselist[n] = (char *)malloc( (nlen + nlen2 + 1) * sizeof(char) );
				if (baselist[n] == NULL){
					fprintf(stderr, "error:failed in memory allocation.\n");
					errnum = 2;
					endf = 1;
				}
				else{
					strcpy(baselist[n], newstr2);                  // folder
					strcat(baselist[n], FindFileData.cFileName);   // name
					// sort
					for(int i=0; i<n; i++){
						if (strcmp(baselist[i], baselist[n]) > 0){
							strtmp = baselist[i];
							baselist[i] = baselist[n];
							baselist[n] = strtmp;
						}
					}
					// get next filename
					if ( !FindNextFile( hFile, &FindFileData ) ){
						endf = 1;
					}
					n ++;
				}
			}
			FindClose(hFile);
		}
		if (n==0){
			fprintf(stderr, "warning:no logo found(%s)\n", newstr);
		}
	}
	if (newstr  != NULL) free(newstr);
	if (newstr2 != NULL) free(newstr2);

	return errnum;
}


//---------------------------------------------------------------------
// ��������������m�ۂ��Đݒ�
//  ����
//   name_src   : �ݒ肷��t�@�C����
//  �o��
//   name_dst   : �ݒ��|�C���^�imalloc�Ń������m�ۍς݁j
//   �Ԃ�l     : 0=����, 2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogo_FileStrGet(char** name_dst, const char* name_src)
{
	char* newstr;
	char* tmpstr;
	int nlen;
	int pos, wlen, flag;

	pos= 0;
	wlen = 0;
	flag = 0;

	if (*name_dst != NULL){
		free(*name_dst);
		*name_dst = NULL;
	}
	if (name_src == NULL){
		return 0;
	}

	nlen = strlen(name_src);
	newstr = (char *)malloc( (nlen + 1) * sizeof(char) );
	if (newstr == NULL){
		fprintf(stderr, "error:failed in memory allocation.\n");
		return 2;
	}
	*name_dst = newstr;

	tmpstr = strchr(name_src, '\"');
	if (tmpstr != NULL){                 // if double quotes found
		tmpstr += 1;                     // move inside of the double quotes
		strcpy(newstr, tmpstr);
		tmpstr = strchr(newstr, '\"');   // next double quotes
		if (tmpstr != NULL){
			strcpy(tmpstr, "");          // delete after the double quotes
			flag = 1;
		}
	}
	if (flag == 0){
		while( name_src[pos] >= ' ' || name_src[pos] < 0){
			newstr[wlen++] = name_src[pos++];
		}
		newstr[wlen] = '\0';
	}
	if (strlen(newstr) == 0){
		free(newstr);
		*name_dst = NULL;
	}
	return 0;
}


//---------------------------------------------------------------------
// �����񂩂獀�ڂƒl���擾
//  ����
//   buf     : ������
//   sizestr : ���ڕ�����ő咷
//  �o��
//   strw    : ���ڕ�����
//   �Ԃ�l  : 0�ȏ�=�l��buf��������ʒu, -1=�l�Ȃ�
//---------------------------------------------------------------------
int MultLogo_StrArgGet(char *strw, const char* buf, int sizestr){
	int pos, wlen, type;

	pos = 0;
	wlen = 0;
	type = 0;
	if (buf[0] == ';' || buf[0] == '#'){
		strcpy(strw, "");
		pos = -1;
	}
	else{
		while( buf[pos] == ' ' ) pos++; 	// trim left
		if (buf[pos] != '-'){
			if (strchr(buf, '=') != NULL){
				type = 1;					// .ini type
			}
		}
		if (type == 1){
			strw[wlen++] = '-';
		}
		while( (buf[pos] > ' ' || buf[pos] < 0) &&
			   (type != 1 || buf[pos] != '=') &&
			   wlen < sizestr-1 ){
			strw[wlen++] = buf[pos++];
		}
		strw[wlen] = '\0';
		while( buf[pos] == ' ' ) pos++;
		if (type == 1 && buf[pos] == '=') pos++;
		if (buf[pos] >= 0 && buf[pos] < ' '){	// not exist 2nd arg
			pos = -1;
		}
	}
	return pos;
}


//##### �I�v�V�����֘A����

//---------------------------------------------------------------------
// 臒l�p�����[�^�̏�����
//  ���o��
//   p : ���ݒ�̈��臒l�p�����[�^
//---------------------------------------------------------------------
void MultLogo_ParamClear(MLOGO_THRESREC *p){
	MLOGO_UPTHRESREC*  pup;

	pup = &(p->up);
	memset(pup, 0, sizeof(MLOGO_UPTHRESREC));
}

//---------------------------------------------------------------------
// 臒l�p�����[�^�����ݒ�
//  ���o��
//   p      : ���ݒ�̈��臒l�p�����[�^
//  ����
//   name   : 臒l�p�����[�^���O
//   value  : 臒l�p�����[�^�l
//  �o��
//   �Ԃ�l : 0=�ݒ�, 1=�p�����[�^�񌟏o
//---------------------------------------------------------------------
int MultLogo_ParamPreSet(MLOGO_THRESREC *p, const char *name, const char *value){
    int n;

    n = atoi(value);
    if (!stricmp(name, "-fadein")) {
        p->dat.num_fadein     = n;
        p->dat.auto_fade      = 0;
        p->up.up_num_fadein   = 1;
        p->up.up_auto_fade    = 1;
    }
    else if (!stricmp(name, "-fadeout")) {
        p->dat.num_fadeout    = n;
        p->dat.auto_fade      = 0;
        p->up.up_num_fadeout  = 1;
        p->up.up_auto_fade    = 1;
    }
    else if (!stricmp(name, "-mrgleft")) {
        p->dat.num_cutleft    = n;
        p->up.up_num_cutleft  = 1;
    }
    else if (!stricmp(name, "-mrgright")) {
        p->dat.num_cutright   = n;
        p->up.up_num_cutright = 1;
    }
    else if (!stricmp(name, "-onwidth")) {
        p->dat.num_onwidth    = n;
        p->up.up_num_onwidth  = 1;
    }
    else if (!stricmp(name, "-onlevel")) {
        p->dat.num_onlevel    = n;
        p->up.up_num_onlevel  = 1;
    }
    else if (!stricmp(name, "-offwidth")) {
        p->dat.num_offwidth    = n;
        p->up.up_num_offwidth  = 1;
    }
    else if (!stricmp(name, "-ymax")) {
        p->dat.thres_ymax     = n;
        p->up.up_thres_ymax   = 1;
    }
    else if (!stricmp(name, "-ymin")) {
        p->dat.thres_ymin     = n;
        p->up.up_thres_ymin   = 1;
    }
    else if (!stricmp(name, "-yedge")) {
        p->dat.thres_yedge     = n;
        p->dat.auto_bs11       = 0;
        p->up.up_thres_yedge   = 1;
        p->up.up_auto_bs11     = 1;
    }
    else if (!stricmp(name, "-ydif")) {
        p->dat.thres_ydif     = n;
        p->up.up_thres_ydif   = 1;
    }
    else if (!stricmp(name, "-ysetdif")) {
        p->dat.thres_ysetdif     = n;
        p->up.up_thres_ysetdif   = 1;
    }
    else if (!stricmp(name, "-yoffedg")) {
        p->dat.thres_yoffedg     = n;
        p->up.up_thres_yoffedg   = 1;
    }
    else if (!stricmp(name, "-areaset")) {
        p->dat.num_areaset       = n;
        p->up.up_num_areaset     = 1;
    }
    else if (!stricmp(name, "-clrrate")) {
        p->dat.num_clrrate    = n;
        p->up.up_num_clrrate  = 1;
    }
    else{
        return 1;
	}

	return 0;
}


//---------------------------------------------------------------------
// 臒l�p�����[�^��ݒ�i���ݒ�̈���e���e���S�f�[�^�ɃR�s�[�j
//  ����
//   p      : ���ݒ�̈��臒l�p�����[�^
//  �o��
//   plogot : �e���S�f�[�^��臒l�p�����[�^
//---------------------------------------------------------------------
void MultLogo_ParamCopy(LOGO_THRESREC *plogot, MLOGO_THRESREC *p){
	MLOGO_UPTHRESREC*  pup;
	LOGO_THRESREC*     pdat;

	pup  = &(p->up);
	pdat = &(p->dat);
	if (pup->up_num_fadein > 0){
		plogot->num_fadein   = pdat->num_fadein;
	}
	if (pup->up_num_fadeout > 0){
		plogot->num_fadeout  = pdat->num_fadeout;
	}
	if (pup->up_num_cutleft > 0){
		plogot->num_cutleft  = pdat->num_cutleft;
	}
	if (pup->up_num_cutright > 0){
		plogot->num_cutright = pdat->num_cutright;
	}
	if (pup->up_num_onwidth > 0){
		plogot->num_onwidth  = pdat->num_onwidth;
	}
	if (pup->up_num_onlevel > 0){
		plogot->num_onlevel  = pdat->num_onlevel;
	}
	if (pup->up_num_offwidth > 0){
		plogot->num_offwidth  = pdat->num_offwidth;
	}
	if (pup->up_thres_ymax > 0){
		plogot->thres_ymax   = pdat->thres_ymax;
	}
	if (pup->up_thres_ymin > 0){
		plogot->thres_ymin   = pdat->thres_ymin;
	}
	if (pup->up_thres_yedge > 0){
		plogot->thres_yedge  = pdat->thres_yedge;
	}
	if (pup->up_thres_ydif > 0){
		plogot->thres_ydif   = pdat->thres_ydif;
	}
	if (pup->up_thres_ysetdif > 0){
		plogot->thres_ysetdif   = pdat->thres_ysetdif;
	}
	if (pup->up_thres_yoffedg > 0){
		plogot->thres_yoffedg   = pdat->thres_yoffedg;
	}
	if (pup->up_num_areaset > 0){
		plogot->num_areaset    = pdat->num_areaset;
	}
	if (pup->up_num_clrrate > 0){
		plogot->num_clrrate  = pdat->num_clrrate;
	}
	if (pup->up_auto_fade > 0){
		plogot->auto_fade    = pdat->auto_fade;
	}
	if (pup->up_auto_bs11 > 0){
		plogot->auto_bs11    = pdat->auto_bs11;
	}
}


//=====================================================================
// Public�֐��F�I�v�V�����ݒ�
// �I�v�V������ǂݍ���
//  ����
//   strcmd : �I�v�V�������O������
//   strval : �p�����[�^�l������
//  �o��
//   �Ԃ�l : �g�p�����i0=�p�����[�^���o����, -1=�G���[�j
//=====================================================================
int MultLogoOptionAdd(MLOGO_DATASET* pml, const char* strcmd, const char* strval){
	int nlist;

	if (strcmd == NULL){
		nlist = 0;
	}
	else if (strval == NULL){
		nlist = 1;
	}
	else{
		nlist = 2;
	}
	if (nlist == 0){
		return 0;
	}
	if (strcmd[0] == '-' && strcmd[1] != '\0'){
		if (!stricmp(strcmd, "-logo")) {
			MultLogo_FileStrGet( &(pml->opt_logofilename), strval );
			return 2;
		}
		else if (!stricmp(strcmd, "-logoparam")) {
			MultLogo_FileStrGet( &(pml->opt_logoparamfile), strval );
			return 2;
		}
		else if (!stricmp(strcmd, "-oanum")) {
			if (nlist < 2) {
				fprintf(stderr, "-oanum needs an argument\n");
				return -1;
			}
			pml->oanum = atoi(strval);
			if (pml->oanum > LOGONUM_MAX){
				pml->oanum = LOGONUM_MAX;
			}
			return 2;
		}
		else if (!stricmp(strcmd, "-oasel")) {
			if (nlist < 2) {
				fprintf(stderr, "-oasel needs an argument\n");
				return -1;
			}
			pml->oasel = atoi(strval);
			return 2;
		}
		else if (!stricmp(strcmd, "-oamask")) {
			if (nlist < 2) {
				fprintf(stderr, "-oamask needs an argument\n");
				return -1;
			}
			pml->oamask = atoi(strval);
			return 2;
		}
		else if (!stricmp(strcmd, "-outform")) {
			if (nlist < 2) {
				fprintf(stderr, "-outform needs an argument\n");
				return -1;
			}
			pml->outform = atoi(strval);
			return 2;
		}
		else if (!stricmp(strcmd, "-dispoff")) {
			if (nlist < 2) {
				fprintf(stderr, "-dispoff needs an argument\n");
				return -1;
			}
			pml->dispoff = atoi(strval);
			return 2;
		}
		else if (!stricmp(strcmd, "-paramoff")) {
			if (nlist < 2) {
				fprintf(stderr, "-paramoff needs an argument\n");
				return -1;
			}
			pml->paramoff = atoi(strval);
			return 2;
		}
		else if (!stricmp(strcmd, "-o")) {
			MultLogo_FileStrGet( &(pml->opt_outfile), strval );
			return 2;
		}
		else if (!stricmp(strcmd, "-oa")) {
			MultLogo_FileStrGet( &(pml->opt_anafile), strval );
			return 2;
		}
		else if (!stricmp(strcmd, "-oa2")) {
			MultLogo_FileStrGet( &(pml->opt_ana2file), strval );
			return 2;
		}
		else if (!strnicmp(strcmd, "-logo", 5) &&
				 (strcmd[5] >= '0' && strcmd[5] <= '9')){
			int k = atoi(&strcmd[5]);
			if (k >= 1 && k <= LOGONUM_MAX){
				MultLogo_FileStrGet( &(pml->all_logofilename[k-1]), strval );
			}
			else{
				fprintf(stderr, "-logoN : N>=1 and N<=%d\n", LOGONUM_MAX);
				return -1;
			}
			return 2;
		}
		else if (!stricmp(strcmd, "-fadein")   ||
				 !stricmp(strcmd, "-fadeout")  ||
				 !stricmp(strcmd, "-mrgleft")  ||
				 !stricmp(strcmd, "-mrgright") ||
				 !stricmp(strcmd, "-onwidth")  ||
				 !stricmp(strcmd, "-onlevel")  ||
				 !stricmp(strcmd, "-offwidth") ||
				 !stricmp(strcmd, "-ymax")     ||
				 !stricmp(strcmd, "-ymin")     ||
				 !stricmp(strcmd, "-yedge")    ||
				 !stricmp(strcmd, "-ydif")     ||
				 !stricmp(strcmd, "-ysetdif")  ||
				 !stricmp(strcmd, "-yoffedg")  ||
				 !stricmp(strcmd, "-areaset")  ||
				 !stricmp(strcmd, "-clrrate")) {			// set threshold
			if (nlist < 2) {
				fprintf(stderr, "%s needs an argument\n", strcmd);
				return -1;
			}
			MultLogo_ParamPreSet( &(pml->thres_arg), strcmd, strval );
			return 2;
		}
	}

	return 0;
}


//=====================================================================
// Public�֐��F�t�@�C������̃I�v�V�����ݒ�
// �t�@�C���ɋL�ڂ���Ă���I�v�V������ǂݍ���
//  ����
//   fname     : �t�@�C����
//  �o��
//   �Ԃ�l : �ݒ肵���I�v�V�������ڐ��i0=�ݒ�Ȃ�, -1=�G���[�j
//=====================================================================
int MultLogoOptionFile(MLOGO_DATASET* pml, const char* fname){
	FILE* fpr;
	int pos, nopt, ncount, invalid;
	char buf[FILE_BUFSIZE];
	char str[FILE_BUFSIZE];
	const char* strval;

	ncount = 0;
	invalid = 0;
	fpr = fopen(fname, "r");
	if (!fpr){
//		fprintf(stderr, "warning:not found '%s'\n", fname);
		return -1;
	}
	else{
		while( fgets(buf, FILE_BUFSIZE, fpr) != NULL ){
			if (buf[0] == '['){				// when section
				if (!strnicmp(buf, SECTION_LOGOFRAME, strlen(SECTION_LOGOFRAME))){
					invalid = 0;			// section-in
				}
				else{
					invalid = 1;			// out of section
				}
			}
			else if (invalid == 0){
				pos = MultLogo_StrArgGet(str, buf, FILE_BUFSIZE);
				if (strlen(str) > 0){
					if (pos > 0){
						strval = &buf[pos];
					}
					else{
						strval = NULL;
					}
					
					nopt = MultLogoOptionAdd(pml, str, strval);
					if (nopt > 0){
						ncount ++;
					}
				}
			}
		}
		fclose(fpr);
	}
	return ncount;
}


//=====================================================================
// Public�֐��F�����ǂݍ��݃t�@�C������̃I�v�V�����ݒ�
// �t�@�C���ɋL�ڂ���Ă���I�v�V������ǂݍ���
//  �o��
//   �Ԃ�l : �ݒ肵���I�v�V�������ڐ��i0=�ݒ�Ȃ�, -1=�G���[�j
//=====================================================================
int MultLogoOptionOrgFile(MLOGO_DATASET* pml){
	char buf[FILE_BUFSIZE];
	char* tmpstr;
	int nlen, ncount;

	//--- get full-path filename ---
	if (MultLogo_GetModuleFileName(buf, FILE_BUFSIZE) == 0){
		return -1;
	}

	//--- get path ---
	tmpstr = strrchrj(buf, DELIMITER_DIR);
	if (tmpstr != NULL){
		*(tmpstr+1) = '\0';			// get only path
	}
	else{
		strcpy(buf, "");			// when no path found
	}

	//--- set path + ini-filename ---
	nlen = strlen(buf) + strlen(INIFILE_NAME);
	if (nlen >= FILE_BUFSIZE){
		return -1;
	}
	strcat(buf, INIFILE_NAME);

	//--- set options ---
	ncount = MultLogoOptionFile(pml, buf);

	return ncount;
}



//##### �������E�I������

//=====================================================================
// Public�֐��F�N�����ɂP�񂾂��Ăяo��
// �ϐ�����������
//=====================================================================
void MultLogoInit(MLOGO_DATASET* pml){
	int i;

	pml->dispoff     = 0;
	pml->paramoff    = 0;
	pml->num_deflogo = 0;

	pml->oanum   = DEF_MLOGO_OANUM;
	pml->oasel   = 0;
	pml->oamask  = 0;
	pml->outform = 0;

	pml->opt_logofilename  = NULL;
	pml->opt_logoparamfile = NULL;
	pml->opt_outfile       = NULL;
	pml->opt_anafile       = NULL;
	pml->opt_ana2file      = NULL;
	for(i=0; i<LOGONUM_MAX; i++){
		pml->all_logofilename[i] = NULL;
	}
	for(i=0; i<LOGONUM_MAX; i++){
		pml->fpo_ana2[i]          = NULL;
		pml->all_logodata[i]      = NULL;
	}
	pml->logoresult.num_find = 0;
	for(i=0; i<LOGONUM_MAX; i++){
		pml->total_valid[i]   = 0;
		pml->total_frame[i]   = 0;
		pml->priority_list[i] = 0;
	}
	pml->num_detect  = 0;
	pml->num_others  = 0;
	pml->num_disable = 0;
	MultLogo_ParamClear(&(pml->thres_arg));
}


//=====================================================================
// Public�֐��F�I�����ɂP�񂾂��Ăяo��
// �̈�m�ۃ������̉������
//=====================================================================
void MultLogoFree(MLOGO_DATASET* pml){
	int i;

	// �f�o�b�O�p�t�@�C���͒ʏ�N���[�Y�ς݂ł��邪�ُ�I�����Ɏg�p
	for(i = 0; i < LOGONUM_MAX; i++){
		if (pml->fpo_ana2[i])
	        fclose(pml->fpo_ana2[i]);
	        pml->fpo_ana2[i] = NULL;
	}

	// ���S�֘A�̃��������
	for(i = 0; i < LOGONUM_MAX; i++){
		if (pml->all_logodata[i] != NULL){
		    LogoFree( pml->all_logodata[i] );
		    free( pml->all_logodata[i] );
		    pml->all_logodata[i] = NULL;
		}
	}
	// �ݒ�t�@�C�����̃��������
	if (pml->opt_logofilename != NULL){
		free( pml->opt_logofilename );
		pml->opt_logofilename = NULL;
	}
	if (pml->opt_logoparamfile != NULL){
		free( pml->opt_logoparamfile );
		pml->opt_logoparamfile = NULL;
	}
	if (pml->opt_outfile != NULL){
		free( pml->opt_outfile );
		pml->opt_outfile = NULL;
	}
	if (pml->opt_anafile != NULL){
		free( pml->opt_anafile );
		pml->opt_anafile = NULL;
	}
	if (pml->opt_ana2file != NULL){
		free( pml->opt_ana2file );
		pml->opt_ana2file = NULL;
	}
	for(i = 0; i < LOGONUM_MAX; i++){
		if (pml->all_logofilename[i] != NULL){
			free( pml->all_logofilename[i] );
			pml->all_logofilename[i] = NULL;
		}
	}
}



//##### ���o�J�n�O�̐ݒ菈��

//---------------------------------------------------------------------
// �g���q�O�܂ł����S�Ɋ܂݊g���q�������t�@�C����W�J���A���S�o�^����
//  ����
//   pml->opt_logofilename  : "-logo"�I�v�V�����t�@�C����
//  �o��
//   pml->all_logofilename  : �e���S�t�@�C�����imalloc�ɂ��̈�m�ہj
//   �Ԃ�l                 : 0=����, 2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoSetup_ExpandFilename(MLOGO_DATASET* pml){
	int i, idst, ibase, fin_flag;
	int errnum;
	const char* basename;
	char* strbase;
	char* baselist_filename[FILELISTNUM_MAX];

	errnum = 0;
	basename = pml->opt_logofilename;
	if (basename != NULL){
		for(i=0; i<FILELISTNUM_MAX; i++){
			baselist_filename[i] = NULL;
		}
		// get filename list
		errnum = MultLogo_FileListGet(baselist_filename, basename);
		if (errnum == 0){
			// add the list to all_logofilename[]
			fin_flag = 0;
			ibase = 0;
			while(ibase < FILELISTNUM_MAX && fin_flag == 0){
				strbase = baselist_filename[ibase];
				if (strbase == NULL){
					fin_flag = 1;
				}
				else{
					idst = -1;
					for(i = 0; i < LOGONUM_MAX; i++){
						if (pml->all_logofilename[i] == NULL){
							if (idst < 0){                    // if 1st NULL point
								idst = i;                     // set point
							}
						}
						else if (!stricmp(strbase, pml->all_logofilename[i])){
							idst = LOGONUM_MAX;               // disable if exist
						}
					}
					if (idst == LOGONUM_MAX){                 // skip by disable
						fprintf(stderr, "already defined logo:%s\n", strbase);
					}
					else if (idst >= pml->oanum){                  // until oanum option
						fprintf(stderr, "too many logo found(skip):%s\n", strbase);
					}
					else if (idst >= 0 && idst < pml->oanum){ // set filename
						MultLogo_FileStrGet( &(pml->all_logofilename[idst]), strbase );
					}
				}
				ibase ++;
			}
		}
		// �t�@�C�����W�J�p�ɕێ��������������
		for(i = 0; i < FILELISTNUM_MAX; i++){
			if (baselist_filename[i] != NULL){
				free( baselist_filename[i] );
			}
		}
	}
	return 0;
}


//---------------------------------------------------------------------
// �e���S�t�@�C����������
// ���S�t�@�C��������`����Ă��郍�S�̂ݗ̈���m�ۂ���
//  �o��
//   �Ԃ�l : 0=���� 2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoSetup_EachInit(MLOGO_DATASET* pml){
	int i;

	pml->num_deflogo = 0;
	for(i = 0; i < LOGONUM_MAX; i++){
		pml->all_logodata[i] = NULL;
		if (pml->all_logofilename[i] != NULL){
			if (strlen(pml->all_logofilename[i]) > 0){
				pml->all_logodata[i] = (LOGO_DATASET*)malloc(sizeof(LOGO_DATASET));
				if (pml->all_logodata[i] == NULL){
					fprintf(stderr, "error:failed in memory allocation.\n");
					return 2;
				}
				LogoInit( pml->all_logodata[i] );
				pml->num_deflogo ++;
			}
		}
	}
	return 0;
}


//---------------------------------------------------------------------
// �t�@�C������臒l�p�����[�^��ǂݍ���
//  ����
//   fname : �t�@�C����
//  �o��
//   pmt    : �擾�p���̈��臒l�p�����[�^
//   �Ԃ�l : 0=����
//---------------------------------------------------------------------
int MultLogoSetup_ParamRead_File(MLOGO_THRESREC *pmt, const char* fname, int dispoff){
	FILE* fpr;
    int pos, wlen;
    char buf[FILE_BUFSIZE];
    char str[FILE_BUFSIZE];

	// set from param-file
	fpr = fopen(fname, "r");
	if (!fpr){
//        fprintf(stderr, "warning:not found '%s'\n", fname);
	}
	else{
		while( fgets(buf, FILE_BUFSIZE, fpr) != NULL ){
	        if (buf[0] != '#'){
				pos = 0;
				wlen = 0;
				while( buf[pos] == ' ' ) pos++;
				while( buf[pos] > ' ' || buf[pos] < 0 ){
					str[wlen++] = buf[pos++];
				}
				str[wlen] = '\0';
				while( buf[pos] == ' ' ) pos++;
				if ( buf[pos] > ' ' || buf[pos] < 0 ){
					MultLogo_ParamPreSet( pmt, str, &buf[pos] );
				}
			}
		}
		fclose(fpr);
		if (dispoff == 0){
	        printf("info:read parameter from '%s'\n", fname);
	    }
	}
	return 0;
}

//---------------------------------------------------------------------
// �e���S��臒l�p�����[�^��ݒ�
//  �o��
//   �Ԃ�l : 0=����  2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoSetup_ParamRead(MLOGO_DATASET* pml){
	MLOGO_THRESREC thres_file;
	LOGO_DATASET* plogo;
	const char* logofilename;
	char* newstr;
	char* tmpstr;
	int i, dispoff;

	dispoff = (pml->dispoff > 0 || pml->paramoff > 0)? 1: 0;

	// read parameter (common)
	if (pml->opt_logoparamfile != NULL){
		MultLogo_ParamClear( &thres_file );
		MultLogoSetup_ParamRead_File( &thres_file, pml->opt_logoparamfile, dispoff );

		for(i = 0; i < LOGONUM_MAX; i++){
			plogo = pml->all_logodata[i];
			if (plogo != NULL){
				MultLogo_ParamCopy( &(plogo->thresdat), &thres_file );
			}
		}
	}
    // read parameter (each)
	for(i = 0; i < LOGONUM_MAX; i++){
		logofilename  = pml->all_logofilename[i];
		plogo         = pml->all_logodata[i];
		if (plogo != NULL && logofilename != NULL){
			newstr = (char *)malloc(
						(strlen(logofilename) + strlen(EXTNAME_LOGOPARAM) + 1)
						 * sizeof(char) );
			if (newstr == NULL){
				fprintf(stderr, "error:failed in memory allocation.\n");
				return 2;
			}
			strcpy(newstr, logofilename);
			tmpstr = strstr(newstr, EXTNAME_LOGODATA);
			if (tmpstr != NULL){
				strcpy(tmpstr, EXTNAME_LOGOPARAM);
			}
			else{
				strcat(newstr, EXTNAME_LOGOPARAM);
			}
			MultLogo_ParamClear( &thres_file );
			MultLogoSetup_ParamRead_File( &thres_file, newstr, dispoff );
			MultLogo_ParamCopy( &(plogo->thresdat), &thres_file );
			if (newstr != NULL){
				free(newstr);
			}
		}
	}
	// set parameter from input option
	for(i = 0; i < LOGONUM_MAX; i++){
		logofilename  = pml->all_logofilename[i];
		plogo = pml->all_logodata[i];
		if (plogo != NULL && logofilename != NULL){
			MultLogo_ParamCopy( &(plogo->thresdat), &(pml->thres_arg) );
		}
	}
	return 0;
}


//---------------------------------------------------------------------
// �e���S�f�[�^�̃t���[�����ɑΉ������̈��������
//  ����
//   num_frames : �ǂݍ��މ摜�f�[�^�̃t���[����
//  �o��
//   �Ԃ�l : 0=����  2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoSetup_FrameInit(MLOGO_DATASET* pml, int num_frames){
	LOGO_DATASET* plogo;
	int i, errnum;

	// initialize framedata
	for(i = 0; i < LOGONUM_MAX; i++){
		plogo      = pml->all_logodata[i];
		if (plogo != NULL){
		    errnum = LogoFrameInit( plogo, num_frames );
		    if (errnum > 0){
		        return 2;
		    }
		}
	}
	return 0;
}


//---------------------------------------------------------------------
// �e���S�f�[�^�ǂݍ���
//  ����
//   pml->all_logofilename : �e���S�t�@�C����
//  �o��
//   pml->all_logodata     : �e���S�f�[�^
//   �Ԃ�l : 0=����  1=�t�@�C���G���[  2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoSetup_LogoRead(MLOGO_DATASET* pml){
	const char*   logofilename;
	LOGO_DATASET* plogo;
	int errnum;

	// read logodata
	for(int i = 0; i < LOGONUM_MAX; i++){
		logofilename  = pml->all_logofilename[i];
		plogo      = pml->all_logodata[i];
		if (plogo != NULL){
		    errnum = LogoRead( logofilename, plogo );
		    if (errnum > 0){
				fprintf(stderr, "error filename:%s\n", logofilename);
		        return errnum;
		    }
		}
	}
	return 0;
}


//---------------------------------------------------------------------
// "-oa2"�I�v�V�����Ŏw�肷��f�o�b�O�p���t���[�����t�@�C���I�[�v��
//  ����
//   pml->opt_ana2file : �t�@�C����
//  �o��
//   �Ԃ�l : 0=����  1=�t�@�C���G���[  2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoSetup_FileAna2Open(MLOGO_DATASET* pml){
	LOGO_DATASET* plogo;
	const char* ana2file;
	char* newstr;
	char* newstr1;
	char* newstr2;
	char* tmpstr;
	int i, ret;

	ret = 0;
	if (pml->opt_ana2file != NULL){
		ana2file = pml->opt_ana2file;		// newstr : ������m��"_100"
		newstr  = (char *)malloc( (strlen(ana2file) + 4 + 1) * sizeof(char) );
		newstr1 = (char *)malloc( (strlen(ana2file)     + 1) * sizeof(char) );
		newstr2 = (char *)malloc( (strlen(ana2file)     + 1) * sizeof(char) );
		if (newstr == NULL || newstr1 == NULL || newstr2 == NULL){
			fprintf(stderr, "error:failed in memory allocation.\n");
			ret = 2;
		}
		else{
			strcpy(newstr1, ana2file);
			tmpstr = strrchr(newstr1, '.');
			if (tmpstr != NULL){
				strcpy(newstr2, tmpstr);		// newstr2 : �g���q����
				strcpy(tmpstr, "");				// newstr1 : �g���q������������
			}
			else{
				strcpy(newstr2, "");
			}
			for(i = 0; i < LOGONUM_MAX; i++){
				plogo      = pml->all_logodata[i];
				if (plogo != NULL){
					if (pml->num_deflogo == 1){		// ���S���P�����Ȃ�]���ʂ�
						strcpy(newstr, ana2file);
					}
					else{						// ���S���Q�ȏ�Ȃ疼�O�ɔԍ���ǉ�
						sprintf(newstr, "%s_%d%s", newstr1, i+1, newstr2);
					}
					pml->fpo_ana2[i] = fopen(newstr, "w");
					if (!pml->fpo_ana2[i]){
						fprintf(stderr, "error: failed to create/open '%s'\n", newstr);
						ret = 1;
						break;
					}
				}
			}
        }
        if (newstr2 != NULL) free(newstr2);
        if (newstr1 != NULL) free(newstr1);
        if (newstr  != NULL) free(newstr);
    }
    return ret;
}


//=====================================================================
// Public�֐��F�J�n�O�̐ݒ菉����
// �ݒ菉�������s��
//  ����
//   num_frames : �ǂݍ��މ摜�f�[�^�̃t���[����
//  �o��
//   �Ԃ�l : 0=����  1=�G���[  2=�������m�ۃG���[  3=���S�f�[�^�Ȃ�
//=====================================================================
int MultLogoSetup(MLOGO_DATASET* pml, int num_frames){
	int ret;

	pml->image_frames = num_frames;

	ret = MultLogoSetup_ExpandFilename(pml);	// ���S�t�@�C������W�J
	if (ret == 0){					// �g�p����e���S�t�@�C���̈��������
		ret = MultLogoSetup_EachInit(pml);
		if (pml->num_deflogo == 0){				// ���S��`�Ȃ�
			fprintf(stderr, "warning:no logo definition found.\n");
			ret = 3;
		}
	}
	if (ret == 0){					// 臒l�p�����[�^�ǂݍ���
		ret = MultLogoSetup_ParamRead(pml);
	}
	if (ret == 0){					// ���S�f�[�^�ǂݍ���
		ret = MultLogoSetup_LogoRead(pml);
	}
	if (ret == 0){					// �t���[�����ɑΉ������̈�m�ہE������
		ret = MultLogoSetup_FrameInit(pml, num_frames);
	}
	if (ret == 0){					// "-oa2"�f�o�b�O�p���t���[�����I�[�v��
		ret = MultLogoSetup_FileAna2Open(pml);
	}
	return ret;
}



//=====================================================================
// Public�֐��F���S�p�����[�^����\��
//=====================================================================
void MultLogoDisplayParam(MLOGO_DATASET* pml){
	LOGO_DATASET* plogo;
	char str_autofade[11];
	char str_offwidth[11];
	int i;

	for(i = 0; i < LOGONUM_MAX; i++){
		plogo      = pml->all_logodata[i];
		if (plogo != NULL){
			// set auto fade
			if (plogo->thresdat.auto_fade > 0){
				strcpy(str_autofade, "autofade=1");
			}
			else{
				strcpy(str_autofade, "");
			}
			if (plogo->thresdat.num_offwidth != 0 &&
				plogo->thresdat.num_onwidth != plogo->thresdat.num_offwidth){
				sprintf(str_offwidth, "(%d)", plogo->thresdat.num_offwidth);
			}
			else{
				strcpy(str_offwidth, "");
			}
			// display
			printf("logo%d:%s\n", i+1, pml->all_logofilename[i]);
#ifdef DEBUG_PRINT
			printf("logo%d:loc(%d,%d) %dx%d Edge:%ld(%d) Sum:(%ld,%ld)-(%ld,%ld)%ld/%ld(%d/%d)\n",
				i+1,
				plogo->paramdat.yx, plogo->paramdat.yy,
				plogo->paramdat.yw, plogo->paramdat.yh,
				plogo->paramdat.total_dif, plogo->paramdat.most_logo_y,
				plogo->paramdat.area[0].xmin, plogo->paramdat.area[0].ymin,
				plogo->paramdat.area[0].xmax, plogo->paramdat.area[0].ymax,
				plogo->paramdat.area[0].total_area1, plogo->paramdat.area[0].total_area2,
				plogo->paramdat.thres_dp_y, plogo->paramdat.scale_area2 );
#endif

			if (plogo->thresdat.auto_bs11 == 2){
				printf("detect:BS11\n");
			}
			printf("params fadein:%d fadeout:%d mrgleft:%d mrgright:%d %s\n", 
				plogo->thresdat.num_fadein,  plogo->thresdat.num_fadeout,
				plogo->thresdat.num_cutleft, plogo->thresdat.num_cutright,
				str_autofade );
			printf("       onwidth:%d%s onlevel:%d Y:%d-%d Yedge:%d Ydif:%d Yoffedg:%d\n",
				plogo->thresdat.num_onwidth, str_offwidth,
				plogo->thresdat.num_onlevel,
				plogo->thresdat.thres_ymin, plogo->thresdat.thres_ymax,
				plogo->thresdat.thres_yedge, plogo->thresdat.thres_ydif,
				plogo->thresdat.thres_yoffedg );
		}
	}
}



//##### �P�t���[�����̎��s����

//=====================================================================
// Public�֐��F�摜�P���ɑ΂��郍�S���o���s��
// �P���̉摜�f�[�^�̃��S�L�����擾
//   data      : �摜�f�[�^�P�x�l�ւ̃|�C���^
//   pitch     : �摜�f�[�^�̂P�s�f�[�^��
//   nframe    : �t���[���ԍ�
//   height    : �摜�f�[�^�̍����i���S�f�[�^���摜�������f�݂̂Ɏg�p�j
//=====================================================================
void MultLogoCalc(MLOGO_DATASET* pml, const BYTE *data, int pitch, int nframe, int height){
	LOGO_DATASET* plogo;
	int i;

	for(i = 0; i < LOGONUM_MAX; i++){
		plogo      = pml->all_logodata[i];
		if (plogo != NULL){
			// �摜�O�������A�N�Z�X�ŗ����邱�Ƃ�h�����߂ɒǉ�
			if ((plogo->paramdat.yx + plogo->paramdat.yw <= pitch+8 &&
				 plogo->paramdat.yy + plogo->paramdat.yh <  height-1) ||
			    (plogo->paramdat.yx + plogo->paramdat.yw <  pitch   &&
				 plogo->paramdat.yy + plogo->paramdat.yh <  height  )){
		        LogoCalc( plogo, data, pitch, nframe );
		        if (pml->fpo_ana2[i] != NULL){     // for debug
					LogoWriteFrameParam( plogo, nframe, pml->fpo_ana2[i] );
				}
			}
		}
	}
}



//##### �S�t���[���ǂݍ��݌�̌��o����

//---------------------------------------------------------------------
// "-oa2"�I�v�V�����Ŏw�肷��f�o�b�O�p���t���[�����t�@�C���N���[�Y
//---------------------------------------------------------------------
void MultLogoFind_FileAna2Close(MLOGO_DATASET* pml){
	int i;

	for(i = 0; i < LOGONUM_MAX; i++){
		if (pml->fpo_ana2[i])
	        fclose(pml->fpo_ana2[i]);
	        pml->fpo_ana2[i] = NULL;
	}
}

//---------------------------------------------------------------------
// ���S���ʑS�̂���̔��ʏ���
// ���S���ʂ���s�v�Ƃ��郍�S�𔻕ʂ��Ė������A�K�v�ȃ��S�͊��Ԃ����Z
//  �o��
//    pml->logoresult    : ���o���S�܂Ƃߕ\������
//    pml->priority_list : ���o�t���[�����������̃��X�g
//  �ipml->total_valid[���S�ԍ�-1] = 0 �ݒ�ɂ��s�v���S�������j
//---------------------------------------------------------------------
void MultLogoFind_TotalResult(MLOGO_DATASET* pml){
	LOGO_DATASET* plogo;
	int i, j, k, ins;
	int oanum, autosel;
	int n_detect, n_others, n_disable;
	short isel, jsel;
	short prior_detect[LOGONUM_MAX];			// CM���o�L�����S�D�揇��
	short prior_others[LOGONUM_MAX];			// CM���o�O���S�D�揇��
	short prior_disable[LOGONUM_MAX];			// �������f���S�D�揇��

	oanum = pml->oanum;
	// sort logo priority
	for(i = 0; i < LOGONUM_MAX; i++){
		pml->priority_list[i] = (short) i;
	}
	if (oanum > 1 && oanum <= LOGONUM_MAX){
		for(i = oanum-1; i > 0; i--){
			for(j = i-1; j >= 0; j--){
				isel = pml->priority_list[i];
				jsel = pml->priority_list[j];
				if (pml->total_frame[isel] > pml->total_frame[jsel]){
					pml->priority_list[i] = jsel;
					pml->priority_list[j] = isel;
				}
			}
		}
	}
	// add as result data
	LogoResultInit( &(pml->logoresult) );
	if (oanum > 0){
		if (pml->oasel == 0){       // option "-oasel 0"
			autosel = 1;
		}
		else{
			autosel = 0;
		}
		for(i = 0; i < LOGONUM_MAX; i++){
			isel     = pml->priority_list[i];
			plogo    = pml->all_logodata[isel];
			if (pml->total_valid[isel] == 0){
			}
			else if (isel < oanum){             // within -oanum option logo
				if (i == 0 || pml->oasel != 1){ // �D�揇�ʂ���ԍ������S��������
					ins = LogoResultAdd( &(pml->logoresult), plogo, autosel );
					if (ins == 0){                  // ���S�ǉ����s��Ȃ��ꍇ
						pml->total_valid[isel] = 0; // invalidate logo
					}
				}
				else{                               // ���S�ǉ����s��Ȃ��ꍇ
					pml->total_valid[isel] = 0;     // invalidate logo
				}
			}
		}
	}
	// set each kind of logo priority
	n_detect = 0;
	n_others = 0;
	n_disable = 0;
	for(i=0; i < LOGONUM_MAX; i++){
		isel = pml->priority_list[i];
		if (pml->total_frame[isel] > 0){
			if (pml->total_valid[isel] == 0){
				prior_disable[n_disable++] = isel;	// �������f���S
			}
			else if (isel >= oanum){
				prior_others[n_others++] = isel;	// CM���o�O���S
			}
			else{
				prior_detect[n_detect++] = isel;	// CM���o�L�����S
			}
		}
	}
	// CM���o�L�����S - CM���o�O���S - �������f���S �̏��ɕ��ёւ�
	k = 0;
	for(i=0; i<n_detect; i++){
		pml->priority_list[k++] = prior_detect[i];
	}
	for(i=0; i<n_others; i++){
		pml->priority_list[k++] = prior_others[i];
	}
	for(i=0; i<n_disable; i++){
		pml->priority_list[k++] = prior_disable[i];
	}
	for(i=k; i<LOGONUM_MAX; i++){
		pml->priority_list[k++] = 0;
	}
	pml->num_detect = n_detect;
	pml->num_others = n_others;
	pml->num_disable = n_disable;
}


//=====================================================================
// Public�֐��F�S�摜���o������Ɏ��s���ă��S�\����Ԃ����o
// ���S���Ԃ����o�i�S���S���o���A�K�v�ȃ��S���ԍ��Z�����s�j
//=====================================================================
void MultLogoFind(MLOGO_DATASET* pml){
	LOGO_DATASET* plogo;
	int i;

	// find logoframe
	for(i = 0; i < LOGONUM_MAX; i++){
		plogo      = pml->all_logodata[i];
		if (plogo != NULL){
			LogoFind( plogo );
			pml->total_frame[i] = LogoGetTotalFrameWithClear( plogo );
			if (pml->total_frame[i] > 0){
				pml->total_valid[i] = 1;
			}
		}
	}
	// �S�̌��ʏ���
	MultLogoFind_TotalResult( pml );

	//"-oa2"�f�o�b�O�p���t���[�������N���[�Y
	MultLogoFind_FileAna2Close( pml );
}



//##### ���ʂ̃t�@�C���o��

//---------------------------------------------------------------------
// "-o"�I�v�V�����Ŏw�肷��t�@�C�����o��
//  �o��
//   �Ԃ�l : 0=����  1=�t�@�C���G���[
//---------------------------------------------------------------------
int MultLogoWrite_OutputAvs(MLOGO_DATASET* pml){
	LOGO_DATASET* plogo;
	const char* logofilename;
	FILE* fpout;
	int i;

	// open files
	fpout = NULL;
	if (pml->opt_outfile != NULL){
		fpout = fopen(pml->opt_outfile, "w");
		if (!fpout){
			fprintf(stderr, "error: failed to create/open '%s'\n", pml->opt_outfile);
			return 1;
		}
	}

	// output for "-o" option
	if (fpout != NULL){
		for(i = 0; i < LOGONUM_MAX; i++){
			logofilename  = pml->all_logofilename[i];
			plogo         = pml->all_logodata[i];
			if (logofilename != NULL && pml->total_valid[i] > 0){
				LogoWriteOutput( plogo, logofilename, fpout, pml->outform );
			}
		}
	}

	// close file
	if (fpout){
		fclose(fpout);
	}

	return 0;
}


//---------------------------------------------------------------------
// �e�ʃ��S��"-oa"�I�v�V�����ɑΉ�����t�@�C���̃��X�g���o��
// ���o���S���A�e���S�ɂ��ă��S���A���o���v���ԁA���̃��S�ԍ����o��
//---------------------------------------------------------------------
void MultLogoWrite_OutputAnaEach_List(MLOGO_DATASET* pml, FILE* fpo_list){
	int i, nid, nmax_loop;
	short isel;
	char stradd[2];

	fprintf(fpo_list, "[logodata]\n");
	fprintf(fpo_list, "LogoTotalN=%d\n", pml->num_detect);
	fprintf(fpo_list, "LogoTotalS=%d\n", pml->num_others);
	fprintf(fpo_list, "LogoTotalX=%d\n", pml->num_disable);
	fprintf(fpo_list, "FrameTotal=%d\n", (int) pml->image_frames);
	nmax_loop = pml->num_detect + pml->num_others + pml->num_disable;
	for(i=0; i < nmax_loop; i++){
		isel = pml->priority_list[i];
		if (i < pml->num_detect){
			nid = i + 1;
			strcpy(stradd, "N");
		}
		else if (i < pml->num_detect + pml->num_others){
			nid = i - pml->num_detect + 1;
			strcpy(stradd, "S");
		}
		else{
			nid = i - (pml->num_detect + pml->num_others) + 1;
			strcpy(stradd, "X");
		}
		fprintf(fpo_list, "LogoName_%s%d=%s\n",
				stradd, nid, pml->all_logofilename[isel]);
		fprintf(fpo_list, "FrameSum_%s%d=%d\n",
				stradd, nid, (int) pml->total_frame[isel]);
		fprintf(fpo_list, "OrgLogoNum_%s%d=%d\n",
				stradd, nid, isel+1);
	}
}


//---------------------------------------------------------------------
// �e�ʃ��S��"-oa"�I�v�V�����ɑΉ�����t�@�C�����o��
//  �o��
//   �Ԃ�l : 0=����  1=�t�@�C���G���[  2=�������m�ۃG���[
//---------------------------------------------------------------------
int MultLogoWrite_OutputAnaEach(MLOGO_DATASET* pml){
	LOGO_DATASET* plogo;
	const char* anafile;
	FILE* fpo_list;
	FILE* fpo_ana;
	char* newstr;
	char* newstr1;
	char* newstr2;
	char* tmpstr;
	char stradd[2];
	int i, ret;
	int nmax_loop, nid;
	short isel;

	ret = 0;
	anafile = pml->opt_anafile;
	// output each frame result (each logo result of "-oa" option)
	if (anafile != NULL){					// newstr : "_list.ini"�̈�m��
		newstr  = (char *)malloc( (strlen(anafile) +10 + 1) * sizeof(char) );
		newstr1 = (char *)malloc( (strlen(anafile)     + 1) * sizeof(char) );
		newstr2 = (char *)malloc( (strlen(anafile)     + 1) * sizeof(char) );
		if (newstr == NULL || newstr1 == NULL || newstr2 == NULL){
			fprintf(stderr, "error:failed in memory allocation.\n");
			ret = 2;
		}
		else{
			// �g���q�O��newstr1�A�g���q��newstr2 �ɑ��
			strcpy(newstr1, anafile);
			tmpstr = strrchr(newstr1, '.');
			if (tmpstr != NULL){
				strcpy(newstr2, tmpstr);
				strcpy(tmpstr, "");
			}
			else{
				strcpy(newstr2, "");
			}
			// �ʃ��S��񃊃X�g���o��
			if ((pml->oamask & 8) == 0){
				// �o�͂���t�@�C������ newstr �ɑ���i�K�v������malloc�Ŏ��O�m�ہj
				sprintf(newstr, "%s_list.ini", newstr1);
				fpo_list = fopen(newstr, "w");
				if (!fpo_list){
					fprintf(stderr, "error: failed to create/open '%s'\n", newstr);
					ret = 1;
				}
				else{	// INI�w�b�_�A���̑������o��
					MultLogoWrite_OutputAnaEach_List(pml, fpo_list);
				}
			}
			else{
				fpo_list = NULL;
			}

			// �e���S�f�[�^�̕\�����Ԃ��o��
			nmax_loop = pml->num_detect + pml->num_others + pml->num_disable;
			for(i=0; i < nmax_loop; i++){
				// ���S��ނƔԍ����擾
				isel = pml->priority_list[i];
				if (i < pml->num_detect){
					nid = i + 1;
					strcpy(stradd, "");
					if ((pml->oamask & 4) != 0){
						nid = -1;
					}
				}
				else if (i < pml->num_detect + pml->num_others){
					nid = i - pml->num_detect + 1;
					strcpy(stradd, "S");
					if ((pml->oamask & 2) != 0){
						nid = -1;
					}
				}
				else{
					nid = i - (pml->num_detect + pml->num_others) + 1;
					strcpy(stradd, "X");
					if ((pml->oamask & 1) != 0){
						nid = -1;
					}
				}
				// ���S���o��
				plogo      = pml->all_logodata[isel];
				if (plogo != NULL && nid > 0){
					// �o�͂���t�@�C������ newstr �ɑ���i�K�v������malloc�Ŏ��O�m�ہj
					sprintf(newstr, "%s_%s%d%s", newstr1, stradd, nid, newstr2);
					fpo_ana = fopen(newstr, "w");
					if (!fpo_ana){
						fprintf(stderr, "error: failed to create/open '%s'\n", newstr);
						ret = 1;
						break;
					}
					LogoWriteFind( plogo, fpo_ana );
					fclose(fpo_ana);

					// �o�͂����t�@�C�������ʃ��S��񃊃X�g�ɒǉ����鏈��
					if (fpo_list != NULL){
						if (strlen(stradd) == 0){
							strcpy(stradd, "N");
						}
						fprintf(fpo_list, "oaFileName_%s%d=%s\n",
								stradd, nid, newstr);
					}
				}
			}
			if (fpo_list != NULL){
			    fclose(fpo_list);
			}
		}
		if (newstr2 != NULL) free(newstr2);
		if (newstr1 != NULL) free(newstr1);
		if (newstr  != NULL) free(newstr);
	}
	return ret;
}


//---------------------------------------------------------------------
// "-oa"�I�v�V�����Ŏw�肷��t�@�C�����o��
//  �o��
//   �Ԃ�l : 0=����  1=�t�@�C���G���[
//---------------------------------------------------------------------
int MultLogoWrite_OutputAnaTotal(MLOGO_DATASET* pml){
	const char* anafile;
	FILE* fpo_ana;

	// output frame result ("-oa" option)
	anafile = pml->opt_anafile;
	if (anafile != NULL){
		fpo_ana = fopen(anafile, "w");
		if (!fpo_ana){
			fprintf(stderr, "error: failed to create/open '%s'\n", anafile);
			return 1;
		}
		LogoResultWrite( &(pml->logoresult), fpo_ana );
		fclose(fpo_ana);
	}
	return 0;
}


//=====================================================================
// Public�֐��F���ʃt�@�C���o��
// �ŏI���ʂ��t�@�C���֏o��
//  �o��
//   �Ԃ�l : 0=����  1=�t�@�C���G���[ 2=�������m�ۃG���[
//=====================================================================
int MultLogoWrite(MLOGO_DATASET* pml){
	int errnum, retval;

	retval = 0;
	// AVS�`�����S���o�� "-o"�I�v�V����
	errnum = MultLogoWrite_OutputAvs(pml);
	if (errnum != 0){
		retval = errnum;
	}

	// ���Z���S���o���ʂ��o�� "-oa"�I�v�V����
	errnum = MultLogoWrite_OutputAnaTotal(pml);
	if (errnum != 0){
		retval = errnum;
	}

	// �e���S���o���ʂ��o�� �e�ʃ��S��"-oa"�I�v�V����
	errnum = MultLogoWrite_OutputAnaEach(pml);
	if (errnum != 0){
		retval = errnum;
	}

	return retval;
}


