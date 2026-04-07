//---------------------------------------------------------------------------
#include <System.hpp>
#include <mmsystem.h>
#pragma hdrstop

#include "uDosCmd.h"
#include "uUseNtDll.h"
#include "uUtilize.h"
#include "uThreadTimer.h"
#include <avrt.h>
#include <mmsystem.h>

// 高精度忙等專用的 _mm_pause 定義
#if defined(__BORLANDC__) && !defined(__clang__)
  // 針對經典 BCC32 編譯器
  #define _mm_pause() __asm { pause }
#else
  // 針對 BCC64 或 BCC32C (CLANG 型編譯器)
  #include <immintrin.h>
#endif


#if defined(_WIN64)
  #pragma comment(lib, "avrt")
#else
  #pragma comment(lib, "avrt.lib")
#endif


//#include <processthreadsapi.h>
#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(&UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TThreadAsynTimer::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//
//   注意:
//     1.m_SynMode == 1; "AMethod" 是由 main thread呼叫, 且同步機制是Blocking模式,
//       所以對 "AMethod" 的呼叫(或執行)是要等到 main thread 處理完它的事情後,
//       才會呼叫(或執行)"AMethod"(注: 在等待期間 thread blocking, 被呼叫(或執行)的
//       "AMethod" 不會被加入 queue 內)
//
//     2.m_SynMode == 2; "AMethod" 是由 main thread呼叫, 且同步機制是 Non Blocking模式,
//       所以對 "AMethod" 的呼叫(或執行)是要等到 main thread 處理完它的事情後,
//       才會呼叫(或執行)"AMethod"(注: 在等待期間 thread Non blocking, 被呼叫(或執行)的
//       "AMethod" 會被加入 queue 內)
//
//     3.m_SynMode == 0;, 同步機制會由User掌控(在main form 看到
//       ThreadID != MainThreadID)
//
//     4.異步,一般使用在和顯示無關的背景工作
//       如: 檔案複製, 存檔, 搬移, 或是通訊傳輸(indy, COMM Port)
//
//     5.如果"硬要"在異步的情況下, 要在Main Thread顯示,有以下限制(與顯示物件特性有關)
//       5.1 TCanvas 相關的物件, 需要, Lock() ... UnLock();
//       5.2 待補...
//
// Synchronize and Queue with Parameters
//Posted on January 30, 2011 by Uwe Raabe
//In Delphi’s TThread class there is Synchronize to call a method in the context of the GUI thread.
//This is necessary in case, for example, you want to update a progressbar or a status label in a form, because the VCL is not thread safe.
//While Synchronize is a blocking method (i.e. the thread code continues when the GUI thread has finished the method call),
//recent versions of Delphi introduced a non-blocking Queue method.
//The drawback of using Synchronize is that it is very cumbersome to handle parameters during such a call.
//The standard solution is to use fields in the TThread descendant holding the parameters during Synchronize,
//but this won’t work properly with Queue.
//As Queue is non-blocking there might be multiple calls to Queue waiting to be processed,
//which often cannot share the same field.
//Worse, using Queue access to these fields have to be thread safe, too.
//The queuing thread may just set this field for the next call to Queuewhile a previous Queue call is executed
//in the main thread just using this field.
//As this problem doesn’t exist with Synchronize, switching
//from Synchronize to Queue just has to be done more careful.
//One way to deal with parameters for Synchronize is the use of Anonymous Methods.
//There is an overloaded version of Synchronize and Queue taking a TThreadProcedure as parameter:
//

