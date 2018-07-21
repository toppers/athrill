/*
 *  TTSP
 *      TOPPERS Test Suite Package
 * 
 *  Copyright (C) 2010-2012 by Center for Embedded Computing Systems
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
 *  $Id: ttsp_test_lib.c 8 2012-09-05 10:21:59Z nces-shigihara $
 */

/* 
 *		�ƥ��ȥץ�������ѥ饤�֥��
 */

#include <kernel.h>
#include <sil.h>
#include <t_syslog.h>
#include <t_stdlib.h>
#include "kernel_cfg.h"
#include "ttsp_test_lib.h"
#include "ttsp_target_test.h"

/*
 *  �����å��ݥ�����̲ᥫ������ѿ�
 */ 
static volatile uint_t	check_count[TNUM_PRCID];

/*
 *  �����å��ݥ�����̲�ξ���(true:���false:�۾�)
 */ 
static volatile bool_t	cp_state;

/*
 *	���ʿ��Ǵؿ�
 */
static volatile BIT_FUNC	check_bit_func;

/*
 *  �ХꥢƱ�����ѿ�
 */
static volatile uint_t local_phase[TNUM_PRCID];
static volatile uint_t global_phase;
static volatile bool_t reentrant_check[TNUM_PRCID];

/*
 *  �ޥ���ץ����å��ѥ����å��ݥ�����̲ᥫ������ѿ�
 */
static volatile ID	check_count_mp[TNUM_PRCID];


/*
 *	���ʿ��Ǵؿ�������
 */
void
set_bit_func(BIT_FUNC bit_func)
{
	check_bit_func = bit_func;
}


/*
 *  �ƥ��ȥ饤�֥�����ѿ������
 *  (�ܴؿ������ץ����å��ˤ������������Ƭ��1������¹Ԥ���)
 */
void
ttsp_initialize_test_lib(void)
{
	uint_t i;

	for (i = 0; i < TNUM_PRCID; i++) {
		check_count[i] = 0U;
		local_phase[i] = 0U;
		reentrant_check[i] = false;
	}
	global_phase = 0U;
	check_bit_func = NULL;
	cp_state = true;
}

/*
 *	�����å��ݥ����(ASP��API�ƥ���ή�ѤΤ߻���)
 */
void
ttsp_check_point(uint_t count)
{
	bool_t	errorflag = false;
	ER		rercd;
	ID      prcid;

	SIL_PRE_LOC;

	/*
	 *  ����ߥ��å����֤�
	 */
	SIL_LOC_INT();

	/*
	 *  PRCID����
	 */
	sil_get_pid(&prcid);

	/*
	 *  �������󥹥����å�
	 */
	if (++check_count[prcid - 1] == count) {
		syslog_2(LOG_NOTICE, "PE %d : Check point %d passed.", prcid, count);
	}
	else {
		syslog_2(LOG_ERROR, "## PE %d : Unexpected check point %d.", prcid, count);
		errorflag = true;
		goto error_exit;
	}

	/*
	 *  �����ͥ���������֤θ���
	 */
	if (check_bit_func != NULL) {
		rercd = (*check_bit_func)();
		if (rercd < 0) {
			syslog_2(LOG_ERROR, "## Internal inconsistency detected (%s, %d).",
								itron_strerror(rercd), SERCD(rercd));
			errorflag = true;
			goto error_exit;
		}
	}

  error_exit:
	/*
	 *  ����ߥ��å����֤���
	 */
	SIL_UNL_INT();

	if (errorflag) {
		cp_state = false;
		if (sns_ker() == false) {
			ext_ker();
		}
	}
}

/*
 *	��λ�����å��ݥ����(ASP��API�ƥ���ή�ѤΤ߻���)
 */
void
ttsp_check_finish(uint_t count)
{
	ID	prcid;

	/*
	 *  PRCID����
	 */
	sil_get_pid(&prcid);

	ttsp_check_point(count);
	syslog_1(LOG_NOTICE, "PE %d : All check points passed.", prcid);

	ext_ker();
}

/*
 *	�����å��ݥ�����̲�ξ��ּ���
 */
bool_t
ttsp_get_cp_state(void)
{
	return cp_state;
}

/*
 *	�����å��ݥ�����̲�ξ�������
 */
void
ttsp_set_cp_state(bool_t state)
{
	cp_state = state;
}

/*
 *	�������å��Υ��顼����
 */
void
_check_assert(const char *expr, const char *file, int_t line)
{
	syslog_3(LOG_ERROR, "## Assertion `%s' failed at %s:%u.",
								expr, file, line);
	cp_state = false;
	if (sns_ker() == false) {
		ext_ker();
	}
}

/*
 *	���顼�����ɥ����å��Υ��顼����
 */
void
_check_ercd(ER ercd, const char *file, int_t line)
{
	syslog_3(LOG_ERROR, "## Unexpected error %s detected at %s:%u.",
								itron_strerror(ercd), file, line);
	cp_state = false;
	if (sns_ker() == false) {
		ext_ker();
	}
}

/*
 *  �ХꥢƱ��
 */
