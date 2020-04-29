/*====================================================================
* ���S���o�p�w�b�_�t�@�C��
*===================================================================*/

#ifndef ___LOGOSET_H
#define ___LOGOSET_H

#define DEBUG_PRINT
//#define DEBUG_PRINT_DET


/* ���S�\������p�p�����[�^�����l */
#define DEF_LOGO_RATE_TH_LOGO   50		/* ���S�\���Ɣ��f���錋��臒l(0-100) */
#define DEF_LOGO_NUM_FADEIN     0		/* �t�F�[�h�C���t���[���� */
#define DEF_LOGO_NUM_FADEOUT    0		/* �t�F�[�h�A�E�g�t���[���� */
#define DEF_LOGO_NUM_CUTLEFT    500		/* �J�n����̃��S���o���Ȃ��t���[���� */
#define DEF_LOGO_NUM_CUTRIGHT   150		/* �I�����O�̃��S���o���Ȃ��t���[����*/
#define DEF_LOGO_NUM_ONWIDTH    25		/* ���S�\��ON/OFF�؂�ւ��Œ�t���[���� */
#define DEF_LOGO_NUM_ONLEVEL    21		/* ���S�\��ON/OFF�؂�ւ�unclear���x�� */
#define DEF_LOGO_NUM_OFFWIDTH   0		/* ���S�\��OFF�؂�ւ��Œ�t���[����(0�̎�ON�l�g�p) */
#define DEF_LOGO_THRES_YMAX     235		/* �ő�P�x�l */
#define DEF_LOGO_THRES_YMIN     16		/* �ŏ��P�x�l */
#define DEF_LOGO_THRES_YEDGE    255		/* �G�b�W���o���s���ő�P�x�l */
#define DEF_LOGO_THRES_YDIF     40		/* �G�b�W���o���s���Œ�P�x��x16 */
#define DEF_LOGO_THRES_YSETDIF  222		/* �G�b�W���o���s���P�x�l��ydif�l�ύX�p */
#define DEF_LOGO_THRES_YOFFEDG  255		/* �G�b�W���o�Ń��S�Ȃ���������Ȃ��P�x�l */
#define DEF_LOGO_NUM_AREASET    0		/* �G���A���o���s���G���A�̐ݒ� */
#define DEF_LOGO_NUM_CLRRATE    8		/* �s���m���S�𖳌��Ɣ��f����t���[������ */
#define DEF_LOGO_AUTO_FADE      8		/* �������o�t�F�[�h */
#define DEF_LOGO_AUTO_YEDGE     220		/* BS11���o���̃G�b�W���o���s���ő�P�x�l */
#define DEF_LOGO_AUTO_YDIFMIN   24      /* ydif��������������ꍇ�̍Œ�l */

/* �ݒ肷��臒l�i�[ */
typedef struct {
	short	rate_th_logo;			/* ���S�\���Ɣ��f���錋��臒l(0-100) */
	short	num_fadein;				/* �t�F�[�h�C���t���[���� */
	short	num_fadeout;			/* �t�F�[�h�A�E�g�t���[���� */
	short	num_cutleft;			/* �J�n����̃��S���o���Ȃ��t���[���� */
	short	num_cutright;			/* �I�����O�̃��S���o���Ȃ��t���[����*/
	short	num_onwidth;			/* ���S�\��ON/OFF�؂�ւ��Œ�t���[���� */
	short	num_onlevel;			/* ���S�\��ON/OFF�؂�ւ�unclear���x�� */
	short	num_offwidth;			/* ���S�\��OFF�؂�ւ��Œ�t���[���� */
	short	thres_ymax;				/* �ő�P�x�l */
	short	thres_ymin;				/* �ŏ��P�x�l */
	short	thres_yedge;			/* �G�b�W���o���s���ő�P�x�l */
	short	thres_ydif;				/* �G�b�W���o���s���Œ�P�x��x16 */
	short	thres_ysetdif;			/* �G�b�W���o���s���P�x�l��ydif�l�ύX�p */
	short	thres_yoffedg;			/* �G�b�W���o�Ń��S�Ȃ���������Ȃ��P�x�l */
	short	num_areaset;			/* �G���A���o���s���G���A�̐ݒ� */
	short	num_clrrate;			/* �s���m���S�𖳌��Ɣ��f����t���[������ */

	short	auto_fade;				/* �������o�t�F�[�h */
	short	auto_bs11;				/* BS11�������o�ithres_yedge�����ݒ�j */
} LOGO_THRESREC;



