//---------------------------------------------------------------------------

#include <System.hpp>
#pragma hdrstop

#include <algorithm>
#include <vector>
#include "uUtilize.h"
#include "uDosCmd.h"
#include "Winapi.TlHelp32.hpp"
#include "Shlobj.h"
#include <pdh.h>
#include <timeapi.h>
#include <mmiscapi2.h>


#pragma package(smart_init)
//---------------------------------------------------------------------------
//
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
//     4.異步,一般使用在與顯示無關的場合
//       如: 檔案複製, 存檔, 搬移, 或是通訊傳輸(indy, COMM Port)
//
//     5.如果"硬要"在異步的情況下, 要在Main Thread顯示,有以下限制(與顯示物件特性有關)
//       5.1 TCanvas 相關的物件, 需要, Lock() ... UnLock();
//       5.2 待補...
//
//  Synchronize and Queue with Parameters
//  Posted on January 30, 2011 by Uwe Raabe
//  In Delphi’s TThread class there is Synchronize to call a method in the context of the GUI thread.
//  This is necessary in case, for example, you want to update a progressbar or a status label in a form, because the VCL is not thread safe.
//  While Synchronize is a blocking method (i.e. the thread code continues when the GUI thread has finished the method call),
//  recent versions of Delphi introduced a non-blocking Queue method.
//  The drawback of using Synchronize is that it is very cumbersome to handle parameters during such a call.
//  The standard solution is to use fields in the TThread descendant holding the parameters during Synchronize,
//  but this won’t work properly with Queue.
//  As Queue is non-blocking there might be multiple calls to Queue waiting to be processed,
//  which often cannot share the same field.
//  Worse, using Queue access to these fields have to be thread safe, too.
//  The queuing thread may just set this field for the next call to Queuewhile a previous Queue call is executed
//  in the main thread just using this field.
//  As this problem doesn’t exist with Synchronize, switching
//  from Synchronize to Queue just has to be done more careful.
//  One way to deal with parameters for Synchronize is the use of Anonymous Methods.
//  There is an overloaded version of Synchronize and Queue taking a TThreadProcedure as parameter:
//---------------------------------------------------------------------------
bool __fastcall fnSetAffinityMaskExclusive(UINT64 CPUMask);
bool __fastcall fnReleaseAffinityMaskExclusive(UINT64 CPUMask);
bool __fastcall fnSuicide();
int __fastcall fnGetProcessorCount();
String __fastcall fnGetExeFileNameFromPID(DWORD aPID);
int __fastcall fnGetPID(String aExeName);
int __fastcall fnCheckExeNameCount(String aExeName);
bool __fastcall fnKillPID(int aPID);
bool __fastcall fnKillExe(String aExeName, bool All);
bool __fastcall fnKillAllOtherInstances(String aExeName);
bool __fastcall fnAdjustProcessPrivilege(HANDLE ProcessHandle, String TokenName);
bool __fastcall fnGetClsNameAndText(HWND aWnd, String &aClsName, String &aText);
DWORD __fastcall fnCreateProcess();
DWORD __fastcall fnCreateProcess(String aCmLine);
void __fastcall fnFillCPULoadingInfo(TStrings *aList);
bool CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
bool CALLBACK EnumChildWindowsProc(HWND hWnd, LPARAM lParam);
bool UpdateAsyEnumWnd(HWND hWnd, TAsyEnumWnd *aAsyEnumWnd);
//---------------------------------------------------------------------------
/**
 * 函式說明：全域 CPU 核心專屬隔離 (CPU Isolation)
 * ---------------------------------------------------------------------------
 * [功能]
 * 遍歷系統中所有進程，將其 Affinity Mask 移除指定的 CPUMask 核心位元。
 * 藉此為本程式的高精度任務（如 TAccuracyTimer）創造一個「無干擾」的執行環境。
 *
 * [參數]
 * CPUMask: 欲隔離的核心位元遮罩 (例如: 0x01 代表 CPU0, 0x08 代表 CPU3)。
 *
 * [使用建議]
 * 1. 啟動任務前呼叫 fnSetAffinityMaskExclusive。
 * 2. 結束任務後務必呼叫 fnReleaseAffinityMaskExclusive 歸還核心給系統。
 * 3. 建議隔離系統中「最後一顆」核心，以避開作業系統預設的 I/O 中斷。
 *
 * [注意事項]
 * 1. 權限：必須具備「系統管理員權限 (Administrator)」才能修改其他進程。
 * 2. 安全：函式內含保護機制，若某進程僅存該核心可用，則不會進行剔除，防止進程崩潰。
 * 3. 效能：不要在核心數過少（如 2 核）的機器上隔離過多核心，以免導致系統卡死。
 * 4. 相容：已針對 Win32/Win64 通用設計，使用 DWORD_PTR 處理 64 位元遮罩。
 * ---------------------------------------------------------------------------
 */
