#ifndef _MROS_TYPES_H_
#define _MROS_TYPES_H_

/* 共通型定義など… */

/* プラットフォーム別セクション属性 */
#if defined(__APPLE__)
  /* macOS: Mach-O は __SEGMENT,__section の形式 */
  #define MROS_SECTION_DEF __attribute__((section("__DATA,__nc_bss"))) __attribute__((used))
#elif defined(__linux__)
  /* Linux: ELF はそのまま */
  #define MROS_SECTION_DEF __attribute__((section("NC_BSS")))
#elif defined(_WIN32) && defined(_MSC_VER)
  /* Windows (MSVC) */
  #define MROS_SECTION_DEF __declspec(allocate(".nc_bss"))
#else
  /* それ以外は属性なし */
  #define MROS_SECTION_DEF
#endif

#endif /* _MROS_TYPES_H_ */
