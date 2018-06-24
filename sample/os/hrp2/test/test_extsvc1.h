/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2012 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
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
 *  $Id: test_extsvc1.h 837 2012-12-26 15:09:59Z ertl-hiro $
 */

/* 
 *		��ĥ�����ӥ�������˴ؤ���ƥ���(1)
 */

/*
 *  �������åȰ�¸�����
 */
#include "target_test.h"

/*
 *  �ƥ�������ͥ���٤����
 */
#define TASK1_PRIORITY	10		/* ������1��ͥ���� */
#define TASK2_PRIORITY	10		/* ������2��ͥ���� */
#define TASK3_PRIORITY	9		/* ������3��ͥ���� */

/*
 *  �������åȤ˰�¸�����ǽ���Τ�����������
 */
#ifndef STACK_SIZE
#define	STACK_SIZE		4096		/* �������Υ����å������� */
#endif /* STACK_SIZE */

#ifndef OVERFLOW_STACK_SIZE
#define OVERFLOW_STACK_SIZE			STACK_SIZE
#endif /* OVERFLOW_STACK_SIZE */

#ifndef NON_OVERFLOW_STACK_SIZE
#define NON_OVERFLOW_STACK_SIZE		(STACK_SIZE * 20)
#endif /* NON_OVERFLOW_STACK_SIZE */

/*
 *  ��ĥSVC�ֹ�����
 */
#define TFN_EXTSVC0		(50)
#define TFN_EXTSVC1		(51)
#define TFN_EXTSVC2		(52)
#define TFN_EXTSVC3		(53)

/*
 *  �ؿ��Υץ��ȥ��������
 */
#ifndef TOPPERS_MACRO_ONLY

extern void	task1(intptr_t exinf);

extern void	task2(intptr_t exinf);

extern void	task3(intptr_t exinf);

extern void	alarm1_handler(intptr_t exinf);

extern ER_UINT extsvc1_routine(intptr_t par1, intptr_t par2, intptr_t par3,
								intptr_t par4, intptr_t par5, ID cdmid);
extern ER_UINT extsvc2_routine(intptr_t par1, intptr_t par2, intptr_t par3,
								intptr_t par4, intptr_t par5, ID cdmid);
extern ER_UINT extsvc3_routine(intptr_t par1, intptr_t par2, intptr_t par3,
								intptr_t par4, intptr_t par5, ID cdmid);

#endif /* TOPPERS_MACRO_ONLY */