//---------------------------------------------------------------------------

#ifndef uStopWatchH
#define uStopWatchH

#include <System.Diagnostics.hpp>
//---------------------------------------------------------------------------
class TStopwatchElp
{
private:
  TStopwatch m_sw;
  __int64 m_CheckCount;
  static const int RateSnapNum = 16;
  double m_StartTime, m_Cumulation, m_WaitTime, m_Timeout;
  __int64 m_StartTick, m_CumulationTick, m_WaitTick, m_Tickout, m_NextTargetTick;
  double m_RateSnap[RateSnapNum];
  int m_RateSnapIdx;
public:
  TStopwatchElp();
  void __fastcall Reset();
  double __fastcall Span();
  __int64 __fastcall SpanTick();
  double __fastcall Cumulation();
  __int64 __fastcall CumulationTick();
  TDateTime __fastcall SpanTime();
  TDateTime __fastcall CumulationTime();
  bool __fastcall CheckWaitTime(double aTime, double *aRate=NULL);
  bool __fastcall CheckWaitTick(__int64 aTick, double *aRate);
  String __fastcall SpanStr();
  String __fastcall CumulationStr();

  __property double WaitTime = {read=m_WaitTime};
  __property __int64 WaitTick = {read=m_WaitTick};
};

class TTargetFrequency
{
private:
  TStopwatch m_sw;
public:
  TTargetFrequency();
  void __fastcall DelayTime(double aTime);
  void __fastcall DelayTick(__int64 aTick);
  double __fastcall WaitTime(double aCurrentHz, double aTargetHz);
};

class T1msEx : public System::Classes::TComponent
{
private:
  HANDLE m_hTimer;
  HANDLE m_hTimerQueue;
  PVOID m_User;
  DWORD m_Flags;
  DWORD m_DueTime;
  DWORD m_Period;
  DWORD m_PausePeriod;
  WAITORTIMERCALLBACKFUNC m_pfnCB;
  TNotifyEvent m_TimerEvent;
  unsigned int m_TimerResolution;

  bool __fastcall DeleteTimer();
  bool __fastcall SetTimerResolution(unsigned int aTimer);
  bool __fastcall ResetTimerResolution();
  void __fastcall InitSetting(DWORD aPeriod, DWORD aFlags);
public:
  __fastcall T1msEx(WAITORTIMERCALLBACK apfnCB, DWORD aPeriod, DWORD aFlags=WT_EXECUTEDEFAULT, TComponent *aUser=NULL);
  __fastcall T1msEx(DWORD aPeriod, DWORD aFlags, TComponent* Owner=NULL);
  __fastcall virtual ~T1msEx();

  bool __fastcall IsActive();
  bool __fastcall Start(int aDueTime=0);
  bool __fastcall Pause(DWORD aDueTime=0);
  bool __fastcall Resume(DWORD aDueTime=0);
  int __fastcall ChangeFlags(DWORD aFlags, DWORD aDueTime=0);
  int __fastcall ChangePeriod(DWORD aPeriod, DWORD aDueTime=0);
  __property DWORD Period = {read=m_Period};
  __property DWORD DueTime = {read=m_DueTime};
  __property DWORD Flags = {read=m_Flags};
  __property TNotifyEvent TimerEvent = {read=m_TimerEvent,write=m_TimerEvent};
};


#endif
