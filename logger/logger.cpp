// logger.cpp : 定义 DLL 应用程序的导出函数。--
//

#include "logger.h"

#include <string>
#include <list>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <cstdio>
#include <io.h>
#include <iostream>
#include <iomanip>
#include <stdarg.h>
#include <ctime>

#include <direct.h>

#ifdef _WINDOWS
#include <Windows.h>
#include <atlstr.h>
#include <tchar.h>
#include <vector>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#if defined(_WIN32)
#include <objbase.h>
#elif defined(__linux__)
#include <uuid/uuid.h>
#else
#error "uuid unsupport platform"
#endif

#define GUID_LEN 64


/*********************     库默认参数值 不设置相关参数则使用默认参数        ***************************************************************/
#define EASY_LOG_DEFAULT_DIR                "log"           /** 日志的默认目录文件名  */
#define EASY_LOG_PRINT_LOG                  false           /** 默认表示禁用打印到控制台 */
#define EASY_LOG_DEFAULT_FILE_MAX_SIZE      10              /** 默认日志文件最大大小,单位M */
#define EASY_LOG_DEFAULT_OUTDATEDAY         30              //预实现功能默认过期日志时间功能
#define EASY_LOG_KEEP_FILE_OPEN             false           //程序是否保持日志文件打开
#define EASY_LOG_DEFAULT_LOG_LEVEL          LOG_TRACE       //默认日志级别:TRACE
#define EASY_LOG_DEFAULT_FILE_NAME          "log.txt"       //默认文件名
#define EASY_LOG_DEFAULT_LINE_BUFF_SIZE     4096            /** 一行的最大缓冲 */

using namespace std;

class EasyLog::Impl
{
public:
    Impl()
    {
        fileMutex = ::CreateMutex(NULL, FALSE, NULL);
    }

    list<string> CheckOutDatadFiles(const string& rootDir, int days);
    void DeleteOutdatedFiles(const list<string>& filelist);

    static unordered_map<std::string, shared_ptr<EasyLog> > mHandle;        //前缀与句柄的映射
    HANDLE fileMutex;                    //当前前缀的文件锁
    string sPrefix;                     //当前自定义前缀
    /** 写文件 */
    std::ofstream  fileOut;             //输出文件流对象
    std::string fileName;               //当前使用的文件名      标准全路径格式为 ./log/2019-11-18/08_10_22_01.txt
    std::string fileNamePrefix;         //当前使用的文件名前缀  前缀包含自定前缀和首个文件创立的时间
    int iIndex;                         //文件名后缀实质是本次文件的索引 文件大小大于10M后+1

    long long fileSize;                 //当前文件大小

    bool isinited;

    //TODO:由于宏作为编译开关不方便切换使用,每次都需要重新编译 因此将可能会改变的宏修改为变量并添加设置函数
    static LOG_LEVEL eLevel;                    //当前最低输出级别
    static bool bKeepOpen;                      //程序是否保持文件打开  暂未使用
    static std::string dir;                     //日志输出目录,在程序运行目录下建立
    static bool bPrint2StdOut;                  //是否输出到控制台
    static int fileMaxSize;                     //文件最大大小
    static int iOutDateDays;                    //日志过期时间

    static bool bCoverLog;                      //是否每次都覆盖文件 此种情况下 只写一个文件 程序打开时建立或重写此文件
    static bool bFileNameDate;                  //是否使用日期创建文件名 否则使用时间创建文件名

