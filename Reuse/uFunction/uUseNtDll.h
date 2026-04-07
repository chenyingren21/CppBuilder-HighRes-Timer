//---------------------------------------------------------------------------

#ifndef uUseNtDllH
#define uUseNtDllH
//---------------------------------------------------------------------------
extern bool fnNtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
extern bool fnNtQueryTimerResolution(PULONG MaximumResolution, PULONG MinimumResolution, PULONG CurrentResolution);

#endif