bool __fastcall fnSetAffinityMaskExclusive(UINT64 CPUMask)
{
  bool Result = false;
  HANDLE Snapshot, hProcess;
  TProcessEntry32 Entry;

  // 使用 DWORD_PTR 確保在 64bit 下能完整遮蔽 64 顆核心
  DWORD_PTR ProcAffinityMask = 0, SysAffinityMask = 0;
  DWORD_PTR TargetedCPUMask = (DWORD_PTR)CPUMask;

  Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if ((Snapshot == INVALID_HANDLE_VALUE) || (Snapshot == 0)) {
    return Result;
  }

  __try {
    ZeroMemory(&Entry, sizeof(Entry));
    Entry.dwSize = sizeof(Entry);

    if (Process32First(Snapshot, &Entry)) {
      do {
        if (GetCurrentProcessId() != Entry.th32ProcessID) {
          // 僅請求必要權限，提升在 Win64 下對背景服務的操作成功率
          hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION,
                                 false,
                                 Entry.th32ProcessID);

          if (hProcess) {
            if (GetProcessAffinityMask(hProcess, &ProcAffinityMask, &SysAffinityMask)) {
              // 計算排除後的新遮罩
              DWORD_PTR NewMask = ProcAffinityMask & (~TargetedCPUMask);

              // 邏輯保護：若計算結果不為 0 且確實有變化，才進行設定
              if (NewMask != 0 && NewMask != ProcAffinityMask) {
                if (SetProcessAffinityMask(hProcess, NewMask)) {
                  Result = true;
                }
              }
            }
            CloseHandle(hProcess);
          }
        }
      } while (Process32Next(Snapshot, &Entry));
    }
  }
  __finally {
    CloseHandle(Snapshot);
  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall fnReleaseAffinityMaskExclusive(UINT64 CPUMask)
{
  bool Result = false;
  HANDLE Snapshot, hProcess;
  TProcessEntry32 Entry;
  DWORD_PTR ProcAffinityMask = 0, SysAffinityMask = 0;
  DWORD_PTR TargetedCPUMask = (DWORD_PTR)CPUMask;

  Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if ((Snapshot == INVALID_HANDLE_VALUE) || (Snapshot == 0)) {
    return Result;
  }

  __try {
    ZeroMemory(&Entry, sizeof(Entry));
    Entry.dwSize = sizeof(Entry);
    if (Process32First(Snapshot, &Entry)) {
      do {
        if (GetCurrentProcessId() != Entry.th32ProcessID) {
          hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION, false, Entry.th32ProcessID);
          if (hProcess) {
            if (GetProcessAffinityMask(hProcess, &ProcAffinityMask, &SysAffinityMask)) {
              // 將核心加回進程的可選清單中，但不超過系統原始遮罩
              DWORD_PTR NewMask = (ProcAffinityMask | TargetedCPUMask) & SysAffinityMask;

              if (NewMask != ProcAffinityMask) {
                if (SetProcessAffinityMask(hProcess, NewMask)) {
                  Result = true;
                }
              }
            }
            CloseHandle(hProcess);
          }
        }
      } while (Process32Next(Snapshot, &Entry));
    }
  }
  __finally {
    CloseHandle(Snapshot);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall fnFillCPULoadingInfo(TStrings *aList)
{
  if (!aList) return;

  PDH_HQUERY cpuQuery = NULL;
  PDH_HCOUNTER hCounter = NULL;

  // 開啟 PDH Query
  if (PdhOpenQuery(NULL, NULL, &cpuQuery) != ERROR_SUCCESS) {
    return;
  }

  // 使用英文計數器，避免中文版 Windows 找不到 "\\Processor(*)\\% Processor Time"
  if (PdhAddEnglishCounterW(cpuQuery, L"\\Processor(*)\\% Processor Time", 0, &hCounter) != ERROR_SUCCESS) {
    PdhCloseQuery(cpuQuery);
    return;
  }

  // 第一次收集資料
  PdhCollectQueryData(cpuQuery);
  Sleep(200);                    // 縮短為 200ms，已足夠取得穩定樣本

  // 第二次收集資料（計算差值）
  if (PdhCollectQueryData(cpuQuery) != ERROR_SUCCESS) {
    PdhCloseQuery(cpuQuery);
    return;
  }

  DWORD dwBufferSize = 0;
  DWORD dwItemCount = 0;

  // 第一次呼叫取得所需緩衝區大小
  PdhGetFormattedCounterArray(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);

  if (dwBufferSize > 0) {
    std::vector<BYTE> buffer(dwBufferSize);
    PDH_FMT_COUNTERVALUE_ITEM *pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(&buffer[0]);

    if (PdhGetFormattedCounterArray(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems) == ERROR_SUCCESS) {
      aList->BeginUpdate();
      try {
        aList->Clear();
        for (DWORD i = 0; i < dwItemCount; ++i) {
          String lName = pItems[i].szName;
          double val = pItems[i].FmtValue.doubleValue;
          aList->Add(Format("CPU [%s] : %6.2f %%", ARRAYOFCONST((lName, (Extended)val))));
        }
      }
      __finally {
        aList->EndUpdate();
      }
    }
  }

  PdhCloseQuery(cpuQuery);
}
//---------------------------------------------------------------------------
int __fastcall fnGetProcessorCount()
{
  _SYSTEM_INFO SYSTEM_INFO;
  GetSystemInfo(&SYSTEM_INFO);
  return SYSTEM_INFO.dwNumberOfProcessors;
}
//---------------------------------------------------------------------------
bool __fastcall fnSuicide()
{
  // 自殺函數 小心使用(執行後 執行檔直接消失)
  SHELLEXECUTEINFO sei;
  const int PathMax = 256;
  wchar_t szModule [PathMax],
  szComspec[PathMax],
  szParams [PathMax];
  ZeroMemory(szModule, sizeof(wchar_t)*PathMax);
  ZeroMemory(szComspec, sizeof(wchar_t)*PathMax);
  ZeroMemory(szParams, sizeof(wchar_t)*PathMax);

  // get file path names:
  if ((GetModuleFileNameW(0, szModule, PathMax) != 0) &&
      (GetShortPathNameW(szModule, szModule, PathMax) != 0) &&
      (GetEnvironmentVariableW(L"COMSPEC", szComspec, PathMax) != 0)) {

    // set command shell parameters
    lstrcpyW(szParams, L"/c del ");

    // lstrcpy(szParams,"dir ");
    lstrcatW(szParams, szModule);

    // set struct members
    sei.cbSize = sizeof(sei);
    sei.hwnd = 0;
    sei.lpVerb = L"Open";
    sei.lpFile = szComspec;
    sei.lpParameters = szParams;
    sei.lpDirectory = 0;
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    // invoke command shell
    if (ShellExecuteEx(&sei)) {

      // suppress command shell process until program exits
      SetPriorityClass(sei.hProcess, HIGH_PRIORITY_CLASS); //IDLE_PRIORITY_CLASS);

      SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

      // notify explorer shell of deletion
      SHChangeNotify(SHCNE_DELETE, SHCNF_PATH, szModule, 0);
      return true;
    }
  }
  return false;
}
//---------------------------------------------------------------------------
bool __fastcall fnAdjustProcessPrivilege(HANDLE ProcessHandle, String TokenName)
{
  bool Result = false;
  HANDLE hToken;
  TOKEN_PRIVILEGES TokenPri;
  LUID Luid;

  if (OpenProcessToken(ProcessHandle, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken) == true) { // 注意：添加 TOKEN_QUERY
    if (LookupPrivilegeValue(NULL, TokenName.c_str(), &Luid) == true) {
      TokenPri.PrivilegeCount = 1;
      TokenPri.Privileges[0].Luid = Luid;
      TokenPri.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

      if (AdjustTokenPrivileges(hToken, FALSE, &TokenPri, 0, NULL, NULL) == true) {
      if (GetLastError() == ERROR_SUCCESS) {
        Result = true;
        }
        else if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
          Result = false;
        }
        else {
          Result = false;
        }
      }
    }
    CloseHandle(hToken);
  }
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnGetExeFileNameFromPID(DWORD aPID)
{
  String Result, lMsg;
  DWORD lErrorCode;
  DWORD lPathLen = 256;
  char lszFullPath[256];
  HANDLE lProc;

//  if (fnAdjustProcessPrivilege(GetCurrentProcess(), "SeDebugPrivilege") == true) {
    lProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, aPID);
    if (lProc != NULL) {
      if (QueryFullProcessImageNameA(lProc, 0/*PROCESS_NAME_NATIVE*/, lszFullPath, &lPathLen) == true) {
        Result = Format("%s", ARRAYOFCONST((lszFullPath)));
        if (CloseHandle(lProc) == false){
          lMsg = fnGetErrorCodeMsg(GetLastError());
        }
      }
    }
    else {
      lMsg = fnGetErrorCodeMsg(GetLastError());
    }
//  }
//  else {
//    lMsg = fnGetErrorCodeMsg(GetLastError());
//  }
  return Result;
}
//---------------------------------------------------------------------------
int __fastcall fnGetPID(String aExeName)
{
  int Result = -1;
  bool Loop;
  String ExeName;
  HANDLE SnapshotHandle;
  Winapi::Tlhelp32::PROCESSENTRY32 ProcessEntry32;
  SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  ProcessEntry32.dwSize = sizeof(ProcessEntry32);

  Loop = Process32First(SnapshotHandle, &ProcessEntry32);
  while (Loop == true) {
    ExeName = ExtractFileName(ProcessEntry32.szExeFile);
    if (UpperCase(ExeName) == UpperCase(aExeName) ) {
      Result= (int)ProcessEntry32.th32ProcessID;
      break;
    }
    Loop = Process32Next(SnapshotHandle, &ProcessEntry32);
  };
  CloseHandle(SnapshotHandle);
  return Result;
}
//---------------------------------------------------------------------------
int __fastcall fnCheckExeNameCount(String aExeName)
{
  int Result = 0;
  bool Loop;
  String ExeName;
  HANDLE SnapshotHandle;
  Winapi::Tlhelp32::PROCESSENTRY32 ProcessEntry32;
  SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  ProcessEntry32.dwSize = sizeof(ProcessEntry32);

  Loop = Process32First(SnapshotHandle, &ProcessEntry32);

  while (Loop == true) {
    ExeName = ExtractFileName(ProcessEntry32.szExeFile);
    if (UpperCase(ExeName) == UpperCase(aExeName) ) {
      ++Result;
    }
    Loop = Process32Next(SnapshotHandle, &ProcessEntry32);
  };
  CloseHandle(SnapshotHandle);
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall fnKillPID(int aPID)
{
  bool Result = false;
  HANDLE hProc;

  if (fnAdjustProcessPrivilege(GetCurrentProcess(), "SeDebugPrivilege") == true) {
    hProc = OpenProcess(PROCESS_ALL_ACCESS, false, DWORD(aPID));
    if (hProc != NULL) {
      Result = TerminateProcess(hProc, 0);
      CloseHandle(hProc);
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall fnKillExe(String aExeName, bool All)
{
  int lResultCnt = 0;
  if (All == true) {
    do {
      if (fnKillPID(fnGetPID(aExeName)) == true) {
        ++lResultCnt;
      }
    } while (fnCheckExeNameCount(aExeName) > 0);

    if (lResultCnt > 0 ) {
      return true;
    }
    else {
      return false;
    }
  }
  else {
    return fnKillPID(fnGetPID(aExeName));
  }
}
//---------------------------------------------------------------------------
// 函數目的：終止所有與當前執行檔名相同，但 PID 不同的其他進程實例。
//---------------------------------------------------------------------------
bool __fastcall fnKillAllOtherInstances(String aExeName)
{
  bool Success = false;
  DWORD CurrentPID = GetCurrentProcessId();
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return false;

  try {
    Winapi::Tlhelp32::PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    String Target = UpperCase(aExeName);

    if (Process32First(hSnap, &pe)) {
      do {
        String entryName = UpperCase(ExtractFileName(pe.szExeFile));
        if (entryName == Target && pe.th32ProcessID != CurrentPID) {
          if (fnKillPID((int)pe.th32ProcessID)) {
            Success = true;
          }
        }
      } while (Process32Next(hSnap, &pe));
    }
  }
  __finally {
    CloseHandle(hSnap);
  }

  return Success;
}
//---------------------------------------------------------------------------
DWORD __fastcall fnCreateProcess()
{
  String lExeFileName = ExpandFileName(ParamStr(0));
  String lCmdLine = Format("%s", ARRAYOFCONST((lExeFileName)));
  return fnCreateProcess(lCmdLine);
}
//---------------------------------------------------------------------------
DWORD __fastcall fnCreateProcess(String aCmLine)
{
  //  aCmLine = L"\\..\\Debug\\another.exe a b c d"
  DWORD Result = 0;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));

  // CREATE_NEW_CONSOLE, 新控制台打開子進程
  // CREATE_SUSPENDED, 子進程創建後掛起，直到調用ResumeThread函數
  DWORD CreationFlags = 0;

  bool b = CreateProcess(NULL, // 指向一個NULL結尾的、用來指定可執行模塊的寬字節字符串
                         aCmLine.c_str(), // 命令行字符串
                         NULL, // 指向一個SECURITY_ATTRIBUTES結構體，這個結構體決定是否返回的句柄可以被子進程繼承。
                         NULL, // 如果lpProcessAttributes參數為空（NULL），那麼句柄不能被繼承。<同上>
                         false, // 指示新進程是否從調用進程處繼承了句柄。
                         CreationFlags, // 指定附加的、用來控制優先類和進程的創建的標
                         NULL, // 指向一個新進程的環境塊。如果此參數為空，新進程使用調用進程的環境
                         NULL, // 指定子進程的工作路徑
                         &si, // 決定新進程的主窗體如何顯示的STARTUPINFO結構體
                         &pi); // 接收新進程的識別信息的PROCESS_INFORMATION結構體

  if (b == true) {
     // 下面兩行關閉句柄，解除本進程和新進程的關係，
     // 不然有可能不小心調用TerminateProcess函數關掉子進程
    Result = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall fnGetClsNameAndText(HWND aWnd, String &aClsName, String &aText)
{
  aClsName = L"";
  aText = L"";
  wchar_t lClsName[512] = {0};
  wchar_t lText[512] = {0};
  if (IsWindow(aWnd) == true) {
    GetClassName(aWnd, lClsName, 512);
    GetWindowText(aWnd, lText, 512);
    aClsName = Format("%s", ARRAYOFCONST((lClsName)));
    aText = Format("%s", ARRAYOFCONST((lText)));
    return true;
  }
  else {
    return false;
  }
}
//---------------------------------------------------------------------------
__fastcall TDosCmd::TDosCmd(String aCmdLine)
  : TThread(true)
{
  m_CmdLine = aCmdLine;
  // Busy loop 離開用
  m_bBusyTerminate = false;
  // 同步 flage
  m_SynMode = 0;
  // timer enable
  m_Enabled = true;
  //
  // Automatically delete while terminating.
  // 1. true: 不需要(不能) delete thread object
  //    釋放資源 只需要 呼叫 Terminate(),
  //    離開順序, Execute(離開), OnTerminate, ~destructor
  //    注意: 在這種請況下 不能再用 WaitFor() 去等待資源釋放
  //    不然會進到永遠等待的死結(因為 WaitFor() 離開的時機(Execute(離開)..),
  //    已經過了)
  //
  // 2. false: 釋放資源必需要 delete thread object
  //    delete 後 method 被呼叫順序為
  //    ~destructor, Execute(離開), OnTerminate
  this->FreeOnTerminate = false;
  // wait Event
  m_hWaitEvent = fnCreateEvent(false, // false = 自動重置; true = 手動重置
                               false, // false = Event初始,為非激發狀態; true = Event初始,為激發狀態
                               fnGetEventName()); // Event物件名稱
  m_Interval = 16;
  //
  m_hReadStdOut = NULL;
  m_hNewStdOut = NULL;
  m_hWriteStdIn = NULL;
  m_hNewStdIn = NULL;
  m_ReadOutEvent = NULL;
  m_ProcessTerminateEvent = NULL;
  m_ExceptionEvent = NULL;
  m_slCmd = new TStringList();
  m_slReadOut = new TStringList();
  m_slCmd->StrictDelimiter = true;
  m_slReadOut->StrictDelimiter = true;
  m_cs = new TCriticalSection();
  m_ErrorCode = 0;
}
//---------------------------------------------------------------------------
__fastcall TDosCmd::~TDosCmd()
{
  if (this->Suspended) {
    this->Start();
  }
  // trigger 離開條件:
  this->Terminate();

  // trigger event
  SetEvent(m_hWaitEvent);
  m_bBusyTerminate = true;

  // wait for 離開條件:
  // 1. 等待 Execute() 離開.
  // 2. 如果 有Assign OnTerminate事件, 要等待 OnTerminate事件完成.
  // 3. WaitFor 回傳值, 是 SetReturnValue(int)
  this->WaitFor();

  // release ...
  if (m_pi.hProcess != NULL) {
    if (m_ProcessAlive) {
      TerminateProcess(m_pi.hProcess, 0);   // 只在還活著時強制終止
    }
    CloseHandle(m_pi.hProcess);
    m_pi.hProcess = NULL;
  }

  if (m_pi.hThread != NULL) {
    CloseHandle(m_pi.hThread);
    m_pi.hThread = NULL;
  }

  if (m_hReadStdOut != NULL) { CloseHandle(m_hReadStdOut); m_hReadStdOut = NULL; }
  if (m_hNewStdOut != NULL)  { CloseHandle(m_hNewStdOut);  m_hNewStdOut = NULL; }
  if (m_hWriteStdIn != NULL) { CloseHandle(m_hWriteStdIn); m_hWriteStdIn = NULL; }
  if (m_hNewStdIn != NULL)   { CloseHandle(m_hNewStdIn);   m_hNewStdIn = NULL; }

  if (m_hWaitEvent != NULL) {
    CloseHandle(m_hWaitEvent);
    m_hWaitEvent = NULL;
  }

  delete m_slCmd;
  delete m_slReadOut;
  delete m_cs;

  m_slCmd = NULL;
  m_slReadOut = NULL;
  m_cs = NULL;
}
//---------------------------------------------------------------------------
void __fastcall TDosCmd::SetEnabled(bool b)
{
  m_Enabled = b;
  if (m_Enabled == true) {
    SetEvent(m_hWaitEvent);
  }
}
//---------------------------------------------------------------------------
void __fastcall TDosCmd::Done()
{
  // 處理讀取到的輸出
  if (m_slReadOut->Count > 0) {
    if (m_ReadOutEvent != NULL) {
      m_ReadOutEvent(this, m_slReadOut->Strings[0]);
    }
    m_slReadOut->Delete(0);
  }

  // 錯誤處理
  if (m_ErrorCode != 0) {
    if (m_ExceptionEvent != NULL) {
      m_ExceptionEvent(this, fnGetErrorCodeMsg(m_ErrorCode));
      m_ExceptionEvent = NULL;   // 只觸發一次
    }
  }

  // 子進程結束事件
  if (!m_ProcessAlive) {
    if (m_ProcessTerminateEvent != NULL) {
      m_ProcessTerminateEvent(this, Format("DosCmd: %s Terminate",
                               ARRAYOFCONST((ExtractFileName(m_CmdLine)))));
      m_ProcessTerminateEvent = NULL;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TDosCmd::UpdataSynMode()
{
  switch (m_SynMode ) {
  case 2: this->Queue(Done); break;
  case 3: this->Synchronize(Done); break;
  default: Done();
  }
}
//---------------------------------------------------------------------------
DWORD __fastcall TDosCmd::WaitForEvent()
{
  DWORD Result;
  if (m_Enabled == true) {
    Result = WaitForSingleObject(m_hWaitEvent, (DWORD)m_Interval);
  }
  else {
    Result = WaitForSingleObject(m_hWaitEvent, INFINITE);
  }

  if (this->Terminated) {
    Result = WAIT_ABANDONED;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TDosCmd::Do()
{
  DWORD ReceiveLen = 0, ReadLen = 0, ExitCode = 0, WriteLen = 0;
  const int MaxLen = 4096;
  char Buffer[MaxLen + 1] = {0};

  if (WaitForEvent() != WAIT_TIMEOUT) {
    return;
  }

  // 檢查子進程是否還在執行
  if (!GetExitCodeProcess(m_pi.hProcess, &ExitCode)) {
    m_ErrorCode = GetLastError();
    m_ProcessAlive = false;
    return;
  }

  if (ExitCode != STILL_ACTIVE) {
    m_ProcessAlive = false;
    return;
  }

  // 檢查是否有輸出資料
  if (PeekNamedPipe(m_hReadStdOut, NULL, 0, NULL, &ReceiveLen, NULL)) {
    if (ReceiveLen > 0) {
      if (ReadFile(m_hReadStdOut, Buffer, std::min(ReceiveLen, (DWORD)MaxLen), &ReadLen, NULL)) {
        Buffer[ReadLen] = '\0';
        m_slReadOut->Add(String(Buffer));
      } else {
        m_ErrorCode = GetLastError();
      }
    }
    else {
      // 嘗試寫入命令
      if (m_cs->TryEnter()) {
        __try {
          if (m_slCmd->Count > 0) {
            AnsiString WriteStr = m_slCmd->Strings[0];
            DWORD SendLen = WriteStr.Length();
            if (WriteFile(m_hWriteStdIn, WriteStr.c_str(), SendLen, &WriteLen, NULL)) {
              m_slCmd->Delete(0);
            }
            else {
              m_ErrorCode = GetLastError();
            }
          }
        }
        __finally {
          m_cs->Leave();
        }
      }
    }
  }
  else {
    m_ErrorCode = GetLastError();
  }
}
//---------------------------------------------------------------------------
void __fastcall TDosCmd::AddCmd(String aCmd)
{
  //add CR/LF at the end of line
  String Cmd = aCmd + L"\r\n";

  m_cs->Enter();
  if (m_slCmd->IndexOf(Cmd) == -1) {
    m_slCmd->Add(Cmd);
  }
  m_cs->Leave();
}
//---------------------------------------------------------------------------
DWORD __fastcall TDosCmd::NewStdInPipe()
{
  m_sa.nLength = sizeof(m_sa);
  m_sa.lpSecurityDescriptor = 0;
  m_sa.bInheritHandle = true;
  if (CreatePipe(&m_hNewStdIn, &m_hWriteStdIn, &m_sa, 0) == true) {
    return 0;
  }
  else {
    return GetLastError();
  }
}
//---------------------------------------------------------------------------
DWORD __fastcall TDosCmd::NewStdOutPipe()
{
  m_sa.nLength = sizeof(m_sa);
  m_sa.lpSecurityDescriptor = 0;
  m_sa.bInheritHandle = true;
  if (CreatePipe(&m_hReadStdOut, &m_hNewStdOut, &m_sa, 0) == true) {
    return 0;
  }
  else {
    return GetLastError();
  }
}
//---------------------------------------------------------------------------
DWORD __fastcall TDosCmd::NewProcess()
{
  m_ProcessAlive = false;

  // 建立 Pipe
  DWORD Code;
  Code = NewStdInPipe();
  if (Code != 0) return Code;

  Code = NewStdOutPipe();
  if (Code != 0) return Code;

  // 清空結構
  ZeroMemory(&m_si, sizeof(m_si));
  ZeroMemory(&m_pi, sizeof(m_pi));

  m_si.cb = sizeof(STARTUPINFO);
  m_si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  m_si.hStdOutput = m_hNewStdOut;   // 子進程的輸出
  m_si.hStdError  = m_hNewStdOut;   // 子進程的錯誤輸出
  m_si.hStdInput  = m_hNewStdIn;    // 子進程的輸入
  m_si.wShowWindow = SW_HIDE;

  // 重要旗標
  DWORD CreationFlags = CREATE_NEW_CONSOLE | NORMAL_PRIORITY_CLASS;

  bool success = CreateProcess(
    NULL,                    // 應用程式名稱 (NULL 表示使用命令列)
    m_CmdLine.c_str(),       // 完整命令列
    NULL,                    // 程序安全性屬性
    NULL,                    // 執行緒安全性屬性
    TRUE,                    // 繼承 Handle（必須為 TRUE 才能繼承 Pipe）
    CreationFlags,           // 建立旗標
    NULL,                    // 環境變數 (NULL = 使用父進程環境)
    NULL,                    // 工作目錄 (NULL = 使用父進程目錄)
    &m_si,                   // STARTUPINFO
    &m_pi);                  // PROCESS_INFORMATION

  if (success) {
    m_ProcessAlive = true;

    // 【重要修正】立即關閉子進程端的 Handle
    // 父進程只需要保留 ReadStdOut 和 WriteStdIn
    if (m_hNewStdOut != NULL) {
      CloseHandle(m_hNewStdOut);
      m_hNewStdOut = NULL;
    }
    if (m_hNewStdIn != NULL) {
      CloseHandle(m_hNewStdIn);
      m_hNewStdIn = NULL;
    }

    // 注意：不要在這裡關閉 m_pi.hProcess 和 m_pi.hThread
    // 它們需要在解構式或子進程結束時才關閉

    return 0;
  }
  else {
    m_ErrorCode = GetLastError();

    // 建立失敗時也要清理已建立的 Pipe
    if (m_hNewStdOut != NULL) { CloseHandle(m_hNewStdOut); m_hNewStdOut = NULL; }
    if (m_hNewStdIn  != NULL) { CloseHandle(m_hNewStdIn);  m_hNewStdIn  = NULL; }

    return m_ErrorCode;
  }
}
//---------------------------------------------------------------------------
void __fastcall TDosCmd::Execute()
{
  // wait for return value
  this->SetReturnValue(777);
  m_ErrorCode = NewProcess();
  do {
    if (m_ErrorCode == 0 && m_ProcessAlive == true) {
      Do();
    }

    // thread syn mode
    UpdataSynMode();
  } while (this->Terminated == false && m_bBusyTerminate == false);
}
//---------------------------------------------------------------------------
__fastcall TAsyEnumWnd::TAsyEnumWnd(EnumWndFilter aFilter, bool aSuspended)
  : TThread(aSuspended)
{
  this->FreeOnTerminate = true;
  m_AsyEnumWndEvent = NULL;
  m_EnumWndFilter = aFilter;
  m_WndList = new TStringList();
}
//---------------------------------------------------------------------------
__fastcall TAsyEnumWnd::TAsyEnumWnd(bool aSuspended)
  : TThread(aSuspended)
{
  this->FreeOnTerminate = true;
  m_AsyEnumWndEvent = NULL;
  m_WndList = new TStringList();
}
//---------------------------------------------------------------------------
__fastcall TAsyEnumWnd::~TAsyEnumWnd()
{
  delete m_WndList;
}
//---------------------------------------------------------------------------
bool __fastcall TAsyEnumWnd::Add(HWND hWnd, String aFileName, DWORD aPID)
{
  bool Result = false;
  int idx;
  if (IsWindow(hWnd) == true) {
    idx = m_WndList->IndexOfObject((TObject *)hWnd);
    if (idx == -1) {
      m_WndList->AddObject(Format("%s=%d", ARRAYOFCONST((aFileName, (int)aPID))), (TObject *)hWnd);
      Result = true;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TAsyEnumWnd::Done()
{
  if (m_AsyEnumWndEvent != NULL) {
    m_AsyEnumWndEvent(this, m_WndList, m_ErrorMsg);
  }
}
//---------------------------------------------------------------------------
void __fastcall TAsyEnumWnd::DoSyn()
{
  switch (m_SynMode) {
  case 1: // thread safe
    this->Synchronize(Done);
    break;

  case 2: // thread safe
    this->Queue(Done);
    break;

  default: // not thread safe
    Done();
  }
}
//---------------------------------------------------------------------------
bool UpdateAsyEnumWnd(HWND hWnd, TAsyEnumWnd *aAsyEnumWnd)
{
  DWORD lPID;
  HWND hParentWnd, hRootWnd;
  String lClsName, lText, lFileName;

  GetWindowThreadProcessId(hWnd, &lPID);
  hParentWnd = GetAncestor(hWnd, GA_PARENT);
  hRootWnd = GetAncestor(hWnd, GA_ROOT);
  lFileName = fnGetExeFileNameFromPID(lPID);
  fnGetClsNameAndText(hWnd, lClsName, lText);

  int lFilterCnt = 0;
  DWORD dwMask = 0x00000001;
  DWORD dwFilter = aAsyEnumWnd->WndFilter.Filter;
  EnumWndFilter lWndFilter = aAsyEnumWnd->WndFilter;
  for (int i = 0; i < 32; i++){
    if ((dwFilter&(dwMask << i))) {
      lFilterCnt++;
    }
  }
  const int lFilterTypeCnt = 6;
  int lMeetCnt = 0;
  if (lFilterCnt == 0) {
    aAsyEnumWnd->Add(hWnd, lFileName, lPID);
  }
  else {
    for (int i = 0; i < lFilterTypeCnt; i++){
      if (dwFilter&(dwMask << i)) {
        switch (i) {
        case 0: // PID
          if (lPID == lWndFilter.PID) {
            ++lMeetCnt;
          }
          break;

        case 1: // parent
          if (hParentWnd == lWndFilter.Parent) {
            ++lMeetCnt;
          }
          break;

        case 2: // root
          if (hRootWnd == lWndFilter.Root) {
            ++lMeetCnt;
          }
          break;

        case 3: // class name
          if (lClsName.Pos(lWndFilter.ClsName) > 0) {
            ++lMeetCnt;
          }
          break;

        case 4: // windows text
          if (lText.Pos(lWndFilter.Text) > 0) {
            ++lMeetCnt;
          }
          break;

        case 5: // file name
          if (lFileName.Pos(lWndFilter.FileName) > 0) {
            ++lMeetCnt;
          }
          break;
        default:;
        }
      }
    }
    if (lMeetCnt == lFilterCnt) {
      aAsyEnumWnd->Add(hWnd, lFileName, lPID);
    }
  }
  return true;
}
//---------------------------------------------------------------------------
bool CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
  return UpdateAsyEnumWnd(hWnd, (TAsyEnumWnd *)lParam);
}
//---------------------------------------------------------------------------
__fastcall TAsyEnumWindows::TAsyEnumWindows(EnumWndFilter aFilter, int aSynMode)
  : TAsyEnumWnd(aFilter)
{
  m_SynMode = aSynMode;
}
//---------------------------------------------------------------------------
__fastcall TAsyEnumWindows::~TAsyEnumWindows()
{
  // TODO:
}
//---------------------------------------------------------------------------
void __fastcall TAsyEnumWindows::Execute()
{
  __try {
    EnumWindows((WNDENUMPROC)EnumWindowsProc, (LPARAM)this);
  }
  __finally {
    DoSyn();
  }
}
//---------------------------------------------------------------------------
bool CALLBACK EnumChildWindowsProc(HWND hWnd, LPARAM lParam)
{
  return UpdateAsyEnumWnd(hWnd, (TAsyEnumWnd *)lParam);
}
//---------------------------------------------------------------------------
__fastcall TAsyChildWindows::TAsyChildWindows(HWND aParent, EnumWndFilter aFilter, int aSynMode)
  : TAsyEnumWnd(aFilter)
{
  m_ParentWnd = aParent;
  m_SynMode = aSynMode;
}
//---------------------------------------------------------------------------
__fastcall TAsyChildWindows::~TAsyChildWindows()
{
  // TODO:
}
//---------------------------------------------------------------------------
void __fastcall TAsyChildWindows::Execute()
{
  __try {
    EnumChildWindows(m_ParentWnd, (WNDENUMPROC)EnumChildWindowsProc, (LPARAM)this);
  }
  __finally {
    DoSyn();
  }
}
//---------------------------------------------------------------------------

