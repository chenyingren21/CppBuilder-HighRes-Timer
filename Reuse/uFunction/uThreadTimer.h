//---------------------------------------------------------------------------

#ifndef uThreadTimerH
#define uThreadTimerH
//---------------------------------------------------------------------------
#include "uStopWatch.h"
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
typedef void (__cdecl *pfnTimerCallBack)(double aPrevisDeltaTime);

class TThxTimer : public TThread
{
private:
  #ifdef _WIN64
  std::atomic<bool> m_Enabled;         // 防止開始/停止失效
  std::atomic<bool> m_bBusyTerminate;  // 防止關閉時卡死
  #else
  volatile bool m_Enabled;
  volatile bool m_bBusyTerminate;
  #endif
  int m_SynMode;
  unsigned int m_Interval;
  double m_Frequency, m_FrequencyPrecision;
  __int64 m_WaitTick;

protected:
  bool m_Affinity;
  UINT64 m_CPUMask;
  __int64 m_DoCount, m_NextTargetTick;
  bool m_NeedPrecision;
  bool m_UseMMCSS; //多媒體類計劃程序服務（MMCSS）
  bool m_CPUIsolation;

  DWORD m_ProAudioTaskIndex;
  HANDLE m_ProAudioTask;
  HANDLE m_hWaitEvent;
  TNotifyEvent m_TimerEvent;
  pfnTimerCallBack m_pfnTimer;
  TStopwatchElp m_sw;
  TStopwatch m_swDelay;
  TTargetFrequency m_tf;

  double m_CurrentFrequencyHz, m_CurrentTimerResolution, m_PrevisDeltaTime;
  double m_DefTimerResolution;
  String __fastcall GetUniqueEventName();
  void __fastcall SetEnabled(bool b);
  void __fastcall SetFrequency(double aFrequency);
  double __fastcall GetTimerResolution();
  bool __fastcall SetTimerResolution(double aInterval);
  double __fastcall GetFrequency();
  DWORD_PTR GetSystemAffinityMask();
  bool __fastcall IncreasePriority();
  UINT64 __fastcall GetCPUMask(int aCpuIdx);
  String m_Tag;

  void __fastcall Execute()=0;
  void __fastcall Done();
  void __fastcall Do();

public:
  __fastcall TThxTimer();
  __fastcall ~TThxTimer();
  __property TNotifyEvent TimerEvent={read=m_TimerEvent, write=m_TimerEvent, default=0};
  __property double Frequency={read=GetFrequency, write=SetFrequency};
  __property double FrequencyPrecision={read=m_FrequencyPrecision, write=m_FrequencyPrecision};
  __property int SynMode={read=m_SynMode, write=m_SynMode};
  __property bool Enabled={read=m_Enabled, write=SetEnabled};
  __property __int64 DoCount={read=m_DoCount};
  __property double CurrentFrequencyHz={read=m_CurrentFrequencyHz};
  __property double PrevisDeltaTime={read=m_PrevisDeltaTime};
  __property String Tag={read=m_Tag, write=m_Tag};

  // only for SynMode = 0
  __property pfnTimerCallBack pfnTimer={read=m_pfnTimer, write=m_pfnTimer, default=0};
};

class TThreadTimer : public TThxTimer
{
private:

protected:
  void __fastcall Execute();

public:
  __fastcall TThreadTimer();
  __fastcall TThreadTimer(UINT64 aCPUMask);
  __fastcall TThreadTimer(int aCPUIndex);
  __fastcall ~TThreadTimer();
};

class TAccuracyTimer : public TThxTimer
{
private:
  void __fastcall DoWork();
  void __fastcall InitHighPrecision();
protected:
  void __fastcall Execute();

public:
  __fastcall TAccuracyTimer();
  __fastcall TAccuracyTimer(UINT64 aCPUMask);
  __fastcall TAccuracyTimer(int aCPUIndex);
  __fastcall ~TAccuracyTimer();
};

extern int __fastcall fnGetCPUCount();

//---------------------------------------------------------------------------
#endif
