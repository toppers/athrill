/*
 *  TTSP
 *      TOPPERS Test Suite Package
 * 
 *  Copyright (C) 2010-2011 by Center for Embedded Computing Systems
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *  Copyright (C) 2010-2011 by Digital Craft Inc.
 *  Copyright (C) 2010-2011 by NEC Communication Systems, Ltd.
 *  Copyright (C) 2010-2011 by FUJISOFT INCORPORATED
 *  Copyright (C) 2010-2011 by Industrial Technology Institute,
 *								Miyagi Prefectural Government, JAPAN
 * 
 *  �嵭����Ԥϡ��ʲ���(1)��(4)�ξ������������˸¤ꡤ�ܥ��եȥ���
 *  �����ܥ��եȥ���������Ѥ�����Τ�ޤࡥ�ʲ�Ʊ���ˤ���ѡ�ʣ������
 *  �ѡ������ۡʰʲ������ѤȸƤ֡ˤ��뤳�Ȥ�̵���ǵ������롥
 *  (1) �ܥ��եȥ������򥽡��������ɤη������Ѥ�����ˤϡ��嵭������
 *      ��ɽ�����������Ѿ�浪��Ӳ�����̵�ݾڵ��꤬�����Τޤޤη��ǥ���
 *      ����������˴ޤޤ�Ƥ��뤳�ȡ�
 *  (2) �ܥ��եȥ������򡤥饤�֥������ʤɡ�¾�Υ��եȥ�������ȯ�˻�
 *      �ѤǤ�����Ǻ����ۤ�����ˤϡ������ۤ�ȼ���ɥ�����ȡ�����
 *      �ԥޥ˥奢��ʤɡˤˡ��嵭�����ɽ�����������Ѿ�浪��Ӳ���
 *      ��̵�ݾڵ����Ǻܤ��뤳�ȡ�
 *  (3) �ܥ��եȥ������򡤵�����Ȥ߹���ʤɡ�¾�Υ��եȥ�������ȯ�˻�
 *      �ѤǤ��ʤ����Ǻ����ۤ�����ˤϡ����Τ����줫�ξ�����������
 *      �ȡ�
 *    (a) �����ۤ�ȼ���ɥ�����ȡ����Ѽԥޥ˥奢��ʤɡˤˡ��嵭����
 *        �ɽ�����������Ѿ�浪��Ӳ�����̵�ݾڵ����Ǻܤ��뤳�ȡ�
 *    (b) �����ۤη��֤��̤�������ˡ�ˤ�äơ�TOPPERS�ץ��������Ȥ�
 *        ��𤹤뤳�ȡ�
 *  (4) �ܥ��եȥ����������Ѥˤ��ľ��Ū�ޤ��ϴ���Ū�������뤤���ʤ�»
 *      ������⡤�嵭����Ԥ����TOPPERS�ץ��������Ȥ����դ��뤳�ȡ�
 *      �ޤ����ܥ��եȥ������Υ桼���ޤ��ϥ���ɥ桼������Τ����ʤ���
 *      ͳ�˴�Ť����ᤫ��⡤�嵭����Ԥ����TOPPERS�ץ��������Ȥ�
 *      ���դ��뤳�ȡ�
 * 
 *  �ܥ��եȥ������ϡ�̵�ݾڤ��󶡤���Ƥ����ΤǤ��롥�嵭����Ԥ�
 *  ���TOPPERS�ץ��������Ȥϡ��ܥ��եȥ������˴ؤ��ơ�����λ�����Ū
 *  ���Ф���Ŭ������ޤ�ơ������ʤ��ݾڤ�Ԥ�ʤ����ޤ����ܥ��եȥ���
 *  �������Ѥˤ��ľ��Ū�ޤ��ϴ���Ū�������������ʤ�»���˴ؤ��Ƥ⡤��
 *  ����Ǥ�����ʤ���
 * 
 *  $Id: ttsp_test_lib.h 2 2012-05-09 02:23:52Z nces-shigihara $
 */

/* 
 *		�ƥ��ȥץ�������ѥ饤�֥��
 */