    //回调函数
    static unordered_map<string, TypeLogNormalCallBack> funNormalCallBack;     //返回常规日志信息,字符串包含所有信息系
    static unordered_map<string, TypeLogSpecCallBack> funSpecCallBack; //返回详细日志信息,用于自定义输出日志记录
    static HANDLE chkmutex;

};
unordered_map<std::string, shared_ptr<EasyLog> > EasyLog::Impl::mHandle;
LOG_LEVEL EasyLog::Impl::eLevel = EASY_LOG_DEFAULT_LOG_LEVEL;
bool EasyLog::Impl::bKeepOpen = EASY_LOG_KEEP_FILE_OPEN;
std::string EasyLog::Impl::dir = EASY_LOG_DEFAULT_DIR;
bool EasyLog::Impl::bPrint2StdOut = EASY_LOG_PRINT_LOG;
int EasyLog::Impl::fileMaxSize = EASY_LOG_DEFAULT_FILE_MAX_SIZE;
int EasyLog::Impl::iOutDateDays = EASY_LOG_DEFAULT_OUTDATEDAY;
bool EasyLog::Impl::bCoverLog = false;
bool EasyLog::Impl::bFileNameDate = true;
unordered_map<string, TypeLogNormalCallBack> EasyLog::Impl::funNormalCallBack;
unordered_map<string, TypeLogSpecCallBack> EasyLog::Impl::funSpecCallBack;
HANDLE EasyLog::Impl::chkmutex = CreateMutex(NULL, FALSE, NULL);



std::list<std::string> EasyLog::Impl::CheckOutDatadFiles(const string& rootDir, int days)
{
    std::list<std::string> filepathlist;
    static time_t lastcheck = time(0) - 60 * 60 * 25;
    time_t curt = time(0);
    if (curt - lastcheck < 60 * 60 * 24)//每天检查一次
        return filepathlist;

    //遍历目录
    _finddata_t fileinfo;
    string filters = rootDir + "/*.*";
    long handle = _findfirst(filters.c_str(), &fileinfo);
    if (-1 == handle)
        return filepathlist;
    // get the time, and convert it to struct tm format
    do
    {
        long long dur_sec = curt - fileinfo.time_write;
        string name = fileinfo.name;
        string filep = rootDir + "/" + name;
        if ((fileinfo.attrib & _A_SUBDIR) == 0 && dur_sec > days * 60 * 60 * 24)
        {
            filepathlist.push_back(filep);
        }
        else if ((fileinfo.attrib & _A_SUBDIR) != 0 && name != "." && name != "..")
        {
            auto filel = CheckOutDatadFiles(filep, days);
            if(!filel.empty())
            {
                filepathlist.merge(filel);
                filepathlist.push_back(filep);
            } 
        }

    } while (_findnext(handle, &fileinfo) == 0);
    _findclose(handle);
    lastcheck = curt;
    return filepathlist;
}

void EasyLog::Impl::DeleteOutdatedFiles(const list<std::string>& filelist)
{
    for (auto it = filelist.begin(); it != filelist.end();it++)
    {
        std::string fp =  *it;
        if(0 != remove(fp.c_str()))
        {
            rmdir(fp.c_str());
        }
    }
}

namespace uuid
{
#if defined(_WIN32)
    static std::string generate()
    {
        char buf[GUID_LEN] = { 0 };
        GUID guid;

        if (CoCreateGuid(&guid))
        {
            return std::move(std::string(""));
        }

        sprintf_s(buf,
            "%08X-%04X-%04x-%02X%02X-%02X%02X%02X%02X%02X%02X",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2],
            guid.Data4[3], guid.Data4[4], guid.Data4[5],
            guid.Data4[6], guid.Data4[7]);

        return std::move(std::string(buf));
    }
#elif defined(__linux__)
    static std::string generate()
    {
        char buf[GUID_LEN] = { 0 };

        uuid_t uu;
        uuid_generate(uu);

        int32_t index = 0;
        for (int32_t i = 0; i < 16; i++)
        {
            int32_t len = i < 15 ?
                sprintf(buf + index, "%02X-", uu[i]) :
                sprintf(buf + index, "%02X", uu[i]);
            if (len < 0)
                return std::move(std::string(""));
            index += len;
        }

        return std::move(std::string(buf));
    }
#endif
}


