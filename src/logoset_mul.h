//====================================================================
// �������S�����p�w�b�_�t�@�C��
//====================================================================

#ifndef ___LOGOSET_MUL_H
#define ___LOGOSET_MUL_H


#define FILE_BUFSIZE 512

// max number of logo data
#define LOGONUM_MAX 100
// "-logo"�I�v�V�����Ŏ擾����t�@�C�����ő吔
#define FILELISTNUM_MAX LOGONUM_MAX
// "-oanum"�I�v�V�����̏����l
#define DEF_MLOGO_OANUM  90


// ���S�f�[�^�̊g���q
#define EXTNAME_LOGODATA   ".lgd"
#define EXTNAME_LOGOPARAM  ".logoframe.txt"

// logoframe�����ǂݍ��݃I�v�V�����t�@�C����
#define INIFILE_NAME       "logoframe.ini"
// logoframe�p�Z�N�V������
#define SECTION_LOGOFRAME  "[logoframe]"

// �t�H���_��؂�L��
#define DELIMITER_DIR     '\\'
#define DELIMITER_STRDIR  "\\"


// 臒l�p�����[�^�X�V�ۑ��p�i�X�V�t���O�j
typedef struct {
	char	up_num_fadein;
	char	up_num_fadeout;
	char	up_num_cutleft;
	char	up_num_cutright;
	char	up_num_onwidth;
	char	up_num_onlevel;
	char	up_num_offwidth;
	char	up_thres_ymax;
	char	up_thres_ymin;
	char	up_thres_yedge;
	char	up_thres_ydif;
	char	up_thres_ysetdif;
	char	up_thres_yoffedg;
	char	up_num_areaset;
	char	up_num_clrrate;
	char	up_auto_fade;
	char	up_auto_bs11;
} MLOGO_UPTHRESREC;

// 臒l�p�����[�^�X�V�ۑ��p
typedef struct {
	LOGO_THRESREC		dat;
	MLOGO_UPTHRESREC	up;
} MLOGO_THRESREC;


// �������S�f�[�^�i�[
typedef struct {
	// �Ăяo�����ł��Q�Ƃ���l
	short dispoff;									// �I�v�V���� "-nodisp"
	short paramoff;									// �I�v�V���� "-nodispparam"
	short num_deflogo;								// ��`���S��

	// logoframe�������S�Ή��Ŏg�p����I�v�V����
	short oanum;									// �I�v�V���� "-oanum"
	short oasel;									// �I�v�V���� "-oasel"
	short oamask;									// �I�v�V���� "-oamask"
	short outform;									// �I�v�V���� "-outform"
	char* opt_logofilename;							// �I�v�V���� "-logo"
	char* opt_logoparamfile;						// �I�v�V���� "-logoparam"
	char* opt_outfile;								// �I�v�V���� "-o"
	char* opt_anafile;								// �I�v�V���� "-oa"
	char* opt_ana2file;								// �I�v�V���� "-oa2"
	char* all_logofilename[LOGONUM_MAX];			// ���S�t�@�C�������X�g

	FILE*             fpo_ana2[LOGONUM_MAX];		// �f�o�b�O�p�t�@�C���ێ��p
	LOGO_DATASET*     all_logodata[LOGONUM_MAX];	// �e���S�f�[�^�i�[
	MLOGO_THRESREC    thres_arg;					// 臒l�i�����w��j
	LOGO_RESULTOUTREC logoresult;					// �S�̌��o���ʊi�[

	long  image_frames;								// �摜�f�[�^�t���[����
	long  total_frame[LOGONUM_MAX];					// ���S�\�����ԃt���[�����v
	short total_valid[LOGONUM_MAX];					// �L�����S����
	short priority_list[LOGONUM_MAX];				// ���S�D�揇�ʃ��X�g
	short num_detect;								// CM���o�L�����S��
	short num_others;								// CM���o�O���S��
	short num_disable;								// �������f���S��
} MLOGO_DATASET;

#endif
