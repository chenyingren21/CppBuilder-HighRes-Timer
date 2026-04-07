//---------------------------------------------------------------------------

#ifndef uUtilizeH
#define uUtilizeH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
extern int __fastcall fnGetObjSN(TObject *Sender);
extern int __fastcall fnAboutVar(int Var, int dt);
extern TStringList * __fastcall fnNewStrictDelimiterStringList(wchar_t aDelimiter=L',');
extern String __fastcall fnGetGUID();
extern String __fastcall fnGetEventName();
extern HANDLE __fastcall fnCreateEvent(bool bManualReset, bool bInitState, String Name);
extern String __fastcall fnGetFileVersion(String FileName);
extern String __fastcall fnGetExeFileName();
extern String __fastcall fnGetExeFileDir();
extern String __fastcall fnGetExeFilePath();
extern String __fastcall fnGetExeName();
extern String __fastcall fnGetExeVS();
extern String __fastcall fnGetErrorCodeMsg(DWORD ErrorCode);
extern String __fastcall fnGetExceptionCodeMsg(System::PExceptionRecord P);
extern String __fastcall fnGetExecuteIniFileName();
extern bool __fastcall fnExecuteIniFileExist();
extern String __fastcall fnAssertIniParms(const wchar_t * const *szFmt, int aCount);
extern void __fastcall fnClearList(TStringList *sl);
extern bool __fastcall fnSplit(String Src, String Delimiter, String &Part1, String &Part2);
extern String __fastcall fnGetStringMD5(String Str);
extern String __fastcall fnGetStreamMD5(TStream * aStream);
extern String __fastcall fnGetFileMD5(String aFileName);
extern unsigned __fastcall fnCRC32(Byte *aBuf, unsigned aLen);
extern bool __fastcall fnGetCompressionData(Byte *dest, unsigned int &destLen, Byte *src, unsigned int srcLen, int aLv=-1);
extern bool __fastcall fnGetUncompressionData(Byte *dest, unsigned int &destLen, Byte *src, unsigned int srcLen);
extern bool __fastcall fnCreateCompressionData(Byte *&dest, unsigned int &destLen, Byte *src, unsigned int srcLen, int aLv=-1);
extern bool __fastcall fnCreateUncompressionData(Byte *&dest, unsigned int &destLen, Byte *src, unsigned int srcLen);
extern void __fastcall fnCompressionStream(TFileStream *Strm, String FileName, int Lv);
extern TFileStream * __fastcall fnDeCompressionStream(String in, String out);
extern String __fastcall fnReadIniString(String aFile, String aSect, String aIdent);
extern TStringList * __fastcall fnNewListForIniSect(String aFile, String aSect);
extern TStringList * __fastcall fnNewListForIniValue(String aFile, String aSect);
extern __int64 __fastcall fnFileSize(String FileName);
extern int __fastcall fnTryCreateDir(String Dir);
extern String __fastcall fnCreateDirDef(String aDir, String aDefDir="");
extern TStringList * __fastcall fnNewDriveList();
extern TStringList * __fastcall fnNewDirectoryList(String Dir);
extern TStringList * __fastcall fnNewFileList(String Dir, String Wildcard, bool aSize=false);
extern void __fastcall fnDeleteDirFile(String Dir, String Wildcard);
extern void __fastcall fnDeleteDirectory(String aDir);
extern String __fastcall fnCheckSharedFile(String aSharedName);
extern bool __fastcall fnCreateChunkFile(String FileName, unsigned int aSize=0, Byte aValue=0x00);
TFileStream * __fastcall fnCreateSharedFileStream(String aFile, int aDataSize);
extern int __fastcall fnExeNameCount(String aExeName="");
extern TStringList * __fastcall fnNewDependOnFiles(String aExt=".bpl");
extern TDateTime __fastcall fnMSecsToDateTime(int aMSecs);
extern int __fastcall fnDateTimeToMSecs(TDateTime aT);
extern RECT __fastcall fnRtfToRt(TRectF &Rtf);




#endif