// ============================================================
// time in 24 hours hh_mm_ss format
// ============================================================
std::string TimeStamp()
{
    char str[9];

    // get the time, and convert it to struct tm format
    time_t a = time(0);
    struct tm b;
    localtime_s(&b, &a);

    // print the time to the string
    strftime(str, 9, "%H_%M_%S", &b);

    return str;
}

// ============================================================
// date YYYY-MM-DD format 如2019-01-18
// ============================================================
std::string DateStamp()
{
    char str[11];

    // get the time, and convert it to struct tm format
    time_t a = time(0);
    struct tm b;
    localtime_s(&b, &a);//VS中请自行修改

    // print the time to the string
    strftime(str, 11, "%Y_%m_%d", &b);

    return str;
}

std::string LogEnumToString(LOG_LEVEL l)
{
    static const string  LOG_STRING[] =
    {
        "TRACE",
        "DEBUG",
        "INFO ",
        "WARN ",
        "ERROR",
        "ALARM",
        "FATAL",
    };
    return LOG_STRING[l];
}

shared_ptr<EasyLog> EasyLog::GetInstance(const std::string& prefix)
{
    WaitForSingleObject(Impl::chkmutex, INFINITE);
    if (Impl::mHandle.count(prefix) != 0){
        ReleaseMutex(Impl::chkmutex);
        return Impl::mHandle[prefix];
    }
    shared_ptr<EasyLog> t(new EasyLog(prefix));
    Impl::mHandle.insert(make_pair(prefix, t));
    ReleaseMutex(Impl::chkmutex);
    return t;
}

void EasyLog::WriteLog(LOG_LEVEL level, const char* pLogText, ...)
{
    va_list args;
    char logText[EASY_LOG_DEFAULT_LINE_BUFF_SIZE] = { 0 };
    va_start(args, pLogText);
    vsnprintf(logText, EASY_LOG_DEFAULT_LINE_BUFF_SIZE - 1, pLogText, args);
    va_end(args);
    WriteLog(logText, level);
}

void EasyLog::WriteLog(std::string logText, LOG_LEVEL level /*= LOG_ERROR*/)
{
    DeleteOutdatedFiles();
     //lock_guard<mutex> lk(m_pimpl->)l
    if (level < m_pimpl->eLevel)
    {//日志级别 设置不打印
        return;
    }
    if (!m_pimpl->isinited && !Init())
    {//TODO:此处提示也许需要修改 不断打印日志会导致控制台全是此信息
        cout << "Log module init failed, please check reason. ";
        return;
    }

    // 生成一行LOG字符串
    std::stringstream szLogLine;
    szLogLine << "[" << DateStamp() << "] [" << TimeStamp() << "] [" << LogEnumToString(level) << "] " << logText << std::endl;//如果有需要请改成\r\n

    /* 输出LOG字符串 - 文件打开不成功的情况下按照标准输出 */
    bool isop = m_pimpl->fileOut.is_open(), chkf = CheckFileSize();
    if (isop && Impl::bFileNameDate && !Impl::bCoverLog && chkf)
    {
        //检查文件大小 如果大于指定文件大小 则更换日志文件
        if (m_pimpl->fileSize != -1)
        {
            m_pimpl->fileSize += szLogLine.str().size();
        }
        m_pimpl->fileOut.write(szLogLine.str().c_str(), szLogLine.str().size());
        for (auto it = m_pimpl->funNormalCallBack.begin(); it != m_pimpl->funNormalCallBack.end(); it++)
        {
            it->second(szLogLine.str());
        }
        for (auto it = m_pimpl->funSpecCallBack.begin(); it != m_pimpl->funSpecCallBack.end(); it++)
        {
            it->second(m_pimpl->sPrefix, level, logText);
        }
        m_pimpl->fileOut.flush();

        if (m_pimpl->bPrint2StdOut)
        {
            std::cout << szLogLine.str();
        }
    }
    else
    {//文件打开失败 输出到控制台
        std::cout << szLogLine.str();
    }
}


