//---------------------------------------------------------------------------

#pragma hdrstop

#include "uUtilize.h"
#include "TLHelp32.h"
#include <System.ZLib.hpp>
#include <IdHashMessageDigest.hpp>
#include <inifiles.hpp>
#include <math.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
int __fastcall fnGetObjSN(TObject *Sender);
int __fastcall fnAboutVar(int Var, int dt);
TStringList * __fastcall fnNewStrictDelimiterStringList(wchar_t aDelimiter);
String __fastcall fnGetEventName();
HANDLE __fastcall fnCreateEvent(bool bManualReset, bool bInitState, String Name);
String __fastcall fnGetGUID();
String __fastcall fnGetFileVersion(String FileName);
String __fastcall fnGetExeFileName();
String __fastcall fnGetExeFileDir();
String __fastcall fnGetExeFilePath();
String __fastcall fnGetExeName();
String __fastcall fnGetExeVS();
String __fastcall fnGetErrorCodeMsg(DWORD ErrorCode);
String __fastcall fnGetExceptionCodeMsg(System::PExceptionRecord P);
String __fastcall fnGetExecuteIniFileName();
bool __fastcall fnExecuteIniFileExist();
String __fastcall fnAssertIniParms(const wchar_t * const *szFmt, int aCount);
void __fastcall fnClearList(TStringList *sl);
bool __fastcall fnSplit(String Src, String Delimiter, String &Part1, String &Part2);
String __fastcall fnGetStringMD5(String Str);
String __fastcall fnGetStreamMD5(TStream * aStream);
String __fastcall fnGetFileMD5(String aFileName);
unsigned __fastcall fnCRC32(Byte *aBuf, unsigned aLen);
bool __fastcall fnGetCompressionData(Byte *dest, unsigned int &destLen, Byte *src, unsigned int srcLen, int aLv);
bool __fastcall fnGetUncompressionData(Byte *dest, unsigned int &destLen, Byte *src, unsigned int srcLen);
bool __fastcall fnCreateCompressionData(Byte *&dest, unsigned int &destLen, Byte *src, unsigned int srcLen, int aLv);
bool __fastcall fnCreateUncompressionData(Byte *&dest, unsigned int &destLen, Byte *src, unsigned int srcLen);
void __fastcall fnCompressionStream(TFileStream *Strm, String FileName, int Lv);
TFileStream * __fastcall fnDeCompressionStream(String in, String out);
String __fastcall fnReadIniString(String aFile, String aSect, String aIdent);
TMemIniFile *__fastcall fnNewIni(String aFile);
String __fastcall fnReadIniSect(String aFile, String aSect);
TStringList * __fastcall fnNewListForIniSect(String aFile, String aSect);
TStringList * __fastcall fnNewListForIniValue(String aFile, String aSect);
__int64 __fastcall fnFileSize(String FileName);
int __fastcall fnTryCreateDir(String Dir);
String __fastcall fnCreateDirDef(String aDir, String aDefDir);
TStringList * __fastcall fnNewDriveList();
TStringList * __fastcall fnNewDirectoryList(String Dir);
TStringList * __fastcall fnNewFileList(String Dir, String Wildcard, bool aSize);
void __fastcall fnDeleteDirFile(String Dir, String Wildcard);
void __fastcall fnDeleteDirectory(String aDir);
String __fastcall fnCheckSharedFile(String aSharedName);
bool __fastcall fnCreateChunkFile(String FileName, unsigned int aSize, Byte aValue);
TFileStream * __fastcall fnCreateSharedFileStream(String aFile, int aDataSize);
TStringList * __fastcall fnNewDependOnFiles(String aExt);
int __fastcall fnExeNameCount(String aExeName);
TDateTime __fastcall fnMSecsToDateTime(int aMSecs);
int __fastcall fnDateTimeToMSecs(TDateTime aT);
RECT __fastcall fnRtfToRt(TRectF &Rtf);
//---------------------------------------------------------------------------
int __fastcall fnGetObjSN(TObject *Sender)
{
  int cnlen, onlen;
  TComponent *cmp = dynamic_cast<TComponent*>(Sender);
  if (cmp != NULL) {
    cnlen = cmp->ClassName().Length();
    onlen = cmp->Name.Length();
    return cmp->Name.SubString(cnlen, onlen - cnlen + 1).ToIntDef(-1);
  }
  else {
    return -1;
  }
}
//---------------------------------------------------------------------------
int __fastcall fnAboutVar(int Var, int dt)
{
  TDateTime t = Time();
  srand(t.Val*1000);
  return Var + rand()%dt;
}
//---------------------------------------------------------------------------
TStringList * __fastcall fnNewStrictDelimiterStringList(wchar_t aDelimiter)
{
  // 呼叫者負責 delete
  TStringList *Result = new TStringList();
  Result->Delimiter = aDelimiter;
  Result->StrictDelimiter = true;
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnGetGUID()
{
  GUID Guid;
  CreateGUID(Guid);
  return GUIDToString(Guid);
}
//---------------------------------------------------------------------------
String __fastcall fnGetEventName()
{
  return Format("Event_%s", ARRAYOFCONST((fnGetGUID())));
}
//---------------------------------------------------------------------------
HANDLE __fastcall fnCreateEvent(bool bManualReset, bool bInitState, String Name)
{
  HANDLE Result;
  #ifdef UNICODE
  UnicodeString lName = Name;
  #else
  AnsiString lName = Name;
  #endif
  Result = CreateEvent(NULL, // 安全防護性 NULL表示使用預設
                       bManualReset, // false = 自動重置; true = 手動重置
                       bInitState, // false = Event初始 為非激發狀態; true = Event初始 為激發狀態
                       lName.c_str()); // Event物件名稱
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnGetFileVersion(String FileName)
{
  String Result;
  wchar_t *pBuf;
  VS_FIXEDFILEINFO *pVsInfo;
  unsigned int FileInfoSize = sizeof(VS_FIXEDFILEINFO);
  int VerInfoSize = GetFileVersionInfoSize(FileName.w_str(), NULL);

  if (VerInfoSize != 0) {
    pBuf = new wchar_t[VerInfoSize];
    if (GetFileVersionInfo(FileName.c_str(), 0, VerInfoSize, pBuf)) {
      if (VerQueryValue(pBuf, L"\\", (void**)&pVsInfo, &FileInfoSize)) {
        Result = Format("%d.%d.%d.%d",
                        ARRAYOFCONST(((HIWORD(pVsInfo->dwFileVersionMS)),
                                      (LOWORD(pVsInfo->dwFileVersionMS)),
                                      (HIWORD(pVsInfo->dwFileVersionLS)),
                                      (LOWORD(pVsInfo->dwFileVersionLS)))));
      }
    }
    delete [] pBuf;
  }
  return Result;
}
//-----------------------------------------------------------------------------
String __fastcall fnGetErrorCodeMsg(DWORD ErrorCode)
{
  String Result;
  LPVOID lpMsgBuf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                ErrorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                (wchar_t *)&lpMsgBuf,
                0,
                NULL);

  Result = Format("%s", ARRAYOFCONST(((wchar_t *)lpMsgBuf)));
  LocalFree(lpMsgBuf);
  return Result;
}
//-----------------------------------------------------------------------------------
String __fastcall fnGetExceptionCodeMsg(System::PExceptionRecord P)
{
  String Result;
  LPVOID lpMsgBuf;
  DWORD_PTR dwAddress = 0;
  DWORD_PTR dwCode = 0;
  bool bLoad = false;
  const String ModuleName = "ntdll.dll";

  HMODULE hNtDll = GetModuleHandle(ModuleName.c_str());
  if (hNtDll == NULL) {
    hNtDll = LoadLibrary(ModuleName.c_str());
    bLoad = true;
  }
  if (hNtDll == NULL) {return Result;}

  try {
    if (P->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
      if (P->NumberParameters == 2) {
        dwAddress = P->ExceptionInformation[1];
        if (P->ExceptionInformation[0] == 0) {
          Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000005: Access violation reading 0x%08Ix.", ARRAYOFCONST((dwAddress)));
        }
        else if (P->ExceptionInformation[0] == 8) {
          Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000005: Access violation DEP 0x%08Ix.", ARRAYOFCONST((dwAddress)));
        }
        else {
          Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000005: Access violation writing 0x%08Ix.", ARRAYOFCONST((dwAddress)));
        }
      }
      else {
        Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000005: Access violation", ARRAYOFCONST((dwAddress)));
      }
    }
    else if (P->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
      if (P->NumberParameters == 3) {
        dwAddress = P->ExceptionInformation[1];
        dwCode = P->ExceptionInformation[3];
        if (P->ExceptionInformation[0] == 0) {
          Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000006: Page fault reading 0x%08Ix with code 0x%08Ix.", ARRAYOFCONST((dwAddress, dwCode)));
        }
        else if (P->ExceptionInformation[0] == 8) {
          Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000006: Page fault DEP 0x%08Ix with code 0x%08Ix.", ARRAYOFCONST((dwAddress, dwCode)));
        }
        else {
          Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000006: Page fault writing 0x%08Ix with code 0x%08Ix.", ARRAYOFCONST((dwAddress, dwCode)));
        }
      }
      else {
        Result = Format("Unhandled exception at 0x%08Ix:\r\nCode 0xC0000006: Page fault", ARRAYOFCONST((dwAddress, dwCode)));
      }
    }
    else {
      FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                    hNtDll,
                    P->ExceptionCode,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    (wchar_t *)&lpMsgBuf,
                    0,
                    NULL);
      Result = Format("%s", ARRAYOFCONST(((wchar_t *)lpMsgBuf)));
    }
  }
  __finally {
    LocalFree(lpMsgBuf);
    if (bLoad == true) {
      FreeLibrary(hNtDll);
    }
  }
  return Result;
}
//-----------------------------------------------------------------------------------
String __fastcall fnGetExeFileName()
{
  return ExpandFileName(ParamStr(0));
}
//-----------------------------------------------------------------------------
String __fastcall fnGetExeFileDir()
{
  return ExtractFileDir(fnGetExeFileName());
}
//-----------------------------------------------------------------------------
String __fastcall fnGetExeFilePath()
{
  return ExtractFilePath(fnGetExeFileName());
}
//-----------------------------------------------------------------------------
String __fastcall fnGetExeName()
{
  return ExtractFileName(ChangeFileExt(fnGetExeFileName(), ""));
}
//-----------------------------------------------------------------------------
String __fastcall fnGetExeVS()
{
  return fnGetFileVersion(fnGetExeFileName());
}
//-----------------------------------------------------------------------------
String __fastcall fnGetExecuteIniFileName()
{
  return ChangeFileExt(fnGetExeFileName(), ".ini");
}
//---------------------------------------------------------------------------
bool __fastcall fnExecuteIniFileExist()
{
  return FileExists(fnGetExecuteIniFileName());
}
//---------------------------------------------------------------------------
String __fastcall fnAssertIniParms(const wchar_t * const *szFmt, int aCount)
{
  String Result = "";
  TStringList *sl = NULL;
  String File = fnGetExecuteIniFileName();

  if (!FileExists(File)) {
    // 使用 __try...__finally 確保 sl 一定會被釋放
    __try {
      sl = new TStringList();
      for (int i = 0; i < aCount; i++) {
        sl->Add((String)szFmt[i]);
      }

      // 內層使用另一組 __try/__except 或單純保護邏輯
      __try {
        sl->WriteBOM = false; // 取消 BOM
        sl->SaveToFile(File, TEncoding::UTF8); // 存成 UTF8

        Result = Format(L"缺少設定檔, 自動產生 %s", ARRAYOFCONST((File)));
      }
      __except(EXCEPTION_EXECUTE_HANDLER) {
        // 捕捉存檔失敗（如權限不足、硬碟滿了）
        // 若需要將錯誤向上拋出，可記錄日誌或修改 Result
        Result = Format(L"無法建立設定檔: %s", ARRAYOFCONST((File)));
      }
    }
    __finally {
      // 確保資源回收，避免 100Hz 頻繁呼叫下的記憶體增長
      if (sl != NULL) {
        delete sl;
        sl = NULL;
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall fnClearList(TStringList *sl)
{
  TObject *obj;
  TStream *stream;
  TComponent *cmp;
  while (sl->Count) {
    obj = sl->Objects[0];
    stream = (dynamic_cast<TStream *>(obj));
    cmp = (dynamic_cast<TComponent *>(obj));
    if (stream) {
      delete stream;
    }
    else if (cmp) {
      delete cmp;
    }
    else {
      if (obj != NULL) {
        delete sl->Objects[0];
      }
    }
    sl->Delete(0);
  }
}
//---------------------------------------------------------------------------
bool __fastcall fnSplit(String Src, String Delimiter, String &Part1, String &Part2)
{
  int srclen = Src.Length();
  int dellen = Delimiter.Length();
  int pos = Src.Pos(Delimiter);
  if (pos > 0) {
    Part1 = Src.SubString(1, pos - 1);
    Part2 = Src.SubString(pos + dellen, srclen - pos - dellen + 1);
    return true;
  }
  else {
    Part1 = Src;
    Part2 = "";
    return false;
  }
}
//---------------------------------------------------------------------------
String __fastcall fnGetStringMD5(String Str)
{
  String Result;
  TIdHashMessageDigest5 *md5 = new TIdHashMessageDigest5();
  Result = md5->HashStringAsHex(Str, IndyTextEncoding_UTF8());
  delete md5;
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnGetStreamMD5(TStream *aStream)
{
  String Result;
  TIdHashMessageDigest5 *md5 = new TIdHashMessageDigest5();
  Result = md5->HashStreamAsHex(aStream);
  delete md5;
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnGetFileMD5(String aFileName)
{
  String Result;
  TFileStream *strm;
  if (FileExists(aFileName) == true) {
    strm = new TFileStream(aFileName, fmOpenRead|fmShareDenyWrite);
    Result = fnGetStreamMD5(strm);
    delete strm;
  }
  return Result;
}
//---------------------------------------------------------------------------
unsigned __fastcall fnCRC32(Byte *aBuf, unsigned aLen)
{
  // POLYNOMIAL 0xEDB88320;     Standard CRC-32 ppolynomial
  unsigned Result;

  // 第一次 crc start 一定要做
  #ifdef _WIN64
  Result = crc32(0L, 0, 0);
  #else
  Result = crc32(0L, 0, 0);
  #endif

  Result = crc32(Result, aBuf, aLen);
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall fnGetCompressionData(Byte *dest, unsigned int &destLen,
    Byte *src, unsigned int srcLen, int aLv)
{
// Lv: range 0 ~ 9
// Z_NO_COMPRESSION         0
// Z_BEST_SPEED             1
// Z_BEST_COMPRESSION       9
// Z_DEFAULT_COMPRESSION  (-1)

  if (srcLen > destLen + 32) {
    return false;
  }
  else {
    if (compress2(dest, destLen, src, srcLen, aLv) == Z_OK) {
      return true;
    }
    else {
      return false;
    }
  }
}
//----------------------------------------------------------------------------
static const int g_CompressMaxLen = 2*1024*1024;
static Byte g_CompressDest[g_CompressMaxLen];
bool __fastcall fnCreateCompressionData(Byte *&dest, unsigned int &destLen,
    Byte *src, unsigned int srcLen, int aLv)
{
  unsigned int ldestLen = g_CompressMaxLen;
  if (fnGetCompressionData(g_CompressDest, ldestLen, src, srcLen, aLv) == true) {
    destLen = ldestLen;
    dest = new Byte[destLen];
    memcpy(dest, g_CompressDest, destLen);
    return true;
  }
  else {
    dest = NULL;
    destLen = 0;
    return false;
  }
}
//----------------------------------------------------------------------------
bool __fastcall fnGetUncompressionData(Byte *dest, unsigned int &destLen,
    Byte *src, unsigned int srcLen)
{
  if (!dest || !src || srcLen == 0) {return false;}

  if (uncompress(dest, destLen, src, srcLen) == Z_OK) {
    return true;
  }
  else {
    return false;
  }
}
//----------------------------------------------------------------------------
static const int g_UncompressMaxLen = 2*1024*1024;
static Byte g_UncompressDest[g_UncompressMaxLen];
bool __fastcall fnCreateUncompressionData(Byte *&dest, unsigned int &destLen,
    Byte *src, unsigned int srcLen)
{
  unsigned int ldestLen = g_UncompressMaxLen;
  if (fnGetUncompressionData(g_UncompressDest, ldestLen, src, srcLen) == true) {
    destLen = ldestLen;
    dest = new Byte[destLen];
    memcpy(dest, g_UncompressDest, destLen);
    return true;
  }
  else {
    dest = NULL;
    destLen = 0;
    return false;
  }
}
//----------------------------------------------------------------------------
void __fastcall fnCompressionStream(TFileStream *Strm, String FileName, int Lv)
{
  TCompressionLevel Level = (TCompressionLevel)Lv;
  TFileStream *ZFileStrmOut = new TFileStream(FileName, fmCreate);
  TZCompressionStream *Cmpr = new TZCompressionStream(Level, ZFileStrmOut);
  Strm->Position = 0;
  // TODO:
  // Cmpr->OnProgress = m_FnPt.OnCmpr;
  Cmpr->CopyFrom(Strm, Strm->Size); // 壓縮
  delete Cmpr;
  delete ZFileStrmOut;
}
//----------------------------------------------------------------------------
TFileStream * __fastcall fnDeCompressionStream(String in, String out)
{
  // 呼叫者負責 delete
  TFileStream *Result, *input;
  TZDecompressionStream *unzip;

  if (FileExists(out) == true && ExtractFileExt(out) == L".log" ) {
    Result = new TFileStream(out, fmOpenRead);
    Result->Position = 0;
  }
  else if (FileExists(in) == true && ExtractFileExt(in) == L".zz") {
    Result = new TFileStream(out, fmCreate);
    input = new TFileStream(in, fmOpenRead);
    unzip = new TZDecompressionStream(input);
    Result->CopyFrom(unzip, 0); // 解壓縮
    Result->Position = 0;
    delete unzip;
    delete input;
  }
  else {
    Result = NULL;
  }
  return Result;
}
//----------------------------------------------------------------------------
TMemIniFile *__fastcall fnNewIni(String aFile)
{
  // 呼叫者負責 delete
  TMemIniFile *Result;
  if (FileExists(aFile)) {
    Result = new TMemIniFile(aFile);
  }
  else {
    Result = NULL;
  }
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnReadIniString(String aFile, String aSect, String aIdent)
{
// return "Value1,Value2"
//
//  [Sect]
//  xxx=Value1
//  yyy=Value2

  String Result;
  TMemIniFile *ini = fnNewIni(aFile);
  if (ini != NULL) {
    Result = ini->ReadString(aSect, aIdent, "");
    delete ini;
  }
  return Result;
}
//---------------------------------------------------------------------------
TStringList * __fastcall fnNewListForIniSect(String aFile, String aSect)
{
// return "Ident1,Ident2"
//
//  [Sect]
//  Ident1=xxx
//  Ident2=yyy
  // 呼叫者負責 delete
  TStringList *Result;
  TMemIniFile *ini = fnNewIni(aFile);
  if (ini != NULL) {
    Result = fnNewStrictDelimiterStringList();
    ini->ReadSection(aSect, Result);
    delete ini;
  }
  else {
    Result = NULL;
  }
  return Result;
}
//---------------------------------------------------------------------------
TStringList * __fastcall fnNewListForIniValue(String aFile, String aSect)
{
// return "Ident1=Value1,Ident1=Value2"
//
// [Sect]
// Ident1=Value1
// Ident2=Value2

  // 呼叫者負責 delete
  TStringList *Result;
  TMemIniFile *ini = fnNewIni(aFile);
  if (ini != NULL) {
    Result = fnNewStrictDelimiterStringList();
    ini->ReadSectionValues(aSect, Result);
    delete ini;
  }
  else {
    Result = NULL;
  }
  return Result;
}
//---------------------------------------------------------------------------
__int64 __fastcall fnFileSize(String FileName)
{
  __int64 Result;
  Int64Rec lRec;
  TWin32FileAttributeData lAttribute;

  if (GetFileAttributesEx(FileName.c_str(), GetFileExInfoStandard, &lAttribute) != 0) {
    lRec.Lo = lAttribute.nFileSizeLow;
    lRec.Hi = lAttribute.nFileSizeHigh;
    memcpy((void *)&Result, (void *)&lRec, sizeof(Int64Rec));
  }
  else {
    Result = -1;
  }

  // TODO: This function has limited capabilities and is not recommended.
  /*
  unsigned int FileHandle;
  try {
    FileHandle = FileOpen(FileName, fmOpenRead);
    if (FileHandle != 0xFFFFFFFF) {
      Result = FileSeek(FileHandle, (__int64)0, 2);
    }
    else {
      Result = -1;
    }
    FileSeek(FileHandle, 0, 0);
    FileClose(FileHandle);
  }
  catch (...) {
    Result = -1;
  }
  */
  return Result;
}
//--------------------------------------------------------------------------
int __fastcall fnTryCreateDir(String aDir)
{
  // 0: directory create failed, 1: directory create success, 2:directory has existed
  int Result = 0;
  String lDirTmp = ExtractFileDrive(aDir);
  TStringList *sl = fnNewStrictDelimiterStringList('\\');

  if (DirectoryExists(aDir)) {
    Result = 2;
  }
  else {
    sl->DelimitedText = aDir;
    for (int i = 1; i < sl->Count; i++) {
      lDirTmp = lDirTmp + L"\\" + sl->Strings[i];
      if (!DirectoryExists(lDirTmp)) {
        try {
          CreateDir(lDirTmp);
          Result = 1;
        }
        catch (...) {
          Result = 0;
          break;
        }
      }
    }
  }
  delete sl;
  return Result;
}
//-------------------------------------------------------------------------
String __fastcall fnCreateDirDef(String aDir, String aDefDir)
{
  String Result;
  if (fnTryCreateDir(aDir) > 0) {
    Result = aDir;
  }
  else if (fnTryCreateDir(aDefDir) > 0) {
    Result = aDefDir;
  }
  else {
    Result = fnGetExeFileDir() + "\\Log";
    if (fnTryCreateDir(Result) == 0) {
      Result = "";
    }
  }
  return Result;
}
//-------------------------------------------------------------------------
TStringList * __fastcall fnNewDriveList()
{
  // 呼叫者負責 delete
  TStringList *Result = fnNewStrictDelimiterStringList();
  const int MaxDNum = 26;
  String Drive;
  for (int i = 2; i < MaxDNum; i++) {
    Drive = (String)(i + 0x41) + L":";
    if (DirectoryExists(Drive)) {
      Result->Add(Drive);
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
TStringList * __fastcall fnNewDirectoryList(String Dir)
{
  // 呼叫者負責 delete
  TStringList *Result = fnNewStrictDelimiterStringList();
  WIN32_FIND_DATA findFile;
  HANDLE hFind;
  String Name, Path;
  Path = Dir + L"\\*.*";
  hFind = ::FindFirstFile(Path.c_str(), &findFile);
  if (hFind == INVALID_HANDLE_VALUE) {
    FindClose(hFind);
    return Result;
  }

  do {
    if (findFile.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
      Name = (String)findFile.cFileName;
      if (Name != L"." && Name != L"..") {
        Result->Add(Name);
      }
    }
  } while (::FindNextFile(hFind, &findFile));
  FindClose(hFind);
  return Result;
}
//---------------------------------------------------------------------------
TStringList * __fastcall fnNewFileList(String Dir, String Wildcard, bool aSize)
{
  // 呼叫者負責 delete
  TStringList *Result = fnNewStrictDelimiterStringList();
  WIN32_FIND_DATA findFile;
  HANDLE hFind;
  String FileName;
  __int64 lSize, lSizeHigh = MAXDWORD;
  lSizeHigh += 1;

  if (Wildcard == "") {
    Wildcard = L"*.*";
  }

  String Path = Dir + L"\\" + Wildcard;
  hFind = ::FindFirstFile(Path.c_str(), &findFile);

  if (hFind == INVALID_HANDLE_VALUE) {
    FindClose(hFind);
    return Result;
  }
  do {
    FileName = "";
    lSize = -1;
    switch (findFile.dwFileAttributes) {
    case FILE_ATTRIBUTE_DIRECTORY:
      FileName = (String)findFile.cFileName;
      break;

    case FILE_ATTRIBUTE_NORMAL: case FILE_ATTRIBUTE_ARCHIVE: case FILE_ATTRIBUTE_READONLY:
      FileName = (String)findFile.cFileName;
      lSize = (findFile.nFileSizeHigh*(lSizeHigh) + findFile.nFileSizeLow);
      break;

    case 8224: // 8224 = 0X00002020 = 0x20 + 0x2000
               //      =             FILE_ATTRIBUTE_ARCHIVE  + FILE_ATTRIBUTE_NOT_CONTENT_INDEXED

      FileName = (String)findFile.cFileName;
      lSize = (findFile.nFileSizeHigh*(lSizeHigh) + findFile.nFileSizeLow);
      break;

    default:;
    }

    if (FileName != "") {
      if (aSize == true) {
        Result->Add(Format("%s=%d", ARRAYOFCONST((FileName, lSize))));
      }
      else {
        Result->Add(FileName);
      }
    }
  } while (::FindNextFile(hFind, &findFile));
  FindClose(hFind);
  return Result;
}
//-----------------------------------------------------------------------------
void __fastcall fnDeleteDirFile(String Dir, String Wildcard)
{
  WIN32_FIND_DATA findFile;
  HANDLE hFind;
  String FileName, Path;
  Path = Format("%s\\%s", ARRAYOFCONST((Dir, Wildcard)));
  hFind = ::FindFirstFile(Path.c_str(), &findFile);

  if (hFind == INVALID_HANDLE_VALUE){
    FindClose(hFind);
    return ;
  }

  do {
    if (findFile.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
      if (findFile.cFileName[0] == '.') {
        continue;
      }
    }
    else {
      FileName = Format("%s\\%s", ARRAYOFCONST((Dir, StrPas(findFile.cFileName))));
      DeleteFile(FileName);
    }
  } while (::FindNextFile(hFind, &findFile));
  FindClose(hFind);
}
//-----------------------------------------------------------------------------
void __fastcall fnDeleteDirectory(String aDir)
{
  TSearchRec SearchRec;
  String FileName, lDir;
  lDir = aDir;

  if (!DirectoryExists(aDir)) {
    return;
  }
  if (lDir.SubString(lDir.Length(), 1) != L"\\") {
    lDir = lDir + "\\";
  }

  if (FindFirst(lDir + L"*.*", faAnyFile, SearchRec) == 0) {
    do {
      if (SearchRec.Attr == faDirectory) {
        if ((SearchRec.Name != L".") && (SearchRec.Name != L"..")) {
          fnDeleteDirectory(lDir + SearchRec.Name);
        }
      }
      else {
        FileSetAttr(lDir + SearchRec.Name, faArchive);
        DeleteFile(lDir + SearchRec.Name);
      }
    } while (FindNext(SearchRec) == 0);
    FindClose(SearchRec);
  }
  RemoveDir(lDir);
}
//-----------------------------------------------------------------------------
bool __fastcall fnCreateChunkFile(String FileName, unsigned int aSize, Byte aValue)
{
  bool Result = false;
  TFileStream *fm = NULL;
  Byte *lBuffer = NULL;

  // 統一使用 SEH (__try...__finally)
  __try {
    __try {
      // 1. 建立檔案流
      fm = new TFileStream(FileName, fmCreate);
      fm->Position = 0;

      if (aSize > 0) {
        // 2. 分配記憶體緩衝區
        lBuffer = new Byte[aSize];

        // 3. 填充資料並寫入
        memset(lBuffer, aValue, aSize);
        fm->WriteBuffer(lBuffer, aSize);
      }
      Result = true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
      // 捕捉例如磁碟空間不足、權限不足等異常
      Result = false;
    }
  }
  __finally {
    // 4. 資源釋放 (RAII 精神)
    // 即使分配過程中失敗，這裡也能安全處理 NULL 指標
    if (lBuffer != NULL) {
      delete[] lBuffer;
      lBuffer = NULL;
    }
    if (fm != NULL) {
      delete fm;
      fm = NULL;
    }
  }
  return Result;
}
//-----------------------------------------------------------------------------
String __fastcall fnCheckSharedFile(String aSharedName)
{
  String Result;
  String lDir = ExtractFileDir(aSharedName);
  String lName = ExtractFileName(aSharedName);
  String lExt = ExtractFileExt(aSharedName);
  if (lExt != "") {
    if (FileExists(aSharedName) == true) {
      Result = aSharedName;
    }
    else {
      if (DirectoryExists(lDir) == true) {
        Result = lDir + "\\" + lName;
      }
      else {
        Result = fnGetExeFilePath() + lName;
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
String __fastcall fnModifySharedFileName(String aFileName)
{
  // 回傳 合適的檔名
  String Result;
  String lDir = ExtractFileDir(aFileName);
  String lName = ExtractFileName(aFileName);
  if (FileExists(aFileName) == false) {
    if (DirectoryExists(lDir)) {
      Result = lDir + "\\" + lName;
    }
    else {
      Result = fnGetExeFilePath() + "SharedFile.dat";
    }
  }
  else {
    Result = aFileName;
  }
  return Result;
}
//---------------------------------------------------------------------------
TFileStream * __fastcall fnCreateSharedFileStream(String aFile, int aDataSize)
{
  // 呼叫者負責 delete
  TFileStream *Result;
  Byte *lBuffer = NULL;
  Byte lValue = 0x00;
  String lFile = fnModifySharedFileName(aFile);

  if (FileExists(lFile) == true) {
    Result = new TFileStream(lFile, fmOpenReadWrite|fmShareDenyNone|fmShareCompat);
    if (fnFileSize(lFile) != aDataSize) {
      Result->Size = aDataSize;
      try {
        lBuffer = new Byte[aDataSize];
        memset(lBuffer, lValue, aDataSize);
        Result->WriteBuffer(lBuffer, aDataSize);
      }
      __finally {
        delete [] lBuffer;
      }
    }
  }
  else {
    fnCreateChunkFile(lFile, aDataSize, lValue);
    Result = new TFileStream(lFile, fmOpenReadWrite|fmShareDenyNone|fmShareCompat);
  }
  return Result;
}
//---------------------------------------------------------------------------
int __fastcall fnExeNameCount(String aExeName)
{
  int Result = 0;
  bool Loop;
  String lProcessExeName, lExeName;
  HANDLE SnapshotHandle;
  PROCESSENTRY32 ProcessEntry32;
  SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  ProcessEntry32.dwSize = sizeof(ProcessEntry32);
  if (aExeName == "") {
    lExeName = ExtractFileName(fnGetExeFileName());
  }
  else {
    lExeName = aExeName;
  }

  Loop = Process32First(SnapshotHandle, &ProcessEntry32);
  while (Loop == true) {
    lProcessExeName = ExtractFileName(ProcessEntry32.szExeFile);
    if (UpperCase(lProcessExeName) == UpperCase(lExeName) ) {
      ++Result;
    }
    Loop = Process32Next(SnapshotHandle, &ProcessEntry32);
  };
  CloseHandle(SnapshotHandle);
  return Result;
}
//---------------------------------------------------------------------------
TStringList * __fastcall fnNewDependOnFiles(String aExt)
{
  // 呼叫者負責 delete
  HANDLE th32handle;
  MODULEENTRY32 procstruct;
  bool finded;
  String sFile;
  TStringList *Result = new TStringList();
  try {
    th32handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (th32handle != INVALID_HANDLE_VALUE) {
      procstruct.dwSize = sizeof(procstruct);
      finded = Module32First(th32handle, &procstruct);
      while (finded) {
        sFile = procstruct.szExePath;
        if (SameText(ExtractFileExt(sFile), aExt)) {
          Result->Add(sFile);
        }
        finded = Module32Next(th32handle, &procstruct);
      }
      Result->Sort();
    }
  }
  __finally {
    CloseHandle(th32handle);
  }
  return Result;
}
//---------------------------------------------------------------------------
TDateTime __fastcall fnMSecsToDateTime(int aMSecs)
{
  TDateTime Result;
  Word lH, lM, lS, lMS;
  lH = aMSecs/(3600*1000);
  lM = (aMSecs/(60*1000))%60;
  lS = (aMSecs/(1000))%60;
  lMS = aMSecs%1000;

  if (TryEncodeTime(lH, lM, lS, lMS, Result) == false) {
    Result.Val = 0;
  }
  return Result;
}
//---------------------------------------------------------------------------
int __fastcall fnDateTimeToMSecs(TDateTime aT)
{
  Word lHour, lMin, lSec, lMSec;
  DecodeTime(aT, lHour, lMin, lSec, lMSec);
  return lMSec + (lSec + lMin*60 + lHour*3600)*1000;
}
//---------------------------------------------------------------------------
RECT __fastcall fnRtfToRt(TRectF &Rtf)
{
  RECT Result;
  Result.left = Rtf.left;
  Result.top = Rtf.top;
  Result.right = Rtf.right;
  Result.bottom = Rtf.bottom;
  return Result;
}
//-----------------------------------------------------------------------------





