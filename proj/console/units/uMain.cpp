#include <windows.h>

#pragma hdrstop
#pragma argsused

#include <tchar.h>
#include <stdio.h>
#include <vector>
#include "uThreadTimer.h"
#include "uUtilize.h"

// 統計緩存結構
struct TTimerStats
{
  double CurrentFrequencyHz;
  double DeltaTime;
  int DoCount;
  int OutCount;
  double CumulationTime;
};

// 使用原子操作或簡單變數（因為計時器寫入，Main讀取，衝突影響極小）
volatile TTimerStats g_LatestStats;
HANDLE g_hWaitEvent;
HANDLE g_hConOut;
TThxTimer *g_Timer;
bool g_CloseExit = false;
const double g_TgtFrequency = 256;
const double g_TimeAccuracy = 0.01;
const int g_LoadingCount = 100000;
std::vector<int> g_StressBuffer;

int g_DoCount, g_OutCount;
TStopwatchElp g_Elp;
TStringList *g_LogList;
COORD g_CursorPos;
//---------------------------------------------------------------------------
void fnSetupStressBuffer()
{
  // 建立約 128MB 的緩衝區 (32M 個 int)
  g_StressBuffer.resize(10 * 1024 * 1024);
  for(size_t i = 0; i < g_StressBuffer.size(); i++) {
    g_StressBuffer[i] = std::rand();
  }
}
//---------------------------------------------------------------------------
void fnRelease()
{
   if (g_Timer != NULL) {
    delete g_Timer;
    g_Timer = NULL;
  }

  if (g_LogList != NULL) {
    // 紀錄結束時間
    AnsiString lTime = FormatDateTime("HH:MM:SS.zzz", g_Elp.CumulationTime());
    g_LogList->Add(Format("%s, Stop", ARRAYOFCONST((lTime.c_str()))));
    g_LogList->SaveToFile("ThreadTimerLog.txt");
    delete g_LogList;
    g_LogList = NULL;
  }

  if (g_hWaitEvent != NULL) {
    CloseHandle(g_hWaitEvent);
    g_hWaitEvent = NULL;
  }

  if (g_hConOut != NULL) {
    CloseHandle(g_hConOut);
  }
}
//---------------------------------------------------------------------------
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
  switch (dwCtrlType) {
  case CTRL_CLOSE_EVENT:
    SetEvent(g_hWaitEvent);
    g_CloseExit = true;

    // 重要 Delay : time out max 5000ms
    Sleep(5000);

    fnRelease();

    return true;
  default:;
  }
  return false;
}
//---------------------------------------------------------------------------
HANDLE fnCreateConsolOutHandle()
{
  // return CreateFile(L"CONOUT$", GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ, 0,OPEN_EXISTING, 0, 0);
  return GetStdHandle(STD_OUTPUT_HANDLE);
}
//---------------------------------------------------------------------------
COORD fnGetCursorPos()
{
  CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
  GetConsoleScreenBufferInfo(g_hConOut, &csbiInfo);
  return csbiInfo.dwCursorPosition;
}
//---------------------------------------------------------------------------
void fnSetCursorPos(COORD aPos)
{
  SetConsoleCursorPosition(g_hConOut, aPos);
}
//---------------------------------------------------------------------------
void __cdecl fnThxTimer(double aPrevisDeltaTime)
{
  if (aPrevisDeltaTime == 0) {return;}

  ++g_DoCount;
  const double lTgtTime = 1000.0/g_TgtFrequency;
  const double lResolution = lTgtTime*g_TimeAccuracy;
  const double lMin = lTgtTime - lResolution;
  const double lMax = lTgtTime + lResolution;

  // loading
  // --- 嚴格負載區：緩衝區壓力測試 ---
  if (g_LoadingCount > 0 && !g_StressBuffer.empty()) {
    int bufferSize = g_StressBuffer.size();
    int mask = bufferSize - 1; // 假設 size 為 2 的冪次方，加速取模
    volatile int dummy;

    for (int i = 0; i < g_LoadingCount; i++) {
      // 透過簡單的偽隨機方式存取記憶體的不同位置
      // 這種讀取模式會讓 L1/L2/L3 Cache 疲於奔命
      int targetIdx = (i * 1103515245 + 12345) & mask;
      dummy = g_StressBuffer[targetIdx];

      // 偶爾寫入，增加記憶體匯流排壓力
      if (i % 100 == 0) {
        // 使用 volatile 確保運算不會被編譯器優化跳過
        g_StressBuffer[targetIdx] = static_cast<int>(std::sqrt(static_cast<float>(dummy))) + 1;
      }
    }
  }

  // --- 異常檢查與 Log (存入 StringList 是記憶體操作，速度極快) ---
  if (aPrevisDeltaTime > lMax || aPrevisDeltaTime < lMin) {
    ++g_OutCount;
    AnsiString lTime = FormatDateTime("HH:MM:SS.zzz", g_Elp.CumulationTime());
    double diff = (aPrevisDeltaTime > lMax) ?
                  (aPrevisDeltaTime - lTgtTime - lResolution) :
                  (aPrevisDeltaTime - lTgtTime + lResolution);

    g_LogList->Add(Format("%s, Elapse:%.3fms, OutOfRange:%.3fms",
                   ARRAYOFCONST((lTime.c_str(), aPrevisDeltaTime, diff))));
  }

  // --- 更新非同步顯示緩衝區 ---
  g_LatestStats.CurrentFrequencyHz = g_Timer->CurrentFrequencyHz;
  g_LatestStats.DeltaTime = aPrevisDeltaTime;
  g_LatestStats.DoCount = g_DoCount;
  g_LatestStats.OutCount = g_OutCount;
  g_LatestStats.CumulationTime = g_Elp.CumulationTime();
}
//---------------------------------------------------------------------------
TThxTimer * __cdecl fnCreateThxTimer()
{
  int lCnt = fnGetCPUCount();
  #ifdef _WIN64
//  g_Timer = new TThreadTimer(0x04);
  g_Timer = new TAccuracyTimer(24);
  #else
  //g_Timer = new TThreadTimer();
  g_Timer = new TAccuracyTimer(12);
  #endif
  g_Timer->pfnTimer = fnThxTimer;
  g_Timer->Frequency = g_TgtFrequency;
  g_Timer->SynMode = 0;
  return g_Timer;
}
//---------------------------------------------------------------------------
 int _tmain(int argc, _TCHAR* argv[])
{
  srand(GetTickCount());  // 或用 time(NULL)
  SetConsoleCtrlHandler(HandlerRoutine, true);

  // 不顯示游標
  COORD lCursorPos;
  CONSOLE_CURSOR_INFO lCursorInfo;
  g_hConOut = fnCreateConsolOutHandle();
  GetConsoleCursorInfo(g_hConOut, &lCursorInfo);
  lCursorInfo.bVisible = false;
  SetConsoleCursorInfo(g_hConOut, &lCursorInfo);
  fnSetupStressBuffer();

  g_LogList = new TStringList();

  printf("目標頻率/週期:%.3fHz/%.6fms, 時間精度:%.6fms, 負載次數:%d\r\n\r\n",
          g_TgtFrequency,
          (1./g_TgtFrequency)*1000.,
          g_TimeAccuracy,
          g_LoadingCount);
  g_CursorPos = fnGetCursorPos();

  g_hWaitEvent = fnCreateEvent(false, // false = 自動重置; true = 手動重置
                               true, // false = Event初始,為非激發狀態; true = Event初始,為激發狀態
                               fnGetEventName()); // Event物件名稱

  fnCreateThxTimer()->Enabled = true;

  // 紀錄開始時間
  AnsiString lTime = FormatDateTime("HH:MM:SS.zzz", g_Elp.CumulationTime());
  g_LogList->Add(Format("%s, Start", ARRAYOFCONST((lTime.c_str()))));

  DWORD dwRes;
  do {
    // 20ms 的等待時間 (50Hz 刷新率)，視覺上會非常絲滑，數據跳動感極強
    dwRes = WaitForSingleObject(g_hWaitEvent, 20);

    if (dwRes == WAIT_TIMEOUT) {
      // 快速抓取快照 (Snapshot)
      TTimerStats snapshot = const_cast<TTimerStats&>(g_LatestStats);

      // 預算顯示範圍 (固定寬度防止殘影)
      const double lTgtTime = 1000.0/g_TgtFrequency;
      const double lResolution = lTgtTime*g_TimeAccuracy;
      const double lMin = lTgtTime - lResolution;
      const double lMax = lTgtTime + lResolution;

      // 定點更新，不要清屏 (CLS)，這樣才會有「電競儀表板」爽感
      fnSetCursorPos(g_CursorPos);

      printf("==================== HIGH-SPEED MONITORING (%.0f Hz) ====================\n", g_TgtFrequency);
      printf("  [Range]  : %.3f ms ~ %.3f ms (Target: %.3f ms)\n", lMin, lMax, lTgtTime);
      printf("------------------------------------------------------------------------\n");

      AnsiString lTimeStr = FormatDateTime("HH:MM:SS.zzz", snapshot.CumulationTime);
      double lPrecent = (snapshot.DoCount > 0) ? (snapshot.OutCount/(double)snapshot.DoCount) : 0;

      // 使用固定寬度格式化 (%-20s)，防止數據位數變動時產生的抖動
      printf("  ELAPSED  : %-20s | DO_COUNT : %-10d\n", lTimeStr.c_str(), snapshot.DoCount);
      printf("  WORK_FPS : %-20.3f | DELTA    : %-10.6f ms\n", snapshot.CurrentFrequencyHz, snapshot.DeltaTime);
      printf("  ACCURACY : %-19.9f%% | OUT_RANGE: %-10d\n", (1.0 - lPrecent)*100.0, snapshot.OutCount);
      printf("========================================================================\n");
    }
  } while (g_CloseExit == false);

  fnRelease();
  return 0;
}