void EasyLog::SetOutdateDay(int day)
{
    Impl::iOutDateDays = day;
}

void EasyLog::SetCoverMode(bool iscoverywrite)
{
    Impl::bCoverLog = iscoverywrite;
}

string EasyLog::SetCallBack(const TypeLogNormalCallBack& func)
{
    string uid = uuid::generate();
    Impl::funNormalCallBack.insert(make_pair(uid, func));
    return uid;
}

std::string EasyLog::SetCallBack(const TypeLogSpecCallBack& func)
{
    string uid = uuid::generate();
    Impl::funSpecCallBack.insert(make_pair(uid, func));
    return uid;
}

void EasyLog::RemoveCallBack(const std::string& key)
{
    if (Impl::funNormalCallBack.count(key) != 0)
    {
        Impl::funNormalCallBack.erase(Impl::funNormalCallBack.find(key));
    }
    if (Impl::funSpecCallBack.count(key) != 0)
    {
        Impl::funSpecCallBack.erase(Impl::funSpecCallBack.find(key));
    }
}

EasyLog::EasyLog(const std::string& prefix)
    : m_pimpl(new Impl)
{
    m_pimpl->dir = EASY_LOG_DEFAULT_DIR;
    m_pimpl->bKeepOpen = EASY_LOG_KEEP_FILE_OPEN;
    m_pimpl->fileMaxSize = EASY_LOG_DEFAULT_FILE_MAX_SIZE;
    m_pimpl->bPrint2StdOut = EASY_LOG_PRINT_LOG;
    m_pimpl->iOutDateDays = EASY_LOG_DEFAULT_OUTDATEDAY;
    m_pimpl->eLevel = EASY_LOG_DEFAULT_LOG_LEVEL;
    m_pimpl->sPrefix = prefix;
    m_pimpl->iIndex = 0;
    m_pimpl->isinited = false;
}

EasyLog::~EasyLog(void)
{
    //WriteLog("------------------ LOG SYSTEM END ------------------ ", EasyLog::LOG_INFO);
    if (m_pimpl->fileOut.is_open())
        m_pimpl->fileOut.close();
    delete m_pimpl;
}

bool EasyLog::CheckFileSize()
{
    //检查日志大小,如果文件大小大于10M,则关闭当前文件,并以当前时间名新建下一个文件
    if (m_pimpl->fileSize == -1)
    {
        m_pimpl->fileOut.seekp(0, m_pimpl->fileOut.end);
        m_pimpl->fileSize = m_pimpl->fileOut.tellp();
    }
    if (m_pimpl->fileSize >= m_pimpl->fileMaxSize * 1024 * 1024)
    {
        //关闭原文件
        m_pimpl->fileOut.close();
        //新建文件用于日志输出
        string filepath = GenerateFilePath();
        if (!ComfirmFolderExists(filepath))
        {
            return false;
        }
        stringstream ss;
        ss << filepath << "\\" << m_pimpl->fileName;
        m_pimpl->fileSize = 0;
        m_pimpl->fileOut.open(ss.str().c_str(), ios::out | ios::app);
    }
    return true;
}


std::string EasyLog::GenerateFilePath()
{
    std::string filepath;

    stringstream ss;
    if (m_pimpl->bFileNameDate)
    {
        if (m_pimpl->bCoverLog)
        {
            string t = DateStamp();
            ss << m_pimpl->fileNamePrefix << "_" << t << ".txt";
            ss >> m_pimpl->fileName;
            ss.clear(); ss.str("");
            ss << m_pimpl->dir;
            ss >> filepath;
            ss.clear(); ss.str("");
        }
        else
        {
            string t = TimeStamp();
            if (m_pimpl->iIndex != 1)
            {
                t = m_pimpl->fileNamePrefix;
            }
            else
            {
                m_pimpl->fileNamePrefix = m_pimpl->sPrefix + t;
            }
            ss << m_pimpl->dir << "/" << DateStamp();
            ss >> filepath;
            ss.clear(); ss.str("");
            ss << m_pimpl->fileNamePrefix << "_" << m_pimpl->iIndex << ".txt";
            ss >> m_pimpl->fileName;
            ss.clear(); ss.str("");
            m_pimpl->iIndex++;
        }
    }
    else
    {
        m_pimpl->fileName = EASY_LOG_DEFAULT_FILE_NAME;
        filepath = m_pimpl->dir;
    }

    return filepath;
}