void
ttsp_barrier_sync(uint_t phase, uint_t tnum_prcid)
{
	bool_t		errorflag = false;
	volatile	ulong_t i;
	volatile	uint_t flag;
	ID			prcid;
	ulong_t		timeout = 0;

	SIL_PRE_LOC;

	/*
	 *  ����ߥ��å����֤�
	 */
	SIL_LOC_INT();

	/*
	 *  PRCID����
	 *  �ɤΤ褦�ʾ��֤Ǥ�����Ǥ���褦�� sil_get_pid() ����Ѥ���
	 */
	sil_get_pid(&prcid);

	/*
	 * �ꥨ��ȥ��Ȥ��줿���ϥ��顼��λ����
	 */
	if (reentrant_check[prcid - 1]) {
		syslog_1(LOG_ERROR, "## PE %d : ttsp_barrier_sync has re-entranted", prcid);
		syslog_2(LOG_ERROR, "## in \"ttsp_barrier_sync(%d, %d)\".", phase, tnum_prcid);
		errorflag = true;
		goto error_exit;
	}
	reentrant_check[prcid - 1] = true;

	local_phase[prcid - 1] = phase;

	/*
	 *  ����ߥ��å����֤���
	 */
	SIL_UNL_INT();

	/*
	 *  �ХꥢƱ������
	 */
	if (prcid == TOPPERS_MASTER_PRCID) {
		while (1) {
			flag = 0;
			for (i = 0; i < TNUM_PRCID; i++) {
				if (local_phase[i] == phase) {
					flag++;
				}
			}

			/*
			 * �ХꥢƱ����λ�����å�
			 * (�����å����齪λ�����ޤǤϥǥ����ѥå����ʤ��褦����ߥ��å����֤Ȥ���)
			 */
			SIL_LOC_INT();
			if (flag == tnum_prcid) {
				reentrant_check[prcid - 1] = false;
				global_phase = phase;
				/*
				 * ��PE�ΥХꥢƱ����������λ���뤳�Ȥ��Ԥ�
				 * (���˼���phase�ξ����Ԥ�ɬ�פϤʤ�)
				 */
				for (i = 0; i < TNUM_PRCID; i++) {
					timeout = 0;
					while ((reentrant_check[i] == true) && (local_phase[i] == phase)) {
						timeout++;
						if (timeout > TTSP_LOOP_COUNT) {
							syslog_3(LOG_ERROR, "## PE %d : ttsp_barrier_sync(phase:%d) caused a timeout to wait for PE %d", prcid, phase, i + 1);
							syslog_2(LOG_ERROR, "## in \"ttsp_barrier_sync(%d, %d)\".", phase, tnum_prcid);
							errorflag = true;
							goto error_exit;
						}
						sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
					}
				}
				syslog_2(LOG_NOTICE, "PE %d : Barrier sync phase : %d synchronized.", prcid, phase);
				SIL_UNL_INT();
				return;
			}
			SIL_UNL_INT();

			/*
			 * �����ॢ���Ƚ���
			 */
			timeout++;
			if (timeout > TTSP_LOOP_COUNT) {
				syslog_2(LOG_ERROR, "## PE %d : ttsp_barrier_sync(phase:%d) caused a timeout", prcid, phase);
				syslog_2(LOG_ERROR, "## in \"ttsp_barrier_sync(%d, %d)\".", phase, tnum_prcid);
				errorflag = true;
				goto error_exit;
			}
			sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
		}
	}
	else {
		while (1) {
			/*
			 * �ХꥢƱ����λ�����å�
			 * �����å����齪λ�����ޤǤϥǥ����ѥå����ʤ��褦����ߥ��å����֤Ȥ���
			 */
			SIL_LOC_INT();
			if (global_phase == phase) {
				reentrant_check[prcid - 1] = false;
				/*
				 * ��PE�ΥХꥢƱ����������λ���뤳�Ȥ��Ԥ�
				 * (���˼���phase�ξ����Ԥ�ɬ�פϤʤ�)
				 */
				for (i = 0; i < TNUM_PRCID; i++) {
					timeout = 0;
					while ((reentrant_check[i] == true) && (local_phase[i] == phase)) {
						timeout++;
						if (timeout > TTSP_LOOP_COUNT) {
							syslog_3(LOG_ERROR, "## PE %d : ttsp_barrier_sync(phase:%d) caused a timeout to wait for PE %d", prcid, phase, i + 1);
							syslog_2(LOG_ERROR, "## in \"ttsp_barrier_sync(%d, %d)\".", phase, tnum_prcid);
							errorflag = true;
							goto error_exit;
						}
						sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
					}
				}
				syslog_2(LOG_NOTICE, "PE %d : Barrier sync phase : %d synchronized.", prcid, phase);
				SIL_UNL_INT();
				return;
			}
			SIL_UNL_INT();

			/*
			 * �����ॢ���Ƚ���
			 */
			timeout++;
			if (timeout > TTSP_LOOP_COUNT) {
				syslog_2(LOG_ERROR, "## PE %d : ttsp_barrier_sync(phase:%d) caused a timeout", prcid, phase);
				syslog_2(LOG_ERROR, "## in \"ttsp_barrier_sync(%d, %d)\".", phase, tnum_prcid);
				errorflag = true;
				goto error_exit;
			}
			sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
		}
	}

  error_exit:
	if (errorflag) {
		cp_state = false;
		ext_ker();
	}

	/*
	 *  ����ʽ����Ǥ��̤�ʤ��ѥ��Τ���۾ｪλ
	 */
	syslog_2(LOG_ERROR, "## PE %d : ttsp_barrier_sync(phase:%d) occurred an unexpected error.", prcid, phase);
	syslog_2(LOG_ERROR, "## in \"ttsp_barrier_sync(%d, %d)\".", phase, tnum_prcid);
	cp_state = false;
	ext_ker();
}

/*
 *  id�ֹ�Υ����å��ݥ���Ȥ�ʤ�롥 
 */
void
ttsp_mp_check_point(ID id, uint_t count)
{
	bool_t	errorflag = false;
	ER		rercd;
	ID		prcid;

	SIL_PRE_LOC;

	/*
	 *  ����ߥ��å����֤�
	 */
	SIL_LOC_INT();

	/*
	 *  PRCID����
	 */
	sil_get_pid(&prcid);

	/*
	 * �����ǻ��ꤷ���ץ����å�ID�����Ѳ����Ƥ������
	 */
	if (prcid != id) {
		syslog_2(LOG_ERROR, "## PE %d : Processor has changed to %d", id, prcid);
		syslog_2(LOG_ERROR, "## in \"ttsp_mp_check_point(%d, %d)\".", id, count);
		errorflag = true;
		goto error_exit;
	}

	/*
	 *  ID�����å�
	 */
	if (!((id > 0) && (id <= TNUM_PRCID))) {
		syslog_1(LOG_ERROR, "## PE %d : Unexpected ID was specified", prcid);
		syslog_2(LOG_ERROR, "## in \"ttsp_mp_check_point(%d, %d)\".", id, count);
		errorflag = true;
		goto error_exit;
	}

	/*
	 *  �������󥹥����å�
	 */
	if (++check_count_mp[id - 1] == count) {
		syslog_2(LOG_NOTICE, "PE %d : Check point : %d passed.", prcid, count);
	}
	else {
		syslog_2(LOG_ERROR, "## PE %d : Unexpected Check Point : %d", prcid, count);
		syslog_2(LOG_ERROR, "## in \"ttsp_mp_check_point(%d, %d)\".", id, count);
		errorflag = true;
		goto error_exit;
	}

	/*
	 *  �����ͥ���������֤θ���
	 */
	if (check_bit_func != NULL) {
		rercd = (*check_bit_func)();
		if (rercd < 0) {
			syslog_3(LOG_ERROR, "## PE %d : Internal inconsistency detected (%s, %d)",
								prcid, itron_strerror(rercd), SERCD(rercd));
			syslog_2(LOG_ERROR, "## in \"ttsp_mp_check_point(%d, %d)\".", id, count);
			errorflag = true;
			goto error_exit;
		}
	}

  error_exit:
	/*
	 *  ����ߥ��å����֤���
	 */
	SIL_UNL_INT();

	if (errorflag) {
		cp_state = false;
		if (sns_ker() == false) {
			ext_ker();
		}
	}
}


