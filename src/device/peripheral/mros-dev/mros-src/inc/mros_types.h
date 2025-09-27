#ifndef _MROS_TYPES_H_
#define _MROS_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef int mros_int32;
typedef char mros_int8;
typedef unsigned char mros_uint8;
typedef unsigned short mros_uint16;
typedef unsigned int mros_uint32;
typedef unsigned char mros_boolean;
#define MROS_FALSE        0
#define MROS_TRUE         1

#define MROS_ID_NONE      0
#define MROS_ID(index)    ((index) + 1U)
#define MROS_INDEX(id)    ((id) - 1U)

typedef mros_uint32  mRosNodeIdType;
typedef mros_uint32  mRosTopicIdType;
typedef mros_uint32  mRosTopicConnectorIdType;
typedef mros_uint32  mRosPacketIdType;
typedef mros_uint32  mRosIdType;
typedef mros_uint32* mRosFuncIdType;
typedef mros_int8*   mRosPtrType;

typedef mros_uint32* mRosContainerObjType;
#define MROS_NULL        (0)
#define MROS_COBJ_NULL   ((mRosContainerObjType)MROS_NULL)

typedef mros_uint32  mRosSizeType;

#define MROS_E_OK        0
#define MROS_E_NOENT     2
#define MROS_E_NOMEM     12
#define MROS_E_BUSY      13
#define MROS_E_EXIST     17
#define MROS_E_INVAL     22
#define MROS_E_RANGE     34
#define MROS_E_LIMIT     111
#define MROS_E_NOTCONN   112
#define MROS_E_SYSERR    -1
typedef mros_int32    mRosReturnType;

/*
 * OS dependent data types
 */
#include "mros_os_target.h"
#define MROS_TASKID_NONE  0U

/*
 * セクション属性マクロ
 *
 * - MROS_MATTR_BSS_NOCLR:
 *   ヘッダの extern 宣言で使われる可能性があるため、
 *   mac(__APPLE__)では空にしてコンパイルエラーを防ぐ。
 *
 * - MROS_SECTION_DEF:
 *   実体定義(.c)側でのみ付ける用。必要に応じて独自セクションへ配置。
 */
#if defined(__APPLE__)
  /* macOS (Mach-O): extern で使われる可能性があるので空にする */
  #define MROS_MATTR_BSS_NOCLR
  /* 実体定義でセクションを分けたいときはこちらを使用 */
  #define MROS_SECTION_DEF __attribute__((section("__DATA,__nc_bss"))) __attribute__((used))
#elif defined(__linux__)
  /* Linux (ELF) */
  #define MROS_MATTR_BSS_NOCLR __attribute__((section("NC_BSS")))
  #define MROS_SECTION_DEF      __attribute__((section("NC_BSS")))
#elif defined(_WIN32) && defined(_MSC_VER)
  /* Windows (MSVC) */
  #define MROS_MATTR_BSS_NOCLR  /* ヘッダでは空推奨 */
  #define MROS_SECTION_DEF      __declspec(allocate(".nc_bss"))
#else
  /* その他環境 */
  #define MROS_MATTR_BSS_NOCLR
  #define MROS_SECTION_DEF
#endif

/*
 * Comm dependent data types
 */
#include "mros_comm_target.h"

#ifdef __cplusplus
}
#endif

#endif /* _MROS_TYPES_H_ */