void EasyLog::SetLogDir(std::string _dir)
{
    if (_dir != Impl::dir)
    {//修改了目录,则需要
        m_pimpl->iIndex = 1;
        if (m_pimpl->fileOut.is_open())
        {
            m_pimpl->fileOut.close();
        }
        Impl::dir = _dir;
        Init();
    }
}

void EasyLog::SetPrint2StdOut(bool isprint)
{
    Impl::bPrint2StdOut = isprint;
}

void EasyLog::SetFileMaxSize(int size)
{
    Impl::fileMaxSize = size;
}

bool EasyLog::ComfirmFolderExists(std::string filepath)
{
#ifdef _WINDOWS //WINDOWS OS
    CString filep;
    filep.Format(_T("%s"), filepath.c_str());
    filep.Replace(_T("\\"), _T("/"));
    {
        CString szPath = filep;
        CString strDir(szPath);//存放要创建的目录字符串
        //确保以'\'结尾以创建最后一个目录
        if (strDir.GetAt(strDir.GetLength() - 1) != _T('/'))
        {
            strDir.AppendChar(_T('/'));
        }
        std::vector<CString> vPath;//存放每一层目录字符串
        CString strTemp;//一个临时变量,存放目录字符串
        bool bSuccess = false;//成功标志
        //遍历要创建的字符串
        for (int i = 0; i < strDir.GetLength(); ++i)
        {
            if (strDir.GetAt(i) != _T('/'))
            {//如果当前字符不是'/'
                strTemp.AppendChar(strDir.GetAt(i));
            }
            else
            {//如果当前字符是'/'
                vPath.push_back(strTemp);//将当前层的字符串添加到数组中
                strTemp.AppendChar(_T('/'));
            }
        }

        //遍历存放目录的数组,创建每层目录
        std::vector<CString>::const_iterator vIter;
        for (vIter = vPath.begin(); vIter != vPath.end(); vIter++)
        {
            //如果CreateDirectory执行成功,返回true,否则返回false
            bSuccess = CreateDirectory(*vIter, NULL) ? true : false;
        }

        return PathFileExists(filep) == TRUE;
    }
#else//LINUX系统
    DIR* dp;
    if (dp = opendir(filepath.c_str()) == NULL || mkdir(filepath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1)
    {//没有则创建目录
        return false;
    }
    closedir(dp);
#endif
    return true;

}

void EasyLog::DeleteOutdatedFiles()
{
    list<string> filelst;
    filelst = m_pimpl->CheckOutDatadFiles(Impl::dir, Impl::iOutDateDays);
    m_pimpl->DeleteOutdatedFiles(filelst);
}

bool EasyLog::Init()
{
    m_pimpl->iIndex = 1;
    m_pimpl->fileSize = -1;
    string filepath = GenerateFilePath();
    ComfirmFolderExists(filepath);

    stringstream ss;
    ss << filepath << "\\" << m_pimpl->fileName;

    if (m_pimpl->bCoverLog)
    {
        m_pimpl->fileOut.open(ss.str().c_str(), ios::out | ios::binary);
    }
    else {
        m_pimpl->fileOut.open(ss.str().c_str(), ios::out | ios::app | ios::binary);
    }

    m_pimpl->isinited = true;
    return m_pimpl->fileOut.is_open();
}

void EasyLog::SetLogLevel(LOG_LEVEL level)
{
    Impl::eLevel = level;
}