int __fastcall fnGetCPUCount();
//---------------------------------------------------------------------------
bool __fastcall fnDisableTimerPowerThrottling()
{
  // 額外保險：強制進程退出「效率模式」(Efficiency Mode)
  // 這能確保執行緒更傾向於留在 P-Core
  PROCESS_POWER_THROTTLING_STATE eco = {0};
  eco.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  eco.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
  eco.StateMask = 0; // 關閉效率模式

  SetProcessInformation(::GetCurrentProcess(),
                        ProcessPowerThrottling,
                        &eco,
                        sizeof(eco));

  PROCESS_POWER_THROTTLING_STATE pts = {};
  pts.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  pts.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION |
                    PROCESS_POWER_THROTTLING_EXECUTION_SPEED;

  pts.StateMask = 0;  // 0 = 不節流 timer resolution

  return ::SetProcessInformation(::GetCurrentProcess(),
                                 ProcessPowerThrottling,
                                 &pts,
                                 sizeof(pts));
}
//---------------------------------------------------------------------------
int __fastcall fnGetCPUCount()
{
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return (int)sysInfo.dwNumberOfProcessors;
}
//---------------------------------------------------------------------------
String __fastcall TThxTimer::GetUniqueEventName()
{
  // 使用物件的記憶體位址 (this) 確保在同一個 Process 內唯一
  // 並且加上類別名稱，方便在核心除錯工具中識別
  return Format("TThxTimer_Event_%p", ARRAYOFCONST((this)));
}
//---------------------------------------------------------------------------
__fastcall TThxTimer::TThxTimer()
  : TThread(false)
{
  // === 同步與執行緒相關 ===
  m_bBusyTerminate = false;    // 用於忙等迴圈安全結束
  m_SynMode = 0;               // 預設非同步模式（0: direct, 1: Synchronize, 2: Queue）

  // === 計時器狀態 ===
  m_Enabled = false;           // 預設關閉
  m_FrequencyPrecision = 1.0;  // 頻率精準度（Hz）
  m_DefTimerResolution = 1.0;  // 預設 1 ms（系統常見值，高精度子類會覆蓋）
  this->Frequency = 62.5;      // 預設 62.5 Hz（週期 16 ms）


  // === CPU 與調度相關（預設不綁定、不隔離、不用 MMCSS） ===
  m_CPUMask = 0;
  m_Affinity = false;
  m_NeedPrecision  = false;    // 預設不追求極致精度（降低 CPU 負載）
  m_UseMMCSS = false;          // 預設關閉 Pro Audio 模式（降低系統開銷）
  m_CPUIsolation = false;      // 預設不隔離 CPU
  m_ProAudioTask = NULL;

  // === 系統級優化: 禁用 Power Throttling（功率節流）===
  fnDisableTimerPowerThrottling();

  // === 資源與生命週期 ===
  // Automatically delete while terminating.
  // 1. true: 不需要(不能) delete thread object
  //    釋放資源 只需要 呼叫 Terminate(),
  //    離開順序, Execute(離開), OnTerminate, ~destructor
  //    注意: 在這種請況下 不能再用 WaitFor() 去等待資源釋放
  //    不然會進到永遠等待的死結(因為 WaitFor() 離開的時機(Execute(離開)..),
  //    已經過了)
  //
  // 2. false: 釋放資源必需要 delete thread object
  //    delete 後 method 被呼叫順序為:
  //    ~destructor, Execute(離開), OnTerminate
  this->FreeOnTerminate = false; // 必須手動 delete，避免死鎖

  // === 事件與計時器 ===
  /* TODO... 需要才開發
  CreateWaitableTimerEx(NULL,
                        NULL,
                        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                        TIMER_ALL_ACCESS);
 */


  m_hWaitEvent = fnCreateEvent(false, // false = 自動重置; true = 手動重置
                               true, // false = Event初始,為非激發狀態; true = Event初始,為激發狀態
                               GetUniqueEventName()); // Event物件名稱

  m_swDelay = TStopwatch::StartNew();
  m_TimerEvent = NULL;
  m_pfnTimer = NULL;
}
//---------------------------------------------------------------------------
__fastcall TThxTimer::~TThxTimer()
{
  if (this->Suspended) {
    this->Start();
  }

  // trigger 離開條件:
  m_bBusyTerminate = true;

  this->Terminate();
  this->RemoveQueuedEvents(this);
  SetEvent(m_hWaitEvent);

  // wait for 離開條件:
  // 1. 等待 Execute() 離開.
  // 2. 如果 有Assign OnTerminate事件, 要等待 OnTerminate事件完成.
  // 3. WaitFor 回傳值, 是 SetReturnValue(int)
  this->WaitFor();

  if (m_ProAudioTask) {
    AvRevertMmThreadCharacteristics(m_ProAudioTask);
  }

  if (m_CPUIsolation == true) {
    fnReleaseAffinityMaskExclusive(m_CPUMask);
  }

  // release ...
  CloseHandle(m_hWaitEvent);
}
//---------------------------------------------------------------------------
double __fastcall TThxTimer::GetTimerResolution()
{
  // 單位：ms
  ULONG lMin = 0, lMax = 0, lCur = 0;
  if (fnNtQueryTimerResolution(&lMax, &lMin, &lCur)) {
    m_CurrentTimerResolution = static_cast<double>(lCur) / 10000.0;
  }
  else {
    // 查詢失敗，回傳 0.0
    m_CurrentTimerResolution = 0.0;
  }
  return m_CurrentTimerResolution;
}
//---------------------------------------------------------------------------
bool __fastcall TThxTimer::SetTimerResolution(double aInterval)
{
  // 單位：ms
  if (aInterval < 0.5 || aInterval > 15.625) {
    // 超出合理範圍，直接拒絕
    return false;
  }

  ULONG lInterval = static_cast<ULONG>(aInterval * 10000.0 + 0.5); // 四捨五入
  ULONG lCur = 0;

  if (fnNtSetTimerResolution(lInterval, TRUE, &lCur)) {
    m_CurrentTimerResolution = static_cast<double>(lCur) / 10000.0;
    return true;
  }
  else {
    return false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TThxTimer::SetEnabled(bool b)
{
  m_Enabled = b;
  if (m_Enabled == true) {
    SetEvent(m_hWaitEvent);
  }
}
//---------------------------------------------------------------------------
void __fastcall TThxTimer::SetFrequency(double aFrequency)
{
  int lInterval;
  lInterval = int(1000./aFrequency) - 2;
  m_Interval = (lInterval < 1) ? 1 : lInterval;
  m_Frequency = aFrequency;
  m_DoCount = 0;
}
//---------------------------------------------------------------------------
double __fastcall TThxTimer::GetFrequency()
{
  return m_Frequency;
}
//---------------------------------------------------------------------------
DWORD_PTR TThxTimer::GetSystemAffinityMask()
{
  DWORD_PTR processMask, systemMask;
  GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask);
  return systemMask;
}
//---------------------------------------------------------------------------
UINT64 __fastcall TThxTimer::GetCPUMask(int aCpuIdx)
{
  // 取得系統實際擁有的邏輯核心數
  int cpuCount = fnGetCPUCount();

  // 安全檢查：
  // 1. 索引不能小於 0
  // 2. 索引不能超過系統核心數 (例如 8 核，索引最高為 7)
  if (aCpuIdx < 0 || aCpuIdx >= cpuCount) {
    return 0; // 回傳 0 代表無效遮罩
  }

  // 執行位元左移運算
  // 第 0 核 = 1 << 0 = 0001 (1)
  // 第 1 核 = 1 << 1 = 0010 (2)
  // 第 2 核 = 1 << 2 = 0100 (4)
  return (UINT64)1 << aCpuIdx;
}
//---------------------------------------------------------------------------
/**
 * 函式說明：提升計時器執行緒優先權與環境優化
 * ---------------------------------------------------------------------------
 * [操作流程]
 * 1. 檢查並校驗 CPUMask。
 * 2. 執行全域 CPU 隔離 (如果開啟 m_CPUIsolation)。
 * 3. 綁定當前執行緒至專屬核心。
 * 4. 提升執行緒優先權至 TIME_CRITICAL。
 * 5. 啟用 MMCSS (Pro Audio) 模式以獲取 OS 調度保證。
 * 6. 停用 Priority Boost 確保排程穩定性。
 * ---------------------------------------------------------------------------
 */
bool __fastcall TThxTimer::IncreasePriority()
{
  bool Result = true;
  HANDLE lCurrentThread = ::GetCurrentThread();

  // 綁定 affinity
  if (m_Affinity && m_CPUMask != 0) {
    DWORD_PTR validMask = (DWORD_PTR)m_CPUMask & GetSystemAffinityMask();
    if (validMask != 0) {

      if (m_CPUIsolation == true) {
        fnSetAffinityMaskExclusive(validMask);
      }

      if (SetThreadAffinityMask(lCurrentThread, validMask) == 0) {
        Result = false;
        // TODO: 紀錄錯誤訊息
      }
    }
    else {
      Result = false;
      // TODO: 紀錄錯誤訊息(L"無效的 CPUMask");
    }
  }

  // 在 TThxTimer::Execute 起始處加入
  if (m_NeedPrecision == true) {
    ::SetThreadPriority(lCurrentThread, THREAD_PRIORITY_TIME_CRITICAL);

    // 將執行緒標記為 Pro Audio 等級，獲得最穩定的調度
    if (m_UseMMCSS == true) {
      m_ProAudioTask = AvSetMmThreadCharacteristics(L"Pro Audio", &m_ProAudioTaskIndex);
      if (m_ProAudioTask != NULL) {
        AvSetMmThreadPriority(m_ProAudioTask, AVRT_PRIORITY_CRITICAL);
      }
    }
  }
  else {
    ::SetThreadPriority(lCurrentThread, THREAD_PRIORITY_HIGHEST);
  }

  // 告訴 Windows，這個執行緒不希望被動態提升優先權（防止 OS 介入排程）
  // 參數: TRUE，則會停用動態提升, 為 FALSE，則會啟用動態提升
  ::SetThreadPriorityBoost(lCurrentThread, TRUE);

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TThxTimer::Done()
{
  __int64 lStartTick = m_swDelay.Elapsed.Ticks;

  // 1秒內的總 Ticks 除以 目標頻率 = 每個週期應有的精確 Ticks
  __int64 lTargetTick = m_swDelay.Frequency/m_Frequency;

  if (m_TimerEvent != NULL) {
    m_TimerEvent(this);
  }

  if (m_SynMode == 0) {
    if (m_pfnTimer) {
      m_pfnTimer(m_PrevisDeltaTime);
    }
  }

  // 1秒調整一次負載
  if (m_sw.CheckWaitTime(1000, &m_CurrentFrequencyHz) == true) {
    if ((m_Frequency - m_CurrentFrequencyHz) > m_FrequencyPrecision*0.5) {
      if (--m_Interval < 1) {
        m_Interval = 1;
      }
    }
    else if ((m_Frequency - m_CurrentFrequencyHz) < -m_FrequencyPrecision*0.5) {
      if (++m_Interval > 1000./m_Frequency) {
        m_Interval = 1000./m_Frequency;
      }
    }
  }

  __int64 lLoadingTick = m_swDelay.Elapsed.Ticks - lStartTick;
  // 計算剩餘應補償的 Ticks
  __int64 lResidualTick = lTargetTick - m_WaitTick - lLoadingTick;
  if (lResidualTick > 0) {
    m_tf.DelayTick(lResidualTick);
  }
}
//---------------------------------------------------------------------------
void __fastcall TThxTimer::Do()
{
  DWORD dwRes;
  double lStartTime = m_swDelay.Elapsed.TotalMilliseconds;
  __int64 lStartTick = m_swDelay.Elapsed.Ticks;
  try {
    if (m_Enabled == true) {
      dwRes = WaitForSingleObject(m_hWaitEvent, (DWORD)m_Interval);
    }
    else {
      dwRes = WaitForSingleObject(m_hWaitEvent, INFINITE);
    }
  }
  catch (...) {
    SetEvent(m_hWaitEvent);
    return;
  }

  m_WaitTick = m_swDelay.Elapsed.Ticks - lStartTick;

  if (this->Terminated == true || m_bBusyTerminate == true) {
    return;
  }

  if (dwRes == WAIT_TIMEOUT) {
    switch (m_SynMode) {
    case 1: this->Synchronize(Done); break;
    case 2: this->Queue(Done); break;
    default: Done();
    }
    m_DoCount++;
    m_PrevisDeltaTime = m_swDelay.Elapsed.TotalMilliseconds - lStartTime;
  }
  else if (dwRes == WAIT_OBJECT_0) {
    // TODO:
  }
  else{
    // TODO:
  }
}
//---------------------------------------------------------------------------
// 基礎計時執行緒：適合一般週期任務、多功能使用
// - 預設不綁定核心、不啟用 MMCSS、不強制高解析度
// - CPU 負載低，適合背景監控、定時檢查、log 等
// - 若需高精度（256 Hz 以上、偏差 <0.1 ms），請使用 TAccuracyTimer
//---------------------------------------------------------------------------
__fastcall TThreadTimer::TThreadTimer()
  : TThxTimer()
{
  // 預設輕量模式：不綁定核心、不用 MMCSS、降低 CPU 負載
  m_Affinity = false;
  m_CPUMask = 0;
  m_UseMMCSS = false;       // 預設關閉，降低系統負擔與調度開銷
  m_NeedPrecision = false;  // 不需要極致精度，適合一般週期任務
}
//---------------------------------------------------------------------------
__fastcall TThreadTimer::TThreadTimer(UINT64 aCPUMask)
  : TThxTimer()
{
  m_Affinity = (aCPUMask != 0);
  m_CPUMask = aCPUMask;
  m_UseMMCSS = false;       // 預設關閉，若需要高穩定可手動開啟
  m_NeedPrecision = false;

  // 若綁定核心，建議設 0.5 ms 解析度（但仍不強制 MMCSS）
  SetTimerResolution(0.5);
}
//---------------------------------------------------------------------------
__fastcall TThreadTimer::TThreadTimer(int aCPUIndex)
  : TThxTimer()
{
  m_Affinity = true;
  m_UseMMCSS = false;  // 預設關閉
  m_NeedPrecision = false;

  if (aCPUIndex < 0) {
    m_CPUMask = GetCPUMask(fnGetCPUCount() - 1); // 最後一個核心
  } else {
    m_CPUMask = GetCPUMask(aCPUIndex);
  }

  if (m_CPUMask == 0) {
    m_Affinity = false;  // 防呆：無效 mask 自動關閉綁定
  }

  SetTimerResolution(0.5);
}
//---------------------------------------------------------------------------
__fastcall TThreadTimer::~TThreadTimer()
{
  // TODO:
}
//---------------------------------------------------------------------------
void __fastcall TThreadTimer::Execute()
{
  // waitfor 回傳值
  this->SetReturnValue(777);

  // 暖機時同時提升優先權，讓系統更快穩定該 thread
  IncreasePriority();
  Sleep(500); // 暖機時間可稍減，但確保在優先權提升後

  // minimum timer resolution; units: ms
  const int minRes = 1;
  MMRESULT hr;
  double lTimerResolution = GetTimerResolution();
  if (lTimerResolution < minRes) {
    hr = timeBeginPeriod(minRes);
    if (hr == TIMERR_NOERROR) {
      __try {
        do {
          Do();
        } while (this->Terminated == false);
      }
      __finally {
        timeEndPeriod(minRes);
        if (m_ProAudioTask) {
          AvRevertMmThreadCharacteristics(m_ProAudioTask);
          m_ProAudioTask = NULL; // 增加安全性
        }
      }
    }
  }
  else {
    do {
      Do();
    } while (this->Terminated == false);
  }
}
//---------------------------------------------------------------------------
void __fastcall TAccuracyTimer::InitHighPrecision()
{
  m_NeedPrecision = true;    // 精準模式：滿載 CPU、使用高優先級
  m_UseMMCSS = true;         // Pro Audio 模式：抖動幅度更均勻、大跳次數更少
                             // 注意：CPU 使用率接近 100% 但系統會微調
                             // 若關閉 (false)：CPU 滿載，但抖動幅度較大、發生次數較少
  SetTimerResolution(0.5);   // 強制 0.5 ms 解析度（高精度必要）
}
//---------------------------------------------------------------------------
// 高精度計時執行緒：專為 100 Hz 以上高頻、長期穩定任務設計
// - 預設啟用 MMCSS Pro Audio（抖動更均勻、大跳更少）
// - 預設啟用 CPU 隔離（當綁定核心時）
// - 強制使用 0.5 ms 解析度
// - CPU 負載接近滿載，適合飛控模擬、音訊處理、輸入延遲優化等
//---------------------------------------------------------------------------
__fastcall TAccuracyTimer::TAccuracyTimer()
  : TThxTimer()
{
  InitHighPrecision();
}
//---------------------------------------------------------------------------
__fastcall TAccuracyTimer::TAccuracyTimer(UINT64 aCPUMask)
  : TThxTimer()
{
  m_CPUMask = aCPUMask;
  m_Affinity = (aCPUMask != 0);
  m_CPUIsolation = m_Affinity;  // 有綁定就預設隔離

  InitHighPrecision();
}
//---------------------------------------------------------------------------
__fastcall TAccuracyTimer::TAccuracyTimer(int aCPUIndex)
  : TThxTimer()
{
  m_Affinity = true;

  if (aCPUIndex < 0) {
    m_CPUMask = GetCPUMask(fnGetCPUCount() - 1); // 最後一個核心
  }
  else {
    m_CPUMask = GetCPUMask(aCPUIndex);
  }

  if (m_CPUMask == 0) {
    m_Affinity = false;           // 無效 mask 自動關閉綁定
    m_CPUIsolation = false;
  }
  else {
    m_CPUIsolation = true;        // 有有效綁定就隔離
  }

  InitHighPrecision();
}
//---------------------------------------------------------------------------
__fastcall TAccuracyTimer::~TAccuracyTimer()
{
  // TODO:
}
//---------------------------------------------------------------------------
void __fastcall TAccuracyTimer::DoWork()
{
  if (!this->Enabled) {
    Sleep(1); // 避免 Enabled=false 時 CPU 噴發
    return;
  }

  __int64 checkCurrentTick = m_swDelay.Elapsed.Ticks;
  // 如果還沒到目標時間，先不執行
  if (checkCurrentTick < m_NextTargetTick) {
    _mm_pause();
    return;
  }

  // 1. 獲取當前絕對 Ticks (QPC)
  __int64 currentTick = m_swDelay.Elapsed.Ticks;
  __int64 frequencyTick = m_swDelay.Frequency;

  // 2. 理論目標週期 (例如 1024Hz 約為 9765 Ticks)
  __int64 lTargetTick = frequencyTick/this->Frequency;

  // --- 關鍵修正：計算 OS 喚醒偏差 ---
  // 紀錄了理論上「這一次」應該觸發的時間點
  if (m_DoCount == 0) {m_NextTargetTick = currentTick;}
  __int64 osJitterTick = currentTick - m_NextTargetTick;

  // 3. 執行業務邏輯 (工作負載)
  if (m_TimerEvent != NULL) {m_TimerEvent(this);}
  if (m_pfnTimer != NULL) {m_pfnTimer(m_PrevisDeltaTime);}

  // 4. 計算工作完成後的總消耗時間 (從喚醒點開始算)
  __int64 lWorkDoneTick = m_swDelay.Elapsed.Ticks;
  __int64 lTotalElapsedTick = lWorkDoneTick - currentTick;

  // 5. 計算剩餘應補償的延遲量
  // 公式：目標週期 - OS喚醒延遲 - 邏輯工作耗時
  __int64 lDelayTick = lTargetTick - osJitterTick - lTotalElapsedTick;

  // 6. 執行高精度微秒補償 (Spin-loop)
  if (lDelayTick > 0) {
    m_tf.DelayTick(lDelayTick);
  }

  // 7. 更新下一個絕對目標點 (確保時間軸不漂移)
  do {
    m_NextTargetTick += lTargetTick;
  } while (m_NextTargetTick <= currentTick);

  // 8. 狀態更新與監控
  //m_sw.CheckWaitTime(1000, &m_CurrentFrequencyHz);
  // 計算單個週期的理論 Tick 數 (例如 1024Hz 對應的 Ticks)
  __int64 lTargetTickPerCycle = m_swDelay.Frequency/this->Frequency;
  m_sw.CheckWaitTick(lTargetTickPerCycle, &m_CurrentFrequencyHz);
  m_DoCount++;

  // 更新 DeltaTime 供下一次邏輯使用 (轉換回 ms)
  m_PrevisDeltaTime = (double)(m_swDelay.Elapsed.Ticks - currentTick)*1000.0/frequencyTick;
}
//---------------------------------------------------------------------------
void __fastcall TAccuracyTimer::Execute()
{
  this->SetReturnValue(777);

  // 暖機時同時提升優先權，讓系統更快穩定該 thread
  IncreasePriority();
  Sleep(500); // 暖機時間可稍減，但確保在優先權提升後

  // 確定是否需要調整系統全域計時器
  // 如果要求精度 <= 0.5ms，通常需要調用 NtSetTimerResolution (5000 = 0.5ms)
  double lTimerResolution = GetTimerResolution();
  bool useNtLowRes = (lTimerResolution <= 0.5);
  ULONG actualRes = 0;
  MMRESULT mmHr = TIMERR_NOCANDO;

  if (useNtLowRes) {
    fnNtSetTimerResolution(5000, TRUE, &actualRes);
  }
  else {
    mmHr = timeBeginPeriod(1);
  }

  // 3. 進入核心工作循環
  __try {
    // 為了減少循環內的 if 判斷，將 Terminated 檢查作為唯一條件
    while (!this->Terminated) {
      DoWork();
    }
  }
  __finally {
    if (useNtLowRes) {
      fnNtSetTimerResolution(5000, FALSE, &actualRes);
    }
    else if (mmHr == TIMERR_NOERROR) {
      timeEndPeriod(1);
    }

    if (m_ProAudioTask) {
      AvRevertMmThreadCharacteristics(m_ProAudioTask);
      m_ProAudioTask = NULL; // 增加安全性
    }
  }
}
//---------------------------------------------------------------------------