/*
 *  id�ֹ�Υ����å��ݥ���Ȥ�count�ˤʤ�Τ��Ԥġ� 
 */
void
ttsp_mp_wait_check_point(ID id, uint_t count)
{
	ulong_t	timeout = 0;

	while (check_count_mp[id - 1] < count) {
		/*
		 * �����ॢ���Ƚ���
		 */
		timeout++;
		if (timeout > TTSP_LOOP_COUNT) {
			syslog_3(LOG_ERROR, "## PE %d : ttsp_mp_wait_check_point(%d, %d) caused a timeout.", id, id, count);
			cp_state = false;
			ext_ker();
		}
		sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
	}
}


/*
 *  id�ֹ�Υ����å��ݥ���Ȥ�ʤ�ơ������ͥ��λ�����롥
 */ 
void
ttsp_mp_check_finish(ID id, uint_t count)
{
	ID	prcid;

	/*
	 *  PRCID����
	 */
	sil_get_pid(&prcid);

	/*
	 * �����ǻ��ꤷ���ץ����å�ID�����Ѳ����Ƥ������
	 */
	if (prcid != id) {
		syslog_2(LOG_ERROR, "## PE %d : Processor has changed to %d", id, prcid);
		syslog_2(LOG_ERROR, "## in \"ttsp_mp_check_finish(%d, %d)\".", id, count);
	}
	else {
		ttsp_mp_check_point(id, count);
		syslog_1(LOG_NOTICE, "PE %d : All check points passed.", id);
	}

	ext_ker();
}


/*
 *  �оݥ������ξ��֤����ꤷ�����֤��Ѳ�����Τ��Ԥġ�
 */ 
void
ttsp_state_sync(char* proc_id, char* target_id, ID target_tskid, char* target_state_id, STAT target_state)
{
	ER ercd;
	ulong_t timeout = 0;
	T_TTSP_RTSK rtsk;

	do {
		ercd = ttsp_ref_tsk(target_tskid, &rtsk);
		check_ercd(ercd, E_OK);
		timeout++;
		if (timeout > TTSP_LOOP_COUNT) {
			syslog_1(LOG_ERROR, "## %s caused a timeout,", proc_id);
			syslog_2(LOG_ERROR, "## because %s didn't change to %s", target_id, target_state_id);
			syslog_0(LOG_ERROR, "## in \"ttsp_state_sync()\".");
			cp_state = false;
			ext_ker();
		}
		sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
	} while (rtsk.tskstat != target_state);
}


/*
 *  �¹Ծ��֤Τޤ�¾����������ter_tsk�ˤ�꽪λ�����Τ��Ԥġ�
 */ 
void
ttsp_wait_finish_sync(char* proc_id)
{
	ulong_t timeout = 0;

	while(1) {
		timeout++;
		if (timeout > TTSP_LOOP_COUNT) {
			syslog_1(LOG_ERROR, "## %s caused a timeout.", proc_id);
			syslog_0(LOG_ERROR, "## in \"ttsp_wait_finish_sync()\".");
			cp_state = false;
			ext_ker();
		}
		sil_dly_nse(TTSP_SIL_DLY_NSE_TIME);
	};
}



/*
 *  ref_tsk���شؿ�
 */