/* ���S�f�[�^ */
typedef struct {
	LOGO_FILE_HEADER lfh;
	LOGO_HEADER      lgh;
	LOGO_PIXEL       *ptr;
} LOGO_READREC;


#define LOGO_AREANUM 7		/* �G���A���o�̃G���A�ő吔 */

/* �G���A���o�������Z�p */
typedef struct {
	long     total_area;			/* �G���A���o�Ŏg�p����Œ��f�� */
	long     total_area1;			/* �w�i�G���A�擾�ӏ��̑��� */
	long     total_area2;			/* ���S�G���A�擾�ӏ��̑��� */
	long     xmin;					/* �G���A���o�Ŏg�p����G���A���W */
	long     xmax;					/* �G���A���o�Ŏg�p����G���A���W */
	long     ymin;					/* �G���A���o�Ŏg�p����G���A���W */
	long     ymax;					/* �G���A���o�Ŏg�p����G���A���W */
} LOGO_PARAMAREAREC;

/* �������Z�p���S�f�[�^ */
typedef struct {
	short    yx, yy;      			/* ��{�ʒu               */
	short    yh, yw;      			/* ���S�����E��           */
	short    *dp_y;					/* ���S���ߓx */
	short    *y;					/* ���S�P�x   */

	char     *dif_y_col;			/* �G�b�W���o�i���j */
	char     *dif_y_row;			/* �G�b�W���o�i�c�j */
	short    *area_y;				/* ���S�G���A���i0:���� 1:�w�i 2:���S�j */
	short    most_logo_y;			/* �ł��������S�̋P�x�l */
	short    thres_dp_y;			/* ���S���ߓx臒l */

	/* ���S���e�m�F�p */
	long     total_dif;				/* �G�b�W���o�ӏ��̑��� */
	long     total_dif_c1;			/* �G�b�W���o�ӏ��̑����i�C���^�[���[�X�����p�j */
	long     total_dif_c2;			/* �G�b�W���o�ӏ��̑����i�C���^�[���[�X�����p�j */

	long     num_local;				/* �G���A���o���[�J���G���A�� */
	short    scale_area2;			/* �G���A���o���̃��S�G���A�P�x */
	LOGO_PARAMAREAREC area[LOGO_AREANUM];		/* �G���A���o�Ŏg�p����G���A��� */
} LOGO_PARAMREC;


/* �t�F�[�h���f�p�X�e�b�v�� */
#define LOGO_FADE_STEP 31			/* �ő�t�F�[�h�v�Z�X�e�b�v���i�t�F�[�h�l�{�P�j */
#define LOGO_FADE_OFST 2			/* �q�X�g�O�����O��ێ��� */
#define LOGO_FADE_MAXLEVEL LOGO_FADE_STEP+(LOGO_FADE_OFST*2)+1	/* �t�F�[�h�q�X�g�O�����ő�ێ��� */

/* �G���A���o�p�̂P�摜���̃��S���Z�f�[�^�i�[ */
typedef struct {
	long	hista_areaoff[256];		/* �G���A���o�p���S�Ȃ��P�x���a */
	long	hista_areaon[256];		/* �G���A���o�p���S����P�x���a */
	long	hista_areacal[256];		/* �G���A���o�p���S�\�z�P�x���a */
	long	num_hista_off;			/* �G���A���o�p���S�Ȃ��P�x�� */
	long	num_hista_on;			/* �G���A���o�p���S����P�x�� */

	short	rate_areavalid;			/* �G���A���o���S�L���̈抄�� */
	short	rate_arealogo;			/* �G���A���o���S���� */
	short	diff_arealogo;			/* �G���A���o���S�L���̋P�x���i�S�́j */
	short	diff1_arealogo;			/* �G���A���o���S�L���̋P�x���i���S�Ȃ��G���A�j */
	short	diff2_arealogo;			/* �G���A���o���S�L���̋P�x���i���S����G���A�j */
	short	vari_arealogo_off;		/* �G���A���o���S�L���̎��ӕ��U�i���S�Ȃ��G���A�j */
	short	vari_arealogo_cal;		/* �G���A���o���S�L���̎��ӕ��U�i���S�v�Z�G���A�j */
	short	vari_arealogo_on;		/* �G���A���o���S�L���̎��ӕ��U�i���S����G���A�j */
} LOGO_CALCAREAREC;

/* �P�摜���̃��S���Z�f�[�^�i�[ */
typedef struct {
	long	hist_y[LOGO_FADE_MAXLEVEL];	/* �t�F�[�h�p�P�x�q�X�g�O���� */
	long	fade_calcstep;				/* �t�F�[�h�p�P�x�q�X�g�O�����̌v�Z�p�X�e�b�v�� */
	
	long	cnt_logooff;			/* ���S�Ȃ��Ɣ��f������ */
	long	cnt_logoon;				/* ���S����Ɣ��f������ */
	long	cnt_logomv;				/* ���S���o�s�Ɣ��f������ */
	long	cnt_logovc;				/* ���S���o�L���J�E���g�� */

	long	cntf_logooff;			/* ���S�Ȃ��Ɣ��f�����񐔁i���U���j */
	long	cntf_logoon;			/* ���S����Ɣ��f�����񐔁i���U���j */
	long	cntf_logost;			/* ���S�U���Ȃ����f�����񐔁i���U���j */
	long	cntf_logovc;			/* ���S���o�L���J�E���g���i���U���j */
	long	cntopp_logoon;			/* ���S����Ɣ��f�����񐔁i��f���]���j */

	long	sum_areaoff;			/* ���U���t�F�[�h�p���S�Ȃ��P�x���a */
	long	sum_areaon;				/* ���U���t�F�[�h�p���S����P�x���a */
	long	sum_areadif;			/* ���U���t�F�[�h�p���S���z�P�x���̑��a */
	long	sum_areanum;			/* ���U���t�F�[�h�p���S���f�� */

	long	cnt_offedg;				/* �s���m�ȃ��S�Ȃ����f�� */
	long	cntf_offedg;			/* �s���m�ȃ��S�Ȃ����f�񐔁i���U���j */
	long	cnts_logooff;			/* �����ȃ��S�e���������S�Ȃ��Ɣ��f������ */
	long	cnts_logoon;			/* �����ȃ��S�e���������S����Ɣ��f������ */

	long	total_dif;				/* �G�b�W���o�ӏ��̑��� */

	long	vc_cntd_logooff;		/* ���S�Ȃ��Ɣ��f������ */
	long	vc_cntd_logoon;			/* ���S����Ɣ��f������ */
	long	vc_cntf_logooff;		/* ���S�Ȃ��Ɣ��f�����񐔁i���U���j */
	long	vc_cntf_logoon;			/* ���S����Ɣ��f�����񐔁i���U���j */
	long	vc_cntd_voff;			/* ���S�Ȃ��Ɣ��f�����L���s�� */
	long	vc_cntd_von;			/* ���S����Ɣ��f�����L���s�� */
	long	vc_cntf_voff;			/* ���S�Ȃ��Ɣ��f�����L���s���i���U���j */
	long	vc_cntf_von;			/* ���S����Ɣ��f�����L���s���i���U���j */

	short	rate_logoon;			/* ���S���o����(0-100) */
	short	rate_fade;				/* ���S�t�F�[�h��Ԍ��o����(0-100) */
	short	flag_nosample;			/* ���S�T���v���s�����o�s��(0-1) */
	short	flag_nofadesm;			/* ���S�t�F�[�h�T���v���s��(0-1) */
	short	rank_unclear;			/* ���S���o�s����x(0-3,9) */

	short	rate_arealogo;			/* �G���A���o���S���� */
	LOGO_CALCAREAREC area[LOGO_AREANUM];	/* �e�G���A���o���S���Z��� */

	short	rate_areaavg_t;			/* ���U���t�F�[�h�p���S���o����(0-100) */
	long	sum_areanum_t;			/* ���U���t�F�[�h�p���S���o�� */
} LOGO_CALCREC;


