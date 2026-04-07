//---------------------------------------------------------------------------

#pragma hdrstop

#include "uStopWatch.h"

// 高精度忙等專用的 _mm_pause 定義
#include <mmsystem.h>
#if defined(__BORLANDC__) && !defined(__clang__)
  // 針對經典 BCC32 編譯器
  #define _mm_pause() __asm { pause }
#else
  // 針對 BCC64 或 BCC32C (CLANG 型編譯器)
  #include <immintrin.h>
#endif
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
VOID CALLBACK fnWaitOrTimerCB(PVOID lpParam, BOOLEAN TimerOrWaitFired);
//---------------------------------------------------------------------------
double fnMin(double dVar1, double dVar2)
{
  if (dVar1 < dVar2) {
    return dVar1;
  }
  return dVar2;
}
//---------------------------------------------------------------------------
double fnMax(double dVar1, double dVar2)
{
  if (dVar1 > dVar2) {
    return dVar1;
  }
  return dVar2;
}
//---------------------------------------------------------------------------
TStopwatchElp::TStopwatchElp()
{
  // crete TStopwatch record and start running
  m_sw = TStopwatch::StartNew();
  m_StartTick = m_sw.Elapsed.Ticks;
  m_StartTime = m_sw.Elapsed.TotalMilliseconds;
  // cumulation time; unit: ms
  m_Cumulation = this->Cumulation();

  m_CheckCount = 0;
  m_WaitTime = 0;
  m_WaitTick = 0;
  m_RateSnapIdx = 0;
  m_NextTargetTick = 0;
  memset((void *)m_RateSnap, 0x00, sizeof(m_RateSnap));
}
//----------------------------------------------------------------------------
void __fastcall TStopwatchElp::Reset()
{
  m_sw.Reset();
  m_sw.Start();
  m_StartTime = m_sw.Elapsed.TotalMilliseconds;
  m_StartTick = m_sw.Elapsed.Ticks;
  m_Cumulation = this->Cumulation();
  m_CumulationTick = this->CumulationTick();
  m_RateSnapIdx = 0;
  m_CheckCount = 0;
  memset(&m_RateSnap, 0x00, sizeof(m_RateSnap));
}
//----------------------------------------------------------------------------
double __fastcall TStopwatchElp::Cumulation()
{
  // unit: ms
  return m_sw.Elapsed.TotalMilliseconds - m_StartTime;
}
//----------------------------------------------------------------------------
__int64 __fastcall TStopwatchElp::CumulationTick()
{
  return m_sw.Elapsed.Ticks - m_StartTick;
}
//----------------------------------------------------------------------------
double __fastcall TStopwatchElp::Span()
{
  // unit: ms
  double now = this->Cumulation();
  double Result = now - m_Cumulation;
  m_Cumulation = now;
  return Result;
}
//----------------------------------------------------------------------------
__int64 __fastcall TStopwatchElp::SpanTick()
{
  __int64 Result;
  Result = this->CumulationTick() - m_CumulationTick;
  m_CumulationTick = this->CumulationTick();
  return Result;
}
//----------------------------------------------------------------------------
TDateTime __fastcall TStopwatchElp::SpanTime()
{
  // 1 millisecond
  double oneMs = double(EncodeTime(0,0,0,1));
  return TDateTime(oneMs * this->Span());
}
//----------------------------------------------------------------------------
TDateTime __fastcall TStopwatchElp::CumulationTime()
{
  // 1 millisecond
  double oneMs = double(EncodeTime(0,0,0,1));
  return TDateTime(oneMs * this->Cumulation());
}
//----------------------------------------------------------------------------
bool __fastcall TStopwatchElp::CheckWaitTime(double aTime, double *aRate)
{
  // a object of TStopwatchElp that only allows one wait time
  // aRate: update rate per second
  bool Result = false;
  double Cumulation = 0;

  if (m_CheckCount == 0) {
    m_WaitTime = aTime;
    m_Timeout = aTime + this->Cumulation();
  }
  else {
    if (this->Cumulation() >= m_Timeout) {
      m_Timeout = aTime + this->Cumulation();
      m_CheckCount = 0;
      Result = true;
    }
  }

  if (aRate != NULL) {
    if (m_RateSnapIdx < RateSnapNum) {
      m_RateSnap[m_RateSnapIdx++] = Span();
    }
    else {
      m_RateSnapIdx = 0;
      m_RateSnap[m_RateSnapIdx] = Span();
    }

    for (int i = 0; i < RateSnapNum; i++) {
      Cumulation += m_RateSnap[i];
    }
    *aRate = 1000./(Cumulation/RateSnapNum);
  }
  ++m_CheckCount;
  return Result;
}
//----------------------------------------------------------------------------
bool __fastcall TStopwatchElp::CheckWaitTick(__int64 aTick, double *aRate)
{
  // aTick: 目標間隔的 Tick 數
  // aRate: 傳回當前計算的平均頻率
  bool Result = false;
  double Cumulation = 0;
  __int64 currentTick = m_sw.Elapsed.Ticks;

  // 第一次呼叫：初始化目標時間
  if (m_CheckCount == 0) {
    m_WaitTick = aTick;
    m_NextTargetTick = currentTick + aTick;
  }
  else {
    // 絕對時間對齊：檢查是否抵達或超過了「預期目標點」
    if (currentTick >= m_NextTargetTick) {

      // --- 核心改進：精準校正 ---
      // 不要用 currentTick + aTick，否則會把已經發生的延遲帶入下一次
      // 使用 m_NextTargetTick 直接累加，強制維持固定頻率
      m_NextTargetTick += aTick;

      // 如果系統延遲太嚴重（掉幀），可在此加入 while 迴圈追趕
      while (currentTick >= m_NextTargetTick) {
        m_NextTargetTick += aTick;
      }
      Result = true;
    }
  }

  // 計算頻率 (Rate) 邏輯
  if (aRate != NULL) {
    // 使用 SpanTick 獲取本次與上次實際觸發的間隔
    // 注意：SpanTick 內部會更新 m_CumulationTick
    __int64 sTick = this->SpanTick();

    // 滾動平均濾波器
    m_RateSnap[m_RateSnapIdx] = (double)sTick;
    m_RateSnapIdx = (m_RateSnapIdx + 1) % RateSnapNum;

    for (int i = 0; i < RateSnapNum; i++) {
      Cumulation += m_RateSnap[i];
    }

    // 頻率計算公式：(Stopwatch 頻率) / (平均每幀 Ticks)
    // TStopwatch::Frequency 是每秒的 Ticks 數
    if (Cumulation > 0) {
      double avgTick = Cumulation / RateSnapNum;
      *aRate = (double)TStopwatch::Frequency / avgTick;
    }
    else {
      *aRate = 0;
    }
  }

  if (Result) {
    m_CheckCount++; // 只有在觸發時增加計數，或依需求持續增加
  }

  return Result;
}
//----------------------------------------------------------------------------
String __fastcall TStopwatchElp::SpanStr()
{
  return FormatDateTime(L"s.zzz", this->SpanTime());
}
//----------------------------------------------------------------------------
String __fastcall TStopwatchElp::CumulationStr()
{
  return FormatDateTime(L"s.zzz", this->CumulationTime());
}
//----------------------------------------------------------------------------
TTargetFrequency::TTargetFrequency()
{
  m_sw = TStopwatch::StartNew();
}
//----------------------------------------------------------------------------
void __fastcall TTargetFrequency::DelayTime(double aTime)
{
  __int64 lCount = 0;
  const __int64 lMaxCount = m_sw.Elapsed.TicksPerSecond;
  double lStart = m_sw.Elapsed.TotalMilliseconds;

  do {
    // PAUSE 指令：減少功耗、降低熱量，並優化超執行緒效能
    _mm_pause();

    if ((m_sw.Elapsed.TotalMilliseconds - lStart) > aTime) {
      break;
    }
  } while (++lCount < lMaxCount);
}
//----------------------------------------------------------------------------
void __fastcall TTargetFrequency::DelayTick(__int64 aTick)
{
  if (aTick <= 0) {return;}

  __int64 lStart = m_sw.Elapsed.Ticks;
  __int64 lTarget = lStart + aTick;

  // Spin-loop 鎖定目標 Tick
  const __int64 maxSpin = 10000000;  // 防卡死，約幾十 ms
  __int64 lCount = 0;
  while (m_sw.Elapsed.Ticks < lTarget) {
    _mm_pause();
    if (++lCount > maxSpin) { // 安全閥
      break;
    }
  }
}
//----------------------------------------------------------------------------
double __fastcall TTargetFrequency::WaitTime(double aCurrentHz, double aTargetHz)
{
  // return 0: 等待條件不符, 大於0: 等待時間 ms, 小於0: 不用等待 ms
  double Result, lCurrentInterval, lTargetInterval;
  if (aCurrentHz <= 0 || aTargetHz <= 0) {
    return 0;
  }

  lCurrentInterval = 1000./aCurrentHz;
  lTargetInterval = 1000./aTargetHz;
  Result = lTargetInterval - lCurrentInterval;

  if (Result >= 0) {
    this->DelayTime(Result);
  }
  return Result;
}
//----------------------------------------------------------------------------
void __fastcall T1msEx::InitSetting(DWORD aPeriod, DWORD aFlags)
{
  m_hTimer = NULL;

  // TODO: HANDLE WINAPI CreateTimerQueue(void);
  m_hTimerQueue = NULL;

  //
  m_TimerEvent = NULL;

  // 第一次呼叫回乎函數的延遲時間(0 勁可能的快) 單位 ms
  m_DueTime = 0;

  //執行時間間隔 單位 ms
  m_Period = aPeriod;

  // WT_EXECUTEDEFAULT: if you want a non-I/O component thread to process the work item
  // WT_EXECUTEINIOTHREAD: if you want to issue an asynchronous I/O request at a certain time
  // WT_EXECUTEINPERSISTENTTHREAD: if you want a thread that never dies to process the work item
  // WT_EXECUTELONGFUNCTION: if you think that your work item will require a long time to execute
  // WT_EXECUTEINTIMERTHREAD: It should not perform alertable wait operations.
  //
  // explaining:
  // This component creates the single waitable timer kernel object and manages its due time.
  // The component always consists of a single thread.
  // When you call CreateTimerQueueTimer, you cause the timer component's thread to wake up,
  // add your timer to a queue of timers, and reset the waitable timer kernel object.
  // The timer component's thread then goes into an alertable sleep,
  // waiting for the waitable timer to queue an APC to it.
  // After the waitable timer queues the APC, the thread wakes up, updates the timer queue,
  // resets the waitable timer, and then decides what to do with the work item that should now be executed.
  //
  // Next, the thread checks for the following flags: WT_EXECUTEDEFAULT, WT_EXECUTEINIOTHREAD,
  // WT_EXECUTEINPERSISTENTTHREAD, WT_EXECUTELONGFUNCTION, and WT_EXECUTEINTIMERTHREAD.
  // By now, it should be obvious what the WT_EXECUTEINTIMERTHREAD flag does:
  // it causes the timer component's thread to execute the work item.
  // While this makes execution of the work item more efficient, it is very dangerous!
  // If the work item function blocks for a long time,
  // the timer component's thread can't do anything else.
  // The waitable timer might still be queuing APC entries to the thread,
  // but these work items won't be handled until the currently executing function returns.
  // If you plan to execute code using the timer thread,
  // the code should execute quickly and should not block.

  // The WT_EXECUTEINIOTHREAD, WT_EXECUTEINPERSISTENTTHREAD, and WT_EXECUTEINTIMERTHREAD flags are mutually exclusive.
  // If you don't pass any of these flags (or use the WT_EXECUTEDEFAULT flag),
  // the work item is queued to the non-I/O component's threads. Also,
  // the WT_EXECUTELONGFUNCTION flag is ignored if the WT_EXECUTEINTIMERTHREAD flag is specified
  m_Flags = aFlags;

  // 提高計時器精度到 1ms
  SetTimerResolution(1);
}
//----------------------------------------------------------------------------
VOID CALLBACK fnWaitOrTimerCB(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
  T1msEx *lThis = (T1msEx *)lpParam;

  // 事件內 不要有 sleep函數
  if (lThis->TimerEvent != NULL) {
    lThis->TimerEvent(lThis);
  }
}
//----------------------------------------------------------------------------
__fastcall T1msEx::T1msEx(WAITORTIMERCALLBACK apfnCB, DWORD aPeriod, DWORD aFlags, TComponent *aUser)
  : TComponent(NULL)
{
  InitSetting(aPeriod, aFlags);

  //
  m_pfnCB = apfnCB;

  //
  if (aUser == NULL) {
    m_User = (PVOID)this;
  }
  else {
    m_User = (PVOID)aUser;
  }
}
//-------------------------------------------------------------------------------------------
__fastcall T1msEx::T1msEx(DWORD aPeriod, DWORD aFlags, TComponent* Owner)
  : TComponent(Owner)
{
  InitSetting(aPeriod, aFlags);

  //
  m_pfnCB = fnWaitOrTimerCB;

  //
  m_User = (PVOID)this;

  m_PausePeriod = 0xFFFFFFFF;
}
//-------------------------------------------------------------------------------------------
bool __fastcall T1msEx::DeleteTimer()
{
  bool Result = false;
  if (m_hTimer != NULL) {

    // 1. INVALID_HANDLE_VALUE 等待 Callback return, 後返回(過載的情況下 要等到 Queue內 task處理完畢)
    // 2. NULL, 立即返回
    //   如果 參數1情況 DeleteTimer fnution 最後被呼叫程式不會當 只是執行緒 鎖死
    //   如果 參數2情況 DeleteTimer fnution 最後被呼叫 程式死給你看
    if (DeleteTimerQueueTimer(NULL, m_hTimer, INVALID_HANDLE_VALUE) != 0) {
      m_hTimer = NULL;
      Result = true;
    }
  }
  return Result;
}
//----------------------------------------------------------------------------
__fastcall T1msEx::~T1msEx()
{
  if (m_hTimerQueue != NULL){

    // INVALID_HANDLE_VALUE 等待 Callback return 後返回
    // NULL 立即返回

    // TODO:
    // DeleteTimerQueueEx(m_hTimerQueue, INVALID_HANDLE_VALUE);
  }
  else {
    DeleteTimer();
  }
  ResetTimerResolution();
}
//----------------------------------------------------------------------------
bool __fastcall T1msEx::SetTimerResolution(unsigned int aTimer)
{
  bool Result = false;
  TIMECAPS tc;

  if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
    m_TimerResolution = fnMin(fnMax(tc.wPeriodMin, aTimer), tc.wPeriodMax);
    if (timeBeginPeriod(m_TimerResolution) == TIMERR_NOERROR) {
      Result = true;
    }
  }
  return Result;
}
//----------------------------------------------------------------------------
bool __fastcall T1msEx::ResetTimerResolution()
{
  if (timeEndPeriod(m_TimerResolution) == TIMERR_NOERROR) {
    return true;
  }
  else {
    return false;
  }
}
//----------------------------------------------------------------------------
bool __fastcall T1msEx::IsActive()
{
  if (m_hTimer != NULL) {
    return true;
  }
  else {
    return false;
  }
}
//----------------------------------------------------------------------------
bool __fastcall T1msEx::Start(int aDueTime)
{
  bool Result = false;

  if (IsActive() == false) {
    m_DueTime = aDueTime;
    Result = CreateTimerQueueTimer(&m_hTimer,
                                   m_hTimerQueue,
                                   m_pfnCB,
                                   m_User,
                                   m_DueTime,
                                   m_Period,
                                   m_Flags);
  }
  return Result;
}
//----------------------------------------------------------------------------
int __fastcall T1msEx::ChangePeriod(DWORD aPeriod, DWORD aDueTime)
{
  // return 0: fail, 1: success, 2: no change
  int Result;
  if (aPeriod != m_Period || aDueTime != m_DueTime) {
    if (ChangeTimerQueueTimer(NULL, m_hTimer, aDueTime, aPeriod) != 0) {
      m_Period = aPeriod;
      m_DueTime = aDueTime;
      Result = 1;
    }
    else {
      Result = 0;
    }
  }
  else {
    Result = 2;
  }
  return Result;
}
//----------------------------------------------------------------------------
bool __fastcall T1msEx::Resume(DWORD aDueTime)
{
  if (IsActive() == false) {
    return Start(aDueTime);
  }
  else {
    if (ChangeTimerQueueTimer(NULL, m_hTimer, aDueTime, m_Period) != 0) {
      return true;
    }
  }
  return false;
}
//----------------------------------------------------------------------------
bool __fastcall T1msEx::Pause(DWORD aDueTime)
{
  if (IsActive() == true) {
    if (ChangeTimerQueueTimer(NULL, m_hTimer, aDueTime, m_PausePeriod) != 0) {
      return true;
    }
  }
  return false;
}
//----------------------------------------------------------------------------
int __fastcall T1msEx::ChangeFlags(DWORD aFlags, DWORD aDueTime)
{
  // WT_EXECUTEDEFAULT = 0x00000000
  // WT_EXECUTEINIOTHREAD = 0x00000001
  // WT_EXECUTEINPERSISTENTTHREAD = 0x00000080
  // WT_EXECUTELONGFUNCTION = 0x00000010
  // WT_TRANSFER_IMPERSONATION = 0x00000100 ; 支援 Windows XP SP2 and Windows Server 2003 以後版本

  // return 0: fail, 1: success, 2: no change
  int Result;
  bool hr;

  if (aFlags != m_Flags || aDueTime != m_DueTime) {
    if (DeleteTimer() == true) {
      hr = CreateTimerQueueTimer(&m_hTimer,
                                 m_hTimerQueue,
                                 m_pfnCB,
                                 m_User,
                                 aDueTime,
                                 m_Period,
                                 aFlags);
      if (hr == true) {
        m_Flags = aFlags;
        m_DueTime = aDueTime;
        Result = 1;
      }
      else {
        Result = 0;
      }
    }
    else {
      Result = 0;
    }
  }
  else {
    Result = 2;
  }
  return Result;
}
//----------------------------------------------------------------------------

