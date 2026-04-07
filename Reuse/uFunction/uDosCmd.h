//---------------------------------------------------------------------------

#ifndef uDosCmdH
#define uDosCmdH
//---------------------------------------------------------------------------
#include <System.SyncObjs.hpp>
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
typedef void __fastcall (__closure *TDosCmdEvent)(TObject* Sender, String aStr);

class TDosCmd : public TThread
{
private:
  SECURITY_ATTRIBUTES m_sa;
  STARTUPINFO m_si;
  PROCESS_INFORMATION m_pi;
  HANDLE m_hWaitEvent;
  HANDLE m_hReadStdOut, m_hNewStdOut;
  HANDLE m_hWriteStdIn, m_hNewStdIn;
  TDosCmdEvent m_ReadOutEvent, m_ProcessTerminateEvent, m_ExceptionEvent;
  TCriticalSection *m_cs;
  TStringList *m_slCmd, *m_slReadOut;
  int m_SynMode;
  DWORD m_ErrorCode;
  bool m_bBusyTerminate, m_Enabled, m_ProcessAlive;
  unsigned m_Interval;
  String m_CmdLine;
  void __fastcall SetEnabled(bool b);
  DWORD __fastcall NewStdInPipe();
  DWORD __fastcall NewStdOutPipe();
  DWORD __fastcall NewProcess();

protected:
  void __fastcall Execute();
  void __fastcall Done();
  void __fastcall Do();
  void __fastcall UpdataSynMode();
  DWORD __fastcall WaitForEvent();

public:
  __fastcall TDosCmd(String aCmdLine);
  __fastcall ~TDosCmd();
  void __fastcall AddCmd(String aCmd);
  __property bool Enabled={read=m_Enabled, write=SetEnabled};
  __property bool ProcessAlive={read=m_ProcessAlive};
  __property unsigned Interval={read=m_Interval, write=m_Interval};
  __property int SynMode={read=m_SynMode, write=m_SynMode};
  __property TDosCmdEvent ReadOutEvent={read=m_ReadOutEvent, write=m_ReadOutEvent, default=NULL};
  __property TDosCmdEvent ExceptionEvent={read=m_ExceptionEvent, write=m_ExceptionEvent, default=NULL};
  __property TDosCmdEvent ProcessTerminateEvent={read=m_ProcessTerminateEvent, write=m_ProcessTerminateEvent, default=NULL};
};

typedef struct _EnumWndFilter
{
  #define _FTPID      0x00000001
  #define _FTParent   0x00000002
  #define _FTRoot     0x00000004
  #define _FTClsName  0x00000008
  #define _FTText     0x00000010
  #define _FTFileName 0x00000020

  DWORD Filter;
  DWORD PID;
  HWND Parent;
  HWND Root;
  String ClsName;
  String Text;
  String FileName;
  _EnumWndFilter() {Filter = 0; Parent = NULL; Root = NULL; PID = 0;}
} EnumWndFilter;

typedef void __fastcall (__closure *TAsyEnumWndEvent)(TObject* Sender, TStringList *aList, String aErrorMsg);
class TAsyEnumWnd : public TThread
{
private:
//...
protected:
  int m_Tag;
  int m_SynMode;
  String m_ErrorMsg;
  TStringList *m_WndList;
  EnumWndFilter m_EnumWndFilter;
  TAsyEnumWndEvent m_AsyEnumWndEvent;
  void __fastcall Done();
  void __fastcall DoSyn();
  void __fastcall Execute()=0;

public:
  __fastcall TAsyEnumWnd(EnumWndFilter aFilter, bool aSuspended=true);
  __fastcall TAsyEnumWnd(bool aSuspended=true);
  __fastcall ~TAsyEnumWnd();
  bool __fastcall Add(HWND hWnd, String aFileName, DWORD aPID);

  __property int Tag={read=m_Tag, write=m_Tag};
  __property int SynMode={read=m_SynMode, write=m_SynMode};
  __property EnumWndFilter WndFilter={read=m_EnumWndFilter};
  __property TAsyEnumWndEvent AsyEnumWndEvent={read=m_AsyEnumWndEvent, write=m_AsyEnumWndEvent, default=NULL};
};

class TAsyEnumWindows : public TAsyEnumWnd
{
private:
protected:
  void __fastcall Execute();
public:
  __fastcall TAsyEnumWindows(EnumWndFilter aFilter, int aSynMode=1);
  __fastcall ~TAsyEnumWindows();
};

class TAsyChildWindows : public TAsyEnumWnd
{
private:
  HWND m_ParentWnd;
protected:
  void __fastcall Execute();
public:
  __fastcall TAsyChildWindows(HWND aParent, EnumWndFilter aFilter, int aSynMode=1);
  __fastcall ~TAsyChildWindows();
};

//---------------------------------------------------------------------------
extern bool __fastcall fnSetAffinityMaskExclusive(UINT64 CPUMask);
extern bool __fastcall fnReleaseAffinityMaskExclusive(UINT64 CPUMask);
extern bool __fastcall fnSuicide();
extern int __fastcall fnGetProcessorCount();
extern String __fastcall fnGetExeFileNameFromPID(DWORD aPID);
extern int __fastcall fnGetPID(String aExeName);
extern int __fastcall fnCheckExeNameCount(String aExeName);
extern bool __fastcall fnKillPID(int aPID);
extern bool __fastcall fnKillExe(String aExeName, bool All=false);
extern bool __fastcall fnKillAllOtherInstances(String aExeName);
extern bool __fastcall fnAdjustProcessPrivilege(HANDLE ProcessHandle, String TokenName);
extern void __fastcall fnFillCPULoadingInfo(TStrings *aList); //要先提升系統權限, 才能查看其他進程信息; fnAdjustProcessPrivilege(GetCurrentProcess(), "SeDebugPrivilege")
extern DWORD __fastcall fnCreateProcess();
extern DWORD __fastcall fnCreateProcess(String aCmLine);
extern bool __fastcall fnGetClsNameAndText(HWND aWnd, String &aClsName, String &aText);



#endif