/* �L���f�[�^������p */
typedef struct {
	long	vc_cntd_logooff;		/* ���S�Ȃ��Ɣ��f������ */
	long	vc_cntd_logoon;			/* ���S����Ɣ��f������ */
	long	vc_cntf_logooff;		/* ���S�Ȃ��Ɣ��f�����񐔁i���U���j */
	long	vc_cntf_logoon;			/* ���S����Ɣ��f�����񐔁i���U���j */
	long	vc_cntd_voff;			/* ���S�Ȃ��Ɣ��f�����L���s�� */
	long	vc_cntd_von;			/* ���S����Ɣ��f�����L���s�� */
	long	vc_cntf_voff;			/* ���S�Ȃ��Ɣ��f�����L���s���i���U���j */
	long	vc_cntf_von;			/* ���S����Ɣ��f�����L���s���i���U���j */
	long	cntd_last_on;
	long	cntd_last_off;
	long	cntf_last_on;
	long	cntf_last_off;
} LOGO_VCCALCREC;


/* ���S�\�����Ԃ̍ő�g�� */
#define LOGO_FIND_MAX 256

/* �ŏI�f�[�^�ɕK�v�ȏ�� */
typedef struct {
	long	frm_rise;				/* �e�J�n�t���[�� */
	long	frm_fall;				/* �e�I���t���[�� */
	long	frm_rise_l;				/* �e�J�n�t���[�����J�n */
	long	frm_rise_r;				/* �e�J�n�t���[�����I�� */
	long	frm_fall_l;				/* �e�I���t���[�����J�n */
	long	frm_fall_r;				/* �e�I���t���[�����I�� */
	char	fade_rise;				/* �e�J�n�t�F�[�h�C�����(0 or fadein) */
	char	fade_fall;				/* �e�I���t�F�[�h�A�E�g���(0 or fadeout) */
	char	intl_rise;				/* �e�J�n�C���^�[���[�X���(0:ALL 1:TOP 2:BTM) */
	char	intl_fall;				/* �e�I���C���^�[���[�X���(0:ALL 1:TOP 2:BTM) */
} LOGO_OUTSUBREC;

/* �S�t���[�����Z�ɕK�v�ȃf�[�^�i�[ */
typedef struct {
	long	num_frames;				/* �t���[���� */
	char	*rate_logoon;			/* �e�t���[�����S���o����(0-100) */
	char	*rate_fade;				/* �e�t���[�����S�t�F�[�h��Ԍ��o����(0-100) */
	char	*flag_nosample;			/* �e�t���[�����S�T���v���s�����o�s��(b0-1:0-1)  */
	char	*flag_nofadesm;			/* �e�t���[�����S�t�F�[�h�T���v���s�����o�s��(b0-1:0-1)  */
	char	*rate_logooni1;			/* �e�t���[�����S���o���ʃC���^�[���[�X�(0-100) */
	char	*rate_logooni2;			/* �e�t���[�����S���o���ʃC���^�[���[�X����(0-100) */
	char	*rate_fadei1;			/* �e�t���[�����S�t�F�[�h��Ԍ��o����(0-100) */
	char	*rate_fadei2;			/* �e�t���[�����S�t�F�[�h��Ԍ��o����(0-100) */
	char	*rank_unclear;			/* �e�t���[�����S���o�s����x(0-3,9)  */
	char	*flag_logoon;			/* �e�t���[�����S���o���ʂQ�i���\�L(0-1) */

	long	frm_leftstart;			/* �L���J�n�t���[�� */
	long	frm_rightend;			/* �L���I���t���[�� */
	LOGO_OUTSUBREC workres[LOGO_FIND_MAX];	/* ���S�\�����Ԍ��ʂ̍�Ɨ̈� */

	/* �ŏI�o�� */
	long	num_find;					/* ���S�\�����Ԃ̑g�� */
	LOGO_OUTSUBREC res[LOGO_FIND_MAX];	/* ���S�\�����Ԃ̌��� */
} LOGO_FRAMEREC;


/* �f�[�^�ꎮ */
typedef struct {
	LOGO_READREC	readdat;
	LOGO_PARAMREC	paramdat;
	LOGO_CALCREC	calcdat1;
	LOGO_CALCREC	calcdat2;
	LOGO_FRAMEREC	framedat;
	LOGO_THRESREC	thresdat;		/* 臒l�ݒ�l */
} LOGO_DATASET;


/* �ŏI���� */
typedef struct {
	long	num_find;					/* ���S�\�����Ԃ̑g�� */
	LOGO_OUTSUBREC res[LOGO_FIND_MAX];	/* ���S�\�����Ԃ̌��� */
} LOGO_RESULTOUTREC;


#endif