ER
ttsp_ref_tsk(ID tskid, T_TTSP_RTSK *pk_rtsk)
{
	TCB		*p_tcb;
	uint_t	tstat;
	ER		ercd;
	PCB		*p_pcb;
	uint_t	porder;
	QUEUE	*p_next;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_TSKID(tskid);
	p_tcb = _kernel_p_tcb_table[tskid - 1];
	p_pcb = ttsp_acquire_tsk_lock_without_preemption(p_tcb);

	tstat = p_tcb->tstat;
	if (TSTAT_DORMANT(tstat)) {
		/*
		 *  �оݥ��������ٻ߾��֤ξ��
		 */
		pk_rtsk->tskstat = TTS_DMT;
	}
	else {
		/*
		 *  ���������֤μ�Ф�
		 */
		if (TSTAT_SUSPENDED(tstat)) {
			if (TSTAT_WAITING(tstat)) {
				pk_rtsk->tskstat = TTS_WAS;
			}
			else if (p_tcb == p_pcb->p_runtsk) {
				pk_rtsk->tskstat = TTS_RUS;
			}
			
			else {
				pk_rtsk->tskstat = TTS_SUS;
			}
		}
		else if (TSTAT_WAITING(tstat)) {
			pk_rtsk->tskstat = TTS_WAI;
		}
		else if (p_tcb == p_pcb->p_runtsk) {
			pk_rtsk->tskstat = TTS_RUN;
		}
		else {
			pk_rtsk->tskstat = TTS_RDY;
		}

		/*
		 *  ����ͥ���٤ȥ١���ͥ���٤μ�Ф�
		 */
		pk_rtsk->tskpri = EXT_TSKPRI(p_tcb->priority);
		pk_rtsk->tskbpri = EXT_TSKPRI(p_tcb->priority);

		if (TSTAT_WAITING(tstat)) {
			/*
			 *  �Ԥ��װ����Ԥ��оݤΥ��֥������Ȥ�ID�μ�Ф�
			 */
			switch (tstat & TS_WAIT_MASK) {
			case TS_WAIT_SLP:
				pk_rtsk->tskwait = TTW_SLP;
				break;
			case TS_WAIT_DLY:
				pk_rtsk->tskwait = TTW_DLY;
				break;
			case TS_WAIT_SEM:
				pk_rtsk->tskwait = TTW_SEM;
				pk_rtsk->wobjid = SEMID(((SEMCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_FLG:
				pk_rtsk->tskwait = TTW_FLG;
				pk_rtsk->wobjid = FLGID(((FLGCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_SDTQ:
				pk_rtsk->tskwait = TTW_SDTQ;
				pk_rtsk->wobjid = DTQID(((DTQCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_RDTQ:
				pk_rtsk->tskwait = TTW_RDTQ;
				pk_rtsk->wobjid = DTQID(((DTQCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_SPDQ:
				pk_rtsk->tskwait = TTW_SPDQ;
				pk_rtsk->wobjid = PDQID(((PDQCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_RPDQ:
				pk_rtsk->tskwait = TTW_RPDQ;
				pk_rtsk->wobjid = PDQID(((PDQCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_MBX:
				pk_rtsk->tskwait = TTW_MBX;
				pk_rtsk->wobjid = MBXID(((MBXCB*)(p_tcb->p_wobjcb)));
				break;
			case TS_WAIT_MPF:
				pk_rtsk->tskwait = TTW_MPF;
				pk_rtsk->wobjid = MPFID(((MPFCB*)(p_tcb->p_wobjcb)));
				break;
			}

			/*
			 *  �����ॢ���Ȥ���ޤǤλ��֤μ�Ф�
			 */
			if ((p_tcb->tmevtb).callback != NULL) {
				pk_rtsk->lefttmo =
				  (TMO) tmevt_lefttim(p_pcb->p_tevtcb,
									  &(p_tcb->tmevtb));
			}
			else {
				pk_rtsk->lefttmo = TMO_FEVR;
			}
		}

		/*
		 *  �����׵ᥭ�塼���󥰿��μ�Ф�
		 */
		pk_rtsk->wupcnt = p_tcb->wupque ? 1U : 0U;
	}

	/*
	 *  ��ư�׵ᥭ�塼���󥰿��μ�Ф�
	 */
	pk_rtsk->actcnt = p_tcb->actque ? 1U : 0U;

	/*
	 *  ������°���μ�Ф�
	 */
	pk_rtsk->tskatr = p_tcb->p_tinib->tskatr;

	/*
	 *  �������γ�ĥ����μ�Ф�
	 */
	pk_rtsk->exinf = p_tcb->p_tinib->exinf;

	/*
	 *  �������ε�ư��ͥ���٤μ�Ф�
	 */
	pk_rtsk->itskpri = EXT_TSKPRI(p_tcb->p_tinib->ipriority);

	/*
	 *  �����å��ΰ�Υ������μ�Ф�
	 */
#ifdef USE_TSKINICTXB
	/*
	 *  USE_TSKINICTXB��������Ƥ��륿�����åȤǤ�
	 *  ɸ������ؤ��Ѵ��ޥ������ѿ����Ѱդ���
	 */
	pk_rtsk->stksz = ttsp_target_get_stksz(p_tcb->p_tinib);
#else /* USE_TSKINICTXB */
	pk_rtsk->stksz = p_tcb->p_tinib->stksz;
#endif /* USE_TSKINICTXB */

	/*
	 *  �����å��ΰ����Ƭ���Ϥμ�Ф�
	 */
#ifdef USE_TSKINICTXB
	pk_rtsk->stk = ttsp_target_get_stk(p_tcb->p_tinib);
#else /* USE_TSKINICTXB */
	pk_rtsk->stk = p_tcb->p_tinib->stk;
#endif /* USE_TSKINICTXB */

	/*
	 *  ���դ��ץ����å���Ʊ��ͥ���٥�������Ǥ�ͥ���̻���
	 *  ("MAIN_TASK(=1)"�ϥ�������оݳ��Ȥ���)
	 */

	if ((TTS_RUN == pk_rtsk->tskstat) || (TTS_RDY == pk_rtsk->tskstat)) {
		porder = 0;
		p_next = NULL;
		p_next = p_pcb->ready_queue[pk_rtsk->tskpri - 1].p_next;
		do {
			if (TSKID(((TCB *)p_next)) != 1) {
				porder++;
			}
			if (TSKID(((TCB *)p_next)) == tskid) {
				break;
			}
			p_next = p_next->p_next;
		} while (&p_pcb->ready_queue[pk_rtsk->tskpri - 1] != p_next);

		pk_rtsk->porder = porder;
	}

	/*
	 *  ���դ��ץ����å���ID�μ�Ф�
	 */
	pk_rtsk->prcid = p_pcb->prcid;

	/*
	 *  ���ε�ư���λ����դ��ץ����å�ID�μ�Ф�
	 */
	pk_rtsk->actprc = p_tcb->actprc;

	/*
	 *  �������ν�����դ��ץ����å��μ�Ф�
	 */
	pk_rtsk->iaffinity = p_tcb->p_tinib->iaffinity;

	/*
	 *  �������γ��դ���ǽ�ץ����å��μ�Ф�
	 */
	pk_rtsk->affinity_mask = p_tcb->p_tinib->affinity_mask;

	ercd = E_OK;
	release_tsk_lock(p_pcb);

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_tex���شؿ�
 */
ER 
ttsp_ref_tex(ID tskid, T_TTSP_RTEX *pk_rtex)
{
	TCB		*p_tcb;
	ER		ercd;
	PCB		*p_pcb;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_TSKID(tskid);
	p_tcb = _kernel_p_tcb_table[tskid - 1];
	p_pcb = ttsp_acquire_tsk_lock_without_preemption(p_tcb);

	if (TSTAT_DORMANT(p_tcb->tstat) || p_tcb->p_tinib->texrtn == NULL) {
		/*
		 *  ���������ٻ߾��֡����������Ф��ƥ������㳰����
		 *  �롼�����������Ƥ��ʤ��ξ��
		 */
		ercd = E_OBJ;
	}
	else {
		/*
		 *  �������㳰�����ξ��֡���α�㳰�װ���
		 *  �������㳰�����롼����°�������
		 */
		pk_rtex->texstat = (p_tcb->enatex) ? TTEX_ENA : TTEX_DIS;
		pk_rtex->pndptn = p_tcb->texptn;
		pk_rtex->texatr = p_tcb->p_tinib->texatr;
		ercd = E_OK;
	}

	release_tsk_lock(p_pcb);

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_sem���شؿ�
 */
ER
ttsp_ref_sem(ID semid, T_TTSP_RSEM *pk_rsem)
{
	SEMCB	*p_semcb;
	ER		ercd;
	bool_t	locked;
	uint_t	waitcnt;
	QUEUE	*p_next;

	locked = ttsp_loc_cpu();

	CHECK_SEMID(semid);
	p_semcb = _kernel_p_semcb_table[semid - 1];

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_semcb));

	/*
	 *  ���ޥե��λ񸻿������ޥե�°����
	 *  ���ޥե��ν���񸻿������ޥե��κ���񸻿������
	 */
	pk_rsem->semcnt = p_semcb->semcnt;
	pk_rsem->sematr = p_semcb->p_seminib->sematr;
	pk_rsem->isemcnt = p_semcb->p_seminib->isemcnt;
	pk_rsem->maxsem = p_semcb->p_seminib->maxsem;

	/*
	 *  �Ԥ��������ο�����
	 */
	waitcnt = 0;
	if (wait_tskid(&(p_semcb->wait_queue)) != TSK_NONE) {
		p_next = p_semcb->wait_queue.p_next;
		while (&(p_semcb->wait_queue) != p_next) {
			waitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rsem->waitcnt = waitcnt;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_semcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ���ޥե����Ԥ����������ȴؿ�
 */
ER
ttsp_ref_wait_sem(ID semid, uint_t order, ID *p_tskid)
{
	SEMCB		*p_semcb;
	ER			ercd;
	bool_t		locked;
	T_TTSP_RSEM	ref_rsem;
	QUEUE		*p_next;
	uint_t		i;

	locked = ttsp_loc_cpu();

	CHECK_SEMID(semid);
	p_semcb = _kernel_p_semcb_table[semid - 1];

	/*
	 *  �Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_sem(semid, &ref_rsem);
	check_ercd(ercd, E_OK);
	if ((ref_rsem.waitcnt == 0) || (ref_rsem.waitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_semcb));

	/*
	 *  order���Ԥ��ȤʤäƤ��륿����ID�����
	 */
	i = 1;
	p_next = &(p_semcb->wait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);
	
	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_semcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_flg���شؿ�
 */
ER
ttsp_ref_flg(ID flgid, T_TTSP_RFLG *pk_rflg)
{
	FLGCB	*p_flgcb;
	ER		ercd;
	bool_t	locked;
	uint_t	waitcnt;
	QUEUE	*p_next;

	locked = ttsp_loc_cpu();

	CHECK_FLGID(flgid);
	p_flgcb = _kernel_p_flgcb_table[flgid - 1];

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_flgcb));

	/*
	 *  ���٥�ȥե饰�θ��ߤΥӥåȥѥ����󡤥��٥�ȥե饰°����
	 *  ���٥�ȥե饰�Υӥåȥѥ�����ν���ͤ����
	 */
	pk_rflg->flgptn = p_flgcb->flgptn;
	pk_rflg->flgatr = p_flgcb->p_flginib->flgatr;
	pk_rflg->iflgptn = p_flgcb->p_flginib->iflgptn;

	/*
	 *  �Ԥ��������ο�
	 */
	waitcnt = 0;
	if (wait_tskid(&(p_flgcb->wait_queue)) != TSK_NONE) {
		p_next = p_flgcb->wait_queue.p_next;
		while (&(p_flgcb->wait_queue) != p_next) {
			waitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rflg->waitcnt = waitcnt;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_flgcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ���٥�ȥե饰���Ԥ����������ȴؿ�
 */
ER
ttsp_ref_wait_flg(ID flgid, uint_t order, ID *p_tskid, FLGPTN *p_waiptn, MODE *p_wfmode)
{
	FLGCB		*p_flgcb;
	ER			ercd;
	bool_t		locked;
	T_TTSP_RFLG	ref_rflg;
	QUEUE		*p_next;
	uint_t		i;

	locked = ttsp_loc_cpu();

	CHECK_FLGID(flgid);
	p_flgcb = _kernel_p_flgcb_table[flgid - 1];

	/*
	 *  �Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_flg(flgid, &ref_rflg);
	check_ercd(ercd, E_OK);
	if ((ref_rflg.waitcnt == 0) || (ref_rflg.waitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_flgcb));

	/*
	 *  order���Ԥ��ȤʤäƤ��륿����ID���Ԥ��ӥåȥѥ����󡤥ǡ�����������
	 */
	i = 1;
	p_next = &(p_flgcb->wait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);
	*p_waiptn = ((WINFO_FLG *)(&(((TCB *)(p_next->p_next))->winfo_obj)))->waiptn;
	*p_wfmode = ((WINFO_FLG *)(&(((TCB *)(p_next->p_next))->winfo_obj)))->wfmode;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_flgcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_dtq���شؿ�
 */
ER
ttsp_ref_dtq(ID dtqid, T_TTSP_RDTQ *pk_rdtq)
{
	DTQCB	*p_dtqcb;
	ER		ercd;
	uint_t	swaitcnt;
	uint_t	rwaitcnt;
	QUEUE	*p_next;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_DTQID(dtqid);
	p_dtqcb = _kernel_p_dtqcb_table[dtqid - 1];

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_dtqcb));

	/*
	 *  �ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ����ο���
	 *  �ǡ������塼°�����ǡ������塼�����̤����
	 */
	pk_rdtq->sdtqcnt = p_dtqcb->count;
	pk_rdtq->dtqatr = p_dtqcb->p_dtqinib->dtqatr;
	pk_rdtq->dtqcnt = p_dtqcb->p_dtqinib->dtqcnt;

	/*
	 *  �����Ԥ��������ο�����
	 */
	swaitcnt = 0;
	if (wait_tskid(&(p_dtqcb->swait_queue)) != TSK_NONE) {
		p_next = p_dtqcb->swait_queue.p_next;
		while (&(p_dtqcb->swait_queue) != p_next) {
			swaitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rdtq->swaitcnt = swaitcnt;

	/*
	 *  �����Ԥ��������ο�����
	 */
	rwaitcnt = 0;
	if (wait_tskid(&(p_dtqcb->rwait_queue)) != TSK_NONE) {
		p_next = p_dtqcb->rwait_queue.p_next;
		while (&(p_dtqcb->rwait_queue) != p_next) {
			rwaitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rdtq->rwaitcnt = rwaitcnt;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_dtqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  �ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ������ȴؿ�
 */
ER
ttsp_ref_data(ID dtqid, uint_t index, intptr_t *p_data)
{
	DTQCB	*p_dtqcb;
	ER		ercd;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_DTQID(dtqid);
	p_dtqcb = _kernel_p_dtqcb_table[dtqid - 1];

	/*
	 *  �ǡ������塼�����ΰ�ˤĤʤ���Ƥ���ǡ����ο�������å�
	 */
	if ((p_dtqcb->count == 0) || (p_dtqcb->count < index)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_dtqcb));

	/*
	 *  index�˳�Ǽ����Ƥ���ǡ�����������
	 *  (�ǡ������塼��ν���������ؤ�äƤ��뤳�Ȥ��θ����)
	 */
	*p_data = (p_dtqcb->p_dtqinib->p_dtqmb + ((p_dtqcb->head +index - 1) % p_dtqcb->p_dtqinib->dtqcnt))->data;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_dtqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  �ǡ������塼�������Ԥ����������ȴؿ�
 */
ER
ttsp_ref_swait_dtq(ID dtqid, uint_t order, ID *p_tskid, intptr_t *p_data)
{
	DTQCB		*p_dtqcb;
	QUEUE		*p_next;
	ER			ercd;
	uint_t		i;
	bool_t		locked;
	T_TTSP_RDTQ	ref_rdtq;

	locked = ttsp_loc_cpu();

	CHECK_DTQID(dtqid);
	p_dtqcb = _kernel_p_dtqcb_table[dtqid - 1];

	/*
	 *  �����Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_dtq(dtqid, &ref_rdtq);
	check_ercd(ercd, E_OK);
	if ((ref_rdtq.swaitcnt == 0) || (ref_rdtq.swaitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_dtqcb));

	/*
	 *  order�������Ԥ��ȤʤäƤ��륿����ID���ǡ�����������
	 */
	i = 1;
	p_next = &(p_dtqcb->swait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);
	*p_data = ((WINFO_DTQ *)(&(((TCB *)(p_next->p_next))->winfo_obj)))->data;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_dtqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  �ǡ������塼�μ����Ԥ����������ȴؿ�
 */
ER
ttsp_ref_rwait_dtq(ID dtqid, uint_t order, ID *p_tskid)
{
	DTQCB		*p_dtqcb;
	QUEUE		*p_next;
	ER			ercd;
	uint_t		i;
	bool_t		locked;
	T_TTSP_RDTQ	ref_rdtq;

	locked = ttsp_loc_cpu();

	CHECK_DTQID(dtqid);
	p_dtqcb = _kernel_p_dtqcb_table[dtqid - 1];

	/*
	 *  �����Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_dtq(dtqid, &ref_rdtq);
	check_ercd(ercd, E_OK);
	if ((ref_rdtq.rwaitcnt == 0) || (ref_rdtq.rwaitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_dtqcb));

	/*
	 *  order�Ǽ����Ԥ��ȤʤäƤ��륿����ID�����
	 */
	i = 1;
	p_next = &(p_dtqcb->rwait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_dtqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_pdq���شؿ�
 */
ER
ttsp_ref_pdq(ID pdqid, T_TTSP_RPDQ *pk_rpdq)
{
	PDQCB	*p_pdqcb;
	ER		ercd;
	uint_t	swaitcnt;
	uint_t	rwaitcnt;
	QUEUE	*p_next;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_PDQID(pdqid);
	p_pdqcb = _kernel_p_pdqcb_table[pdqid - 1];

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_pdqcb));

	/*
	 *  ͥ���٥ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ����ο���
	 *  ͥ���٥ǡ������塼°����ͥ���٥ǡ������塼�����̡�
	 *  �ǡ���ͥ���٤κ����ͤ����
	 */
	pk_rpdq->spdqcnt = p_pdqcb->count;
	pk_rpdq->pdqatr = p_pdqcb->p_pdqinib->pdqatr;
	pk_rpdq->pdqcnt = p_pdqcb->p_pdqinib->pdqcnt;
	pk_rpdq->maxdpri = p_pdqcb->p_pdqinib->maxdpri;

	/*
	 *  �����Ԥ��������ο�����
	 */
	swaitcnt = 0;
	if (wait_tskid(&(p_pdqcb->swait_queue)) != TSK_NONE) {
		p_next = p_pdqcb->swait_queue.p_next;
		while (&(p_pdqcb->swait_queue) != p_next) {
			swaitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rpdq->swaitcnt = swaitcnt;

	/*
	 *  �����Ԥ��������ο�����
	 */
	rwaitcnt = 0;
	if (wait_tskid(&(p_pdqcb->rwait_queue)) != TSK_NONE) {
		p_next = p_pdqcb->rwait_queue.p_next;
		while (&(p_pdqcb->rwait_queue) != p_next) {
			rwaitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rpdq->rwaitcnt = rwaitcnt;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_pdqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ͥ���٥ǡ������塼�����ΰ�˳�Ǽ����Ƥ���ǡ������ȴؿ�
 */
ER
ttsp_ref_pridata(ID pdqid, uint_t index, intptr_t *p_data, PRI *p_datapri)
{
	PDQCB	*p_pdqcb;
	PDQMB	*p_pdqmb;
	ER		ercd;
	uint_t	i;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_PDQID(pdqid);
	p_pdqcb = _kernel_p_pdqcb_table[pdqid - 1];

	/*
	 *  ��Ǽ����Ƥ���ǡ����ο�������å� 
	 */
	if ((p_pdqcb->count == 0) || (p_pdqcb->count < index)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_pdqcb));

	/*
	 *  index�˳�Ǽ����Ƥ���ǡ�����������
	 */
	i = 1;
	p_pdqmb = p_pdqcb->p_head;
	while (i < index) {
		p_pdqmb = p_pdqmb->p_next;
		i++;
	}
	*p_data = p_pdqmb->data;
	*p_datapri = p_pdqmb->datapri;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_pdqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ͥ���٥ǡ������塼�������Ԥ����������ȴؿ�
 */
ER
ttsp_ref_swait_pdq(ID pdqid, uint_t order, ID *p_tskid, intptr_t *p_data, PRI *p_datapri)
{
	PDQCB		*p_pdqcb;
	QUEUE		*p_next;
	ER			ercd;
	uint_t		i;
	bool_t		locked;
	T_TTSP_RPDQ	ref_rpdq;

	locked = ttsp_loc_cpu();

	CHECK_PDQID(pdqid);
	p_pdqcb = _kernel_p_pdqcb_table[pdqid - 1];

	/*
	 *  �����Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_pdq(pdqid, &ref_rpdq);
	check_ercd(ercd, E_OK);
	if ((ref_rpdq.swaitcnt == 0) || (ref_rpdq.swaitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_pdqcb));

	/*
	 *  order�������Ԥ��ȤʤäƤ��륿����ID���ǡ�����������
	 */
	i = 1;
	p_next = &(p_pdqcb->swait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);
	*p_data = ((WINFO_PDQ *)(&(((TCB *)(p_next->p_next))->winfo_obj)))->data;
	*p_datapri = ((WINFO_PDQ *)(&(((TCB *)(p_next->p_next))->winfo_obj)))->datapri;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_pdqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ͥ���٥ǡ������塼�μ����Ԥ����������ȴؿ�
 */
ER
ttsp_ref_rwait_pdq(ID pdqid, uint_t order, ID *p_tskid)
{
	PDQCB		*p_pdqcb;
	QUEUE		*p_next;
	ER			ercd;
	uint_t		i;
	bool_t		locked;
	T_TTSP_RPDQ	ref_rpdq;

	locked = ttsp_loc_cpu();

	CHECK_PDQID(pdqid);
	p_pdqcb = _kernel_p_pdqcb_table[pdqid - 1];

	/*
	 *  �����Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_pdq(pdqid, &ref_rpdq);
	check_ercd(ercd, E_OK);
	if ((ref_rpdq.rwaitcnt == 0) || (ref_rpdq.rwaitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_pdqcb));

	/*
	 *  order�Ǽ����Ԥ��ȤʤäƤ��륿����ID�����
	 */
	i = 1;
	p_next = &(p_pdqcb->rwait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_pdqcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_spn���شؿ�
 */
ER
ttsp_ref_spn(ID spnid, T_TTSP_RSPN *pk_rspn)
{
	ER		ercd;
	SPNCB	*p_spncb;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_SPNID(spnid);
	p_spncb = _kernel_p_spncb_table[spnid - 1];

	/*
	 *  ���ԥ���å��Υ��å����֤�°�������
	 */
	pk_rspn->spnstat = (p_spncb->lock_flg) ? TSPN_LOC : TSPN_UNL;
	pk_rspn->spnatr = p_spncb->p_spninib->spnatr;
	
	ercd = E_OK;

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_mbx���شؿ�
 */
ER
ttsp_ref_mbx(ID mbxid, T_TTSP_RMBX *pk_rmbx)
{
	MBXCB	*p_mbxcb;
	ER		ercd;
	bool_t	locked;
	uint_t	msgcnt;
	uint_t	waitcnt;
	QUEUE	*p_next;
	T_MSG	*pk_msg;

	locked = ttsp_loc_cpu();

	CHECK_MBXID(mbxid);
	p_mbxcb = _kernel_p_mbxcb_table[mbxid - 1];

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_mbxcb));

	/*
	 *  �᡼��ܥå����ˤĤʤ���Ƥ����å������ο�
	 */
	msgcnt = 0;
	pk_msg = p_mbxcb->pk_head;
	while (pk_msg != NULL) {
		msgcnt++;
		pk_msg = pk_msg->pk_next;
	}
	pk_rmbx->msgcnt = msgcnt;

	/*
	 *  �᡼��ܥå���°������å�����ͥ���٤κ����ͤ����
	 */
	pk_rmbx->mbxatr = p_mbxcb->p_mbxinib->mbxatr;
	pk_rmbx->maxmpri = p_mbxcb->p_mbxinib->maxmpri;

	/*
	 *  �����Ԥ��������ο�
	 */
	waitcnt = 0;
	if (wait_tskid(&(p_mbxcb->wait_queue)) != TSK_NONE) {
		p_next = p_mbxcb->wait_queue.p_next;
		while (&(p_mbxcb->wait_queue) != p_next) {
			waitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rmbx->rwaitcnt = waitcnt;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_mbxcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  �᡼��ܥå����ˤĤʤ���Ƥ���ǡ������ȴؿ�
 */
ER
ttsp_ref_msg(ID mbxid, uint_t index, T_MSG **pp_msg)
{
	MBXCB		*p_mbxcb;
	ER			ercd;
	bool_t		locked;
	T_TTSP_RMBX	ref_rmbx;
	T_MSG		*pk_next;
	uint_t		i;

	locked = ttsp_loc_cpu();

	CHECK_MBXID(mbxid);
	p_mbxcb = _kernel_p_mbxcb_table[mbxid - 1];

	/*
	 *  �᡼��ܥå����ˤĤʤ���Ƥ����å������ο�������å�
	 */
	ercd = ttsp_ref_mbx(mbxid, &ref_rmbx);
	check_ercd(ercd, E_OK);
	if ((ref_rmbx.msgcnt == 0) || (ref_rmbx.msgcnt < index)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_mbxcb));

	/*
	 *  index�˳�Ǽ����Ƥ����å������إå��ݥ��󥿤����
	 */
	i = 1;
	pk_next = p_mbxcb->pk_head;
	while (i < index) {
		pk_next = pk_next->pk_next;
		i++;
	}
	*pp_msg = pk_next;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_mbxcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  �᡼��ܥå����μ����Ԥ����������ȴؿ�
 */
ER
ttsp_ref_rwait_mbx(ID mbxid, uint_t order, ID *p_tskid)
{
	MBXCB		*p_mbxcb;
	ER			ercd;
	bool_t		locked;
	T_TTSP_RMBX	ref_rmbx;
	QUEUE		*p_next;
	uint_t		i;

	locked = ttsp_loc_cpu();

	CHECK_MBXID(mbxid);
	p_mbxcb = _kernel_p_mbxcb_table[mbxid - 1];

	/*
	 *  �����Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_mbx(mbxid, &ref_rmbx);
	check_ercd(ercd, E_OK);
	if ((ref_rmbx.rwaitcnt == 0) || (ref_rmbx.rwaitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_mbxcb));

	/*
	 *  order�Ǽ����Ԥ��ȤʤäƤ��륿����ID�����
	 */
	i = 1;
	p_next = &(p_mbxcb->wait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_mbxcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_mpf���شؿ�
 */
ER
ttsp_ref_mpf(ID mpfid, T_TTSP_RMPF *pk_rmpf)
{
	MPFCB	*p_mpfcb;
	ER		ercd;
	bool_t	locked;
	uint_t	waitcnt;
	QUEUE	*p_next;

	locked = ttsp_loc_cpu();

	CHECK_MPFID(mpfid);
	p_mpfcb = _kernel_p_mpfcb_table[mpfid - 1];

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_mpfcb));

	/*
	 *  ����Ĺ����ס����ΰ�ζ��������ΰ�˳���դ��뤳�Ȥ��Ǥ���
	 *  ����Ĺ����֥��å��ο�������Ĺ����ס���°��������֥��å�����
	 *  ����֥��å��Υ����������
	 */
	pk_rmpf->fblkcnt = p_mpfcb->fblkcnt;
	pk_rmpf->mpfatr = p_mpfcb->p_mpfinib->mpfatr;
	pk_rmpf->blkcnt = p_mpfcb->p_mpfinib->blkcnt;
	pk_rmpf->blksz = p_mpfcb->p_mpfinib->blksz;
	pk_rmpf->mpf = p_mpfcb->p_mpfinib->mpf;
	
	/*
	 *  �Ԥ��������ο�
	 */
	waitcnt = 0;
	if (wait_tskid(&(p_mpfcb->wait_queue)) != TSK_NONE) {
		p_next = p_mpfcb->wait_queue.p_next;
		while (&(p_mpfcb->wait_queue) != p_next) {
			waitcnt++;
			p_next = p_next->p_next;
		}
	}
	pk_rmpf->waitcnt =waitcnt;

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_mpfcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ����Ĺ����ס�����Ԥ����������ȴؿ�
 */
ER
ttsp_ref_wait_mpf(ID mpfid, uint_t order, ID *p_tskid)
{
	MPFCB		*p_mpfcb;
	ER			ercd;
	bool_t		locked;
	T_TTSP_RMPF	ref_rmpf;
	QUEUE		*p_next;
	uint_t		i;

	locked = ttsp_loc_cpu();

	CHECK_MPFID(mpfid);
	p_mpfcb = _kernel_p_mpfcb_table[mpfid - 1];

	/*
	 *  �Ԥ��������ο�������å�
	 */
	ercd = ttsp_ref_mpf(mpfid, &ref_rmpf);
	check_ercd(ercd, E_OK);
	if ((ref_rmpf.waitcnt == 0) || (ref_rmpf.waitcnt < order)) {
		ercd = E_PAR;
		goto error_exit;
	}

	ttsp_acquire_obj_lock_without_preemption(&GET_OBJLOCK(p_mpfcb));

	/*
	 *  order���Ԥ��ȤʤäƤ��륿����ID�����
	 */
	i = 1;
	p_next = &(p_mpfcb->wait_queue);
	while (i < order) {
		p_next = p_next->p_next;
		i++;
	}
	*p_tskid = wait_tskid(p_next);

	ercd = E_OK;
	release_obj_lock(&GET_OBJLOCK(p_mpfcb));

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_cyc���شؿ�
 */
ER
ttsp_ref_cyc(ID cycid, T_TTSP_RCYC *pk_rcyc)
{
	CYCCB	*p_cyccb;
	ER		ercd;
	PCB		*p_pcb;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_CYCID(cycid);
	p_cyccb = _kernel_p_cyccb_table[cycid - 1];

	p_pcb = ttsp_acquire_tsk_lock_without_preemption_cyc(p_cyccb);

	/*
	 *  �����ϥ�ɥ��ư����֡�
	 *  ���˼����ϥ�ɥ��ư�������ޤǤ����л���
	 */
	if (p_cyccb->cycsta) {
		pk_rcyc->cycstat = TCYC_STA;
		pk_rcyc->lefttim = tmevt_lefttim(p_cyccb->p_pcb->p_tevtcb, &(p_cyccb->tmevtb));
	}
	else {
		pk_rcyc->cycstat = TCYC_STP;
	}

	/*
	 *  �����ϥ�ɥ�°���������ϥ�ɥ�γ�ĥ���󡤼����ϥ�ɥ�ε�ư������
	 *  �����ϥ�ɥ�ε�ư���ꡤ���դ��ץ����å���ID��������դ��ץ����å���
	 *  ���դ���ǽ�ץ����å������
	 */
	pk_rcyc->cycatr = p_cyccb->p_cycinib->cycatr;
	pk_rcyc->exinf = p_cyccb->p_cycinib->exinf;
	pk_rcyc->cyctim = p_cyccb->p_cycinib->cyctim;
	pk_rcyc->cycphs = p_cyccb->p_cycinib->cycphs;
	pk_rcyc->prcid = p_cyccb->p_pcb->prcid;
#ifdef TOPPERS_SYSTIM_LOCAL
	pk_rcyc->iaffinity = p_cyccb->p_cycinib->iaffinity;
	pk_rcyc->affinity_mask = p_cyccb->p_cycinib->affinity_mask;
#endif /* TOPPERS_SYSTIM_LOCAL */

	ercd = E_OK;
	release_tsk_lock(p_pcb);

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  ref_alm���شؿ�
 */
ER
ttsp_ref_alm(ID almid, T_TTSP_RALM *pk_ralm)
{
	ALMCB	*p_almcb;
	ER		ercd;
	PCB		*p_pcb;
	bool_t	locked;

	locked = ttsp_loc_cpu();

	CHECK_ALMID(almid);
	p_almcb = _kernel_p_almcb_table[almid - 1];
	p_pcb = ttsp_acquire_tsk_lock_without_preemption_alm(p_almcb);

	/*
	 *  ���顼��ϥ�ɥ��ư����֡�
	 *  ���顼��ϥ�ɥ��ư�������ޤǤ����л��֤����
	 */
	if (p_almcb->almsta) {
		pk_ralm->almstat = TALM_STA;
		pk_ralm->lefttim = tmevt_lefttim(p_almcb->p_pcb->p_tevtcb, &(p_almcb->tmevtb));
	}
	else {
		pk_ralm->almstat = TALM_STP;
	}

	/*
	 *  ���顼��ϥ�ɥ�°�������顼��ϥ�ɥ顤���դ��ץ����å���ID��
	 *  ������դ��ץ����å������դ���ǽ�ץ����å��γ�ĥ��������
	 */
	pk_ralm->almatr = p_almcb->p_alminib->almatr;
	pk_ralm->exinf = p_almcb->p_alminib->exinf;
	pk_ralm->prcid = p_almcb->p_pcb->prcid;
#ifdef TOPPERS_SYSTIM_LOCAL
	pk_ralm->iaffinity = p_almcb->p_alminib->iaffinity;
	pk_ralm->affinity_mask = p_almcb->p_alminib->affinity_mask;
#endif /* TOPPERS_SYSTIM_LOCAL */
 
	ercd = E_OK;
	release_tsk_lock(p_pcb);

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}


/*
 *  get_ipm���شؿ�
 */
ER
ttsp_get_ipm(PRI *p_intpri)
{
	bool_t	locked;
	ER		ercd;

	locked = ttsp_loc_cpu();

	/*
	 *  �����ͥ���٥ޥ��������
	 */
	if (!sense_context()) {
		*p_intpri = t_get_ipm();
	}
	else {
		*p_intpri = i_get_ipm();
	}

	ercd = E_OK;

	ttsp_unl_cpu(locked);

	return(ercd);
}


/*
 *  sus_tsk���شؿ�
 */
ER
ttsp_sus_tsk(ID tskid)
{
	TCB		*p_tcb;
	ER		ercd;
	PCB		*p_pcb;
	bool_t	locked;
	
	locked = ttsp_loc_cpu();

	CHECK_TSKID(tskid);

	p_tcb = get_tcb(tskid);
	p_pcb = ttsp_acquire_tsk_lock_without_preemption(p_tcb);

	if (TSTAT_DORMANT(p_tcb->tstat)) {
		ercd = E_OBJ;
	}
	else if (TSTAT_RUNNABLE(p_tcb->tstat)) {
		/*
		 *  �¹ԤǤ�����֤��鶯���Ԥ����֤ؤ�����
		 */
		p_tcb->tstat = TS_SUSPENDED;
		make_non_runnable(p_tcb);
		target_ipi_raise(p_pcb->prcid);
		
		ercd = E_OK;
	}
	else if (TSTAT_SUSPENDED(p_tcb->tstat)) {
		ercd = E_QOVR;
	}
	else {
		/*
		 *  �Ԥ����֤�������Ԥ����֤ؤ�����
		 */
		p_tcb->tstat |= TS_SUSPENDED;
		ercd = E_OK;
	}

	release_tsk_lock(p_pcb);

  error_exit:
	ttsp_unl_cpu(locked);
	return(ercd);
}