#ifndef TTSP_TEST_LIB_H
#define TTSP_TEST_LIB_H

#include <t_stddef.h>
#include <queue.h>

#include <kernel/kernel_impl.h>
#include <kernel/check.h>
#include <kernel/task.h>
#include <kernel/wait.h>
#include <kernel/semaphore.h>
#include <kernel/eventflag.h>
#include <kernel/dataqueue.h>
#include <kernel/pridataq.h>
#include <kernel/mailbox.h>
#include <kernel/mempfix.h>
#include <kernel/time_event.h>
#include <kernel/alarm.h>
#include <kernel/cyclic.h>

/*
 *  �������åȰ�¸�����
 */
#include "ttsp_target_test.h"

/*
 *  ���ʿ��Ǵؿ��η�
 */
typedef ER (*BIT_FUNC)(void);

/*
 *  ���ʿ��Ǵؿ�������
 */
extern void	set_bit_func(BIT_FUNC bit_func);

/*
 *  �ƥ��ȥ饤�֥�����ѿ������
 */
extern void	ttsp_initialize_test_lib(void);

/*
 *  �����å��ݥ����
 */
extern void	ttsp_check_point(uint_t count);

/*
 *  �����å��ݥ���Ȥ�count�ˤʤ�Τ��Ԥ�
 */
extern void	ttsp_wait_check_point(uint_t count);

/*
 *  ��λ�����å��ݥ����
 */
extern void	ttsp_check_finish(uint_t count);

/*
 *	�����å��ݥ�����̲�ξ��ּ���
 */
extern bool_t	ttsp_get_cp_state(void);

/*
 *	�����å��ݥ�����̲�ξ�������
 */
extern void	ttsp_set_cp_state(bool_t state);

/*
 *  �������å�
 */
extern void	_check_assert(const char *expr, const char *file, int_t line);
#define check_assert(exp) \
	((void)(!(exp) ? (_check_assert(#exp, __FILE__, __LINE__), 0) : 0))

/*
 *  ���顼�����ɥ����å�
 */
extern void	_check_ercd(ER ercd, const char *file, int_t line);
#define check_ercd(ercd, expected_ercd) \
	((void)((ercd) != (expected_ercd) ? \
					(_check_ercd(ercd, __FILE__, __LINE__), 0) : 0))


/*
 *  ����ƥ����Ȥ˱�����CPU���å�
 */
Inline bool_t
ttsp_loc_cpu(void)
{
	bool_t	locked = false;

	if (!sense_context()) {
		if (!t_sense_lock()) {
			t_lock_cpu();
			locked = true;
		}
	}
	else {
		if (!i_sense_lock()) {
			i_lock_cpu();
			locked = true;
		}
	}

	return locked;
}

/*
 *  ����ƥ����Ȥ˱�����CPU���å����
 */
Inline void
ttsp_unl_cpu(bool_t locked)
{
	if (locked) {
		if (!sense_context()) {
			t_unlock_cpu();
		}
		else {
			i_unlock_cpu();
		}
	}
}


/*
 *  ref_tsk���شؿ�
 */
typedef struct t_ttsp_rtsk {
	STAT		tskstat;	/* ���������� */
	PRI			tskpri;		/* �������θ���ͥ���� */
	PRI			tskbpri;	/* �������Υ١���ͥ���� */
	STAT		tskwait;	/* �Ԥ��װ� */
	ID			wobjid;		/* �Ԥ��оݤΥ��֥������Ȥ�ID */
	TMO			lefttmo;	/* �����ॢ���Ȥ���ޤǤλ��� */
	uint_t		actcnt;		/* ��ư�׵ᥭ�塼���󥰿� */
	uint_t		wupcnt;		/* �����׵ᥭ�塼���󥰿� */
	ATR			tskatr;		/* ������°�� */
	intptr_t	exinf;		/* �������γ�ĥ���� */
	PRI			itskpri;	/* �������ε�ư��ͥ���� */
	SIZE		stksz;		/* �����å��ΰ�Υ����� */
	void		*stk;		/* �����å��ΰ����Ƭ���� */
	uint_t		porder;		/* Ʊ��ͥ���٥�������Ǥ�ͥ���� */
} T_TTSP_RTSK;

extern ER ttsp_ref_tsk(ID tskid, T_TTSP_RTSK *pk_rtsk);



/*
 *  ref_tex���شؿ�
 */
typedef struct t_ttsp_rtex {
	STAT	texstat;	/* �������㳰�����ξ��� */
	TEXPTN	pndptn;		/* ��α�㳰�װ� */
	ATR		texatr;		/* �������㳰�����롼����°�� */
} T_TTSP_RTEX;

extern ER ttsp_ref_tex(ID tskid, T_TTSP_RTEX *pk_rtex);



/*
 *  ref_sem���شؿ�
 */
typedef struct t_ttsp_rsem {
	uint_t	semcnt;		/* ���ޥե��λ񸻿� */
	ATR		sematr;		/* ���ޥե�°�� */
	uint_t	isemcnt;	/* ���ޥե��ν���񸻿� */
	uint_t	maxsem;		/* ���ޥե��κ���񸻿� */
	uint_t	waitcnt;	/* �Ԥ��������ο� */
} T_TTSP_RSEM;

extern ER ttsp_ref_sem(ID semid, T_TTSP_RSEM *pk_rsem);


/*
 *  ���ޥե����Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_wait_sem(ID semid, uint_t order, ID *p_tskid);



/*
 *  ref_flg���شؿ�
 */
typedef struct t_ttsp_rflg {
	FLGPTN	flgptn;		/* ���٥�ȥե饰�θ��ߤΥӥåȥѥ����� */
	ATR		flgatr;		/* ���٥�ȥե饰°�� */
	FLGPTN	iflgptn;	/* ���٥�ȥե饰�Υӥåȥѥ�����ν���� */
	uint_t	waitcnt;	/* �Ԥ��������ο� */
} T_TTSP_RFLG;

extern ER ttsp_ref_flg(ID flgid, T_TTSP_RFLG *pk_rflg);


/*
 *  ���٥�ȥե饰���Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_wait_flg(ID flgid, uint_t order, ID *p_tskid, FLGPTN *p_waiptn, MODE *p_wfmode);



/*
 *  ref_dtq���شؿ�
 */
typedef struct t_ttsp_rdtq {
	uint_t	sdtqcnt;	/* �ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ����ο� */
	ATR		dtqatr;		/* �ǡ������塼°�� */
	uint_t	dtqcnt;		/* �ǡ������塼������ */
	uint_t	swaitcnt;	/* �����Ԥ��������ο� */
	uint_t	rwaitcnt;	/* �����Ԥ��������ο� */
} T_TTSP_RDTQ;

extern ER ttsp_ref_dtq(ID dtqid, T_TTSP_RDTQ *pk_rdtq);


/*
 *  �ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ������ȴؿ�
 */
extern ER ttsp_ref_data(ID dtqid, uint_t index, intptr_t *p_data);


/*
 *  �ǡ������塼�������Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_swait_dtq(ID dtqid, uint_t order, ID *p_tskid, intptr_t *p_data);


/*
 *  �ǡ������塼�μ����Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_rwait_dtq(ID dtqid, uint_t order, ID *p_tskid);



/*
 *  ref_pdq���شؿ�
 */
typedef struct t_ttsp_rpdq {
	uint_t	spdqcnt;		/* ͥ���٥ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ����ο� */
	ATR		pdqatr;			/* ͥ���٥ǡ������塼°�� */
	uint_t	pdqcnt;			/* ͥ���٥ǡ������塼������ */
	PRI		maxdpri;		/* �ǡ���ͥ���٤κ����� */
	uint_t	swaitcnt;		/* �����Ԥ��������ο� */
	uint_t	rwaitcnt;		/* �����Ԥ��������ο� */
} T_TTSP_RPDQ;

extern ER ttsp_ref_pdq(ID pdqid, T_TTSP_RPDQ *pk_rpdq);


/*
 *  ͥ���٥ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ������ȴؿ�
 */
extern ER ttsp_ref_pridata(ID pdqid, uint_t index, intptr_t *p_data, PRI *p_datapri);


/*
 *  ͥ���٥ǡ������塼�������Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_swait_pdq(ID pdqid, uint_t order, ID *p_tskid, intptr_t *p_data, PRI *p_datapri);


/*
 *  ͥ���٥ǡ������塼�μ����Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_rwait_pdq(ID pdqid, uint_t order, ID *p_tskid);



/*
 *  ref_mbx���شؿ�
 */
typedef struct t_ttsp_rmbx {
	uint_t	msgcnt;		/* �᡼��ܥå����ˤĤʤ���Ƥ����å������ο� */
	ATR		mbxatr;		/* �᡼��ܥå���°�� */
	PRI		maxmpri;	/* ��å�����ͥ���٤κ����� */
	uint_t	rwaitcnt;	/* �����Ԥ��������ο� */
} T_TTSP_RMBX;

extern ER ttsp_ref_mbx(ID mbxid, T_TTSP_RMBX *pk_rmbx);


/*
 *  �᡼��ܥå����ˤĤʤ���Ƥ���ǡ������ȴؿ�
 */
extern ER ttsp_ref_msg(ID mbxid, uint_t index, T_MSG **pp_msg);


/*
 *  �᡼��ܥå����μ����Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_rwait_mbx(ID mbxid, uint_t order, ID *p_tskid);



/*
 *  ref_mpf���شؿ�
 */
typedef struct t_ttsp_rmpf {
	uint_t	fblkcnt;	/* ����Ĺ����ס����ΰ�ζ��������ΰ�˳���դ��뤳�Ȥ��Ǥ������Ĺ����֥��å��ο� */
	ATR		mpfatr;		/* ����Ĺ����ס���°�� */
	uint_t	blkcnt;		/* ����֥��å��� */
	uint_t	blksz;		/* ����֥��å��Υ����� */
	uint_t	waitcnt;	/* �Ԥ��������ο� */
	void	*mpf;		/* ����Ĺ����ס����ΰ����Ƭ���� */
} T_TTSP_RMPF;

extern ER ttsp_ref_mpf(ID mpfid, T_TTSP_RMPF *pk_rmpf);


/*
 *  ����Ĺ����ס�����Ԥ����������ȴؿ�
 */
extern ER ttsp_ref_wait_mpf(ID mpfid, uint_t order, ID *p_tskid);



/*
 *  ref_cyc���شؿ�
 */
typedef struct t_ttsp_rcyc {
	STAT		cycstat;	/* �����ϥ�ɥ��ư����� */
	RELTIM		lefttim;	/* ���˼����ϥ�ɥ��ư�������ޤǤ����л��� */
	ATR			cycatr;		/* �����ϥ�ɥ�°�� */
	intptr_t	exinf;		/* �����ϥ�ɥ�γ�ĥ���� */
	RELTIM		cyctim;		/* �����ϥ�ɥ�ε�ư���� */
	RELTIM		cycphs;		/* �����ϥ�ɥ�ε�ư���� */
} T_TTSP_RCYC;

extern ER ttsp_ref_cyc(ID cycid, T_TTSP_RCYC *pk_rcyc);



/*
 *  ref_alm���شؿ�
 */
typedef struct t_ttsp_ralm {
	STAT		almstat;	/* ���顼��ϥ�ɥ��ư����� */
	RELTIM		lefttim;	/* ���顼��ϥ�ɥ��ư�������ޤǤ����л��� */
	ATR			almatr;		/* ���顼��ϥ�ɥ�°�� */
	intptr_t	exinf;		/* ���顼��ϥ�ɥ�γ�ĥ���� */
} T_TTSP_RALM;

extern ER ttsp_ref_alm(ID almid, T_TTSP_RALM *pk_ralm);



/*
 *  get_ipm���شؿ�
 */
extern ER ttsp_get_ipm(PRI *p_intpri);

#endif /* TTSP_TEST_LIB_H */