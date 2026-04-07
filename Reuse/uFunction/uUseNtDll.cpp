//---------------------------------------------------------------------------

#pragma hdrstop

#include <windows.h>
#include "uUseNtDll.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

#ifndef NT_SUCCESS
  #define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef NT_INFORMATION
  #define NT_INFORMATION(Status) ((((ULONG)(Status)) >> 30) == 1)
#endif

#ifndef NT_WARNING
  #define NT_WARNING(Status) ((((ULONG)(Status)) >> 30) == 2)
#endif

#ifndef NT_ERROR
  #define NT_ERROR(Status) ((((ULONG)(Status)) >> 30) == 3)
#endif

// NTSTATUS 是一個 LONG (32-bit signed integer)
#ifndef STATUS_SUCCESS
  #define STATUS_SUCCESS           ((LONG)0x00000000L)
#endif

#ifndef STATUS_UNSUCCESSFUL
  #define STATUS_UNSUCCESSFUL       ((LONG)0xC0000001L)
#endif

// 如果之後需要判斷權限不足（例如 SetAffinity 失敗）
#ifndef STATUS_ACCESS_DENIED
  #define STATUS_ACCESS_DENIED      ((LONG)0xC0000022L)
#endif

bool fnNtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
bool fnNtQueryTimerResolution(PULONG MaximumResolution, PULONG MinimumResolution, PULONG CurrentResolution);
//---------------------------------------------------------------------------
typedef LONG NTSTATUS;  // NTSTATUS 通常是 LONG

// 函式指標型態定義
typedef NTSTATUS (NTAPI *pNtSetTimerResolution)
(
  ULONG DesiredResolution,      // 100ns 單位，例如 5000 = 0.5 ms
  BOOLEAN SetResolution,        // TRUE = 設定, FALSE = 取消
  PULONG CurrentResolution
);

typedef NTSTATUS (NTAPI *pNtQueryTimerResolution)
(
  PULONG MaximumResolution, // 1. 最大間隔 (通常是 15.6ms / 156250 units)
  PULONG MinimumResolution, // 2. 最小間隔 (通常是 0.5ms / 5000 units)
  PULONG CurrentResolution  // 3. 當前間隔
);

bool g_NtTimerFunFunctionsReady = false;
pNtSetTimerResolution  g_pNtSetTimerResolution = NULL;
pNtQueryTimerResolution  g_pNtQueryTimerResolution = NULL;
//---------------------------------------------------------------------------
bool fnInitNtTimerFunctions()
{
  HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
  if (!hNtDll) {
    hNtDll = LoadLibraryW(L"ntdll.dll");
    if (!hNtDll) {
      return false;
    }
  }

  g_pNtSetTimerResolution = (pNtSetTimerResolution)GetProcAddress(hNtDll, "NtSetTimerResolution");
  g_pNtQueryTimerResolution = (pNtQueryTimerResolution)GetProcAddress(hNtDll, "NtQueryTimerResolution");

  g_NtTimerFunFunctionsReady = (g_pNtSetTimerResolution != NULL &&
                                g_pNtQueryTimerResolution != NULL);

  return g_NtTimerFunFunctionsReady;
}
//---------------------------------------------------------------------------
bool fnNtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution)
{
  bool IinitFnuction = true;
  if (g_NtTimerFunFunctionsReady == false) {
    IinitFnuction = fnInitNtTimerFunctions();
  }

  LONG lStatus = STATUS_UNSUCCESSFUL;
  if (IinitFnuction == true) {
    lStatus = g_pNtSetTimerResolution(DesiredResolution, SetResolution, CurrentResolution);
  }

  if (NT_SUCCESS(lStatus)) {
    return true;
  }
  else {
    return false;
  }
}
//---------------------------------------------------------------------------
bool fnNtQueryTimerResolution(PULONG MaximumResolution, PULONG MinimumResolution, PULONG CurrentResolution)
{
  bool IinitFnuction = true;
  if (g_NtTimerFunFunctionsReady == false) {
    IinitFnuction = fnInitNtTimerFunctions();
  }

  LONG lStatus = STATUS_UNSUCCESSFUL;
  if (IinitFnuction == true) {
    lStatus = g_pNtQueryTimerResolution(MaximumResolution, MinimumResolution, CurrentResolution);
  }

  if (NT_SUCCESS(lStatus)) {
    return true;
  }
  else {
    return false;
  }
}
//---------------------------------------------------------------------------