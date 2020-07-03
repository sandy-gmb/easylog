// logger.cpp : 定义 DLL 应用程序的导出函数。--
//

#include "logger.h"

#include <sstream>

#ifdef _WINDOWS
#include <Windows.h>
#include <atlstr.h>
#include <tchar.h>
#include <vector>
#else
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <iostream>
#include <unordered_map>
#include <functional>
#include <iomanip>
#include <stdarg.h>
#include <time.h>
#include <sstream>
#include <mutex>

/*********************     库默认参数值 不设置相关参数则使用默认参数        ***************************************************************/
#define EASY_LOG_DEFAULT_DIR                "log"           /** 日志的默认目录文件名  */
#define EASY_LOG_PRINT_LOG                  false           /** 默认表示禁用打印到控制台 */
#define EASY_LOG_DEFAULT_FILE_MAX_SIZE      10              /** 默认日志文件最大大小,单位M */
#define EASY_LOG_DEFAULT_OUTDATEDAY         30              //预实现功能默认过期日志时间功能
#define EASY_LOG_KEEP_FILE_OPEN             false           //程序是否保持日志文件打开
#define EASY_LOG_DEFAULT_LOG_LEVEL          LOG_TRACE       //默认日志级别:TRACE

using namespace std;

class EasyLog::Impl
{
public:
    static unordered_map<std::string, shared_ptr<EasyLog> > mHandle;        //前缀与句柄的映射
    mutex fileMutex;                    //当前前缀的文件锁
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
    static bool bKeepOpen;                      //程序是否保持文件打开
    static std::string dir;                     //日志输出目录,在程序运行目录下建立
    static bool bPrint2StdOut;                  //是否输出到控制台
    static int fileMaxSize;                     //文件最大大小
    static int iOutDateDays;                    //日志过期时间
    static bool bCoverLog;                      //是否追加日志文件
};



// ============================================================
// time in 24 hours hh_mm_ss format
// ============================================================
std::string TimeStamp()
{
    char str[9];

    // get the time, and convert it to struct tm format
    time_t a = time(0);
    struct tm* b = localtime(&a);

    // print the time to the string
    strftime(str, 9, "%H_%M_%S", b);

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
    struct tm* b = localtime(&a);//VS中请自行修改

    // print the time to the string
    strftime(str, 11, "%Y_%m_%d", b);

    return str;
}

EasyLog * EasyLog::GetInstance(std::string suffix)
{ 
    static EasyLog* m_pInstance = new EasyLog();
    return m_pInstance; 
}

void EasyLog::WriteLog( LOG_LEVEL level, const char *pLogText, ... )
{
    va_list args;
    char logText[EASY_LOG_LINE_BUFF_SIZE] = { 0 };
    va_start(args, pLogText);
    vsnprintf(logText, EASY_LOG_LINE_BUFF_SIZE - 1, pLogText, args);
    WriteLog(logText, level);
}

void EasyLog::WriteLog( std::string logText, LOG_LEVEL level /*= LOG_ERROR*/ )	
{
    static const char *const LOG_STRING[] =
    {
        "LOG_TRACE",
        "LOG_DEBUG",
        "LOG_INFO ",
        "LOG_WARN ",
        "LOG_ERROR",
        "LOG_ALARM",
        "LOG_FATAL",
    };

    if(level < m_pimpl->eLevel)
    {//日志级别 设置不打印
        return;
    }
    if(!m_pimpl->isinited && !Init())
    {
        return;
    }

    // 生成一行LOG字符串
    std::stringstream szLogLine;
    szLogLine << "[" << DateStamp() <<"] [" << TimeStamp() << "] [" << LOG_STRING[level] << "] " << logText<<std::endl;//如果有需要请改成\r\n


#if defined EASY_LOG_DISABLE_LOG && EASY_LOG_DISABLE_LOG == 0
    /* 输出LOG字符串 - 文件打开不成功的情况下按照标准输出 */
    if (m_pimpl->fileOut.is_open())
    {
#if defined EASY_LOG_FILE_NAME_DATE && EASY_LOG_FILE_NAME_DATE == 1
#if defined EASY_LOG_FILE_NAME_DATE && EASY_LOG_COVER_LOG != 0
        if(!CheckFileSize())
        {//创建目录失败,输出日志到控制台
            std::cout << szLogLine.str();
        }
        
#endif
#endif
        //检查文件大小 如果大于指定文件大小 则更换日志文件
        if(m_pimpl->fileSize != -1)
        {
            m_pimpl->fileSize += szLogLine.str().size();
        }
        m_pimpl->fileOut.write(szLogLine.str().c_str(), szLogLine.str().size());
        m_pimpl->fileOut.flush();
    }
    else
    {
        std::cout << szLogLine.str();
    }
#endif

    if(m_pimpl->bPrint2StdOut)
    {
        std::cout << szLogLine.str();
    }
}

EasyLog::EasyLog( void )
{
    m_pimpl->dir = EASY_LOG_DEFAULT_DIR;
    m_pimpl->bKeepOpen = EASY_LOG_KEEP_FILE_OPEN;
    m_pimpl->fileMaxSize = EASY_LOG_DEFAULT_FILE_MAX_SIZE;
    m_pimpl->bPrint2StdOut = EASY_LOG_PRINT_LOG;
    m_pimpl->iOutDateDays = EASY_LOG_DEFAULT_OUTDATEDAY;
    m_pimpl->eLevel = EASY_LOG_DEFAULT_LOG_LEVEL;
    m_pimpl->isinited = false;
}

EasyLog::~EasyLog( void )
{
    //WriteLog("------------------ LOG SYSTEM END ------------------ ", EasyLog::LOG_INFO);
    if (m_pimpl->fileOut.is_open())
        m_pimpl->fileOut.close();
}


bool EasyLog::CheckFileSize()
{
    //检查日志大小,如果文件大小大于10M,则关闭当前文件,并以当前时间名新建下一个文件
    if(m_pimpl->fileSize == -1)
    {
        m_pimpl->fileOut.seekp(0, m_pimpl->fileOut.end);
        m_pimpl->fileSize = m_pimpl->fileOut.tellp();
    }
    if(m_pimpl->fileSize >= m_pimpl->fileMaxSize*1024*1024)
    {
        //关闭原文件
        m_pimpl->fileOut.close();
        //新建文件用于日志输出
        string filepath = GenerateFilePath();
        if(!ComfirmFolderExists(filepath))
        {
            return false;
        }
        stringstream ss;
        ss<<filepath<<"\\"<< m_pimpl->fileName;
        m_pimpl->fileSize = 0;
        m_pimpl->fileOut.open( ss.str().c_str(), ios::out|ios::app);
    }
    return true;
}


std::string EasyLog::GenerateFilePath()
{
    std::string filepath;

    stringstream ss;
#if defined EASY_LOG_FILE_NAME_DATE && EASY_LOG_FILE_NAME_DATE == 1
#if defined EASY_LOG_COVER_LOG && EASY_LOG_COVER_LOG == 0
    string t = DateStamp();
    ss<< m_pimpl->dir;
    ss>>filepath;
    ss.clear();ss.str("");
    ss<<<<t<<".txt";
    ss>> m_pimpl->fileName;
    ss.clear();ss.str("");
#else
    string t = TimeStamp();
    if(m_pimpl->iIndex != 1)
    {
        t = m_pimpl->fileNamePrefix;
    }
    else
    {
        m_pimpl->fileNamePrefix = m_pimpl->sPrefix+t;
    }
    ss<<dir<<"/"<< DateStamp();
    ss>>filepath;
    ss.clear();ss.str("");
    ss<<t<<"_"<<index<<".txt";
    ss>> m_pimpl->fileName;
    ss.clear();ss.str("");
    m_pimpl->iIndex++;
#endif
#else
    m_pimpl->fileName = EASY_LOG_FILE_NAME;
    filepath = m_pimpl->dir ;
#endif
    return filepath;
}

void EasyLog::SetLogDir( std::string dir )
{
    if(dir != m_pimpl->dir)
    {//修改了目录,则需要
        m_pimpl->iIndex = 1;
        if(m_pimpl->fileOut.is_open())
        {
            m_pimpl->fileOut.close();
        }
        m_pimpl->dir = dir;
       Init();
    }
}

void EasyLog::SetPrint2StdOut( bool isprint )
{
    m_pimpl->bPrint2StdOut = isprint;
}

void EasyLog::SetFileMaxSize( int size )
{
    m_pimpl->fileMaxSize = size;
}

bool EasyLog::ComfirmFolderExists( std::string filepath )
{
#ifdef _WINDOWS //WINDOWS OS
    CString filep;
    filep.Format(_T("%s"), filepath.c_str());
    filep.Replace(_T("\\"), _T("/"));
    {
        CString szPath = filep;
        CString strDir(szPath);//存放要创建的目录字符串
        //确保以'\'结尾以创建最后一个目录
        if (strDir.GetAt(strDir.GetLength()-1)!=_T('/'))
        {
            strDir.AppendChar(_T('/'));
        }
        std::vector<CString> vPath;//存放每一层目录字符串
        CString strTemp;//一个临时变量,存放目录字符串
        bool bSuccess = false;//成功标志
        //遍历要创建的字符串
        for (int i=0;i<strDir.GetLength();++i)
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

        return PathFileExists(filep)==TRUE;
    }
#else//LINUX系统
    DIR* dp;
    if(dp = opendir(filepath.c_str()) == NULL || mkdir(filepath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO ) == -1)
    {//没有则创建目录
        return false;
    }
    closedir(dp);
#endif
    return true;

}

bool EasyLog::Init()
{
    m_pimpl->iIndex = 1;
    m_pimpl->fileSize = -1;
    string filepath = GenerateFilePath();
    ComfirmFolderExists(filepath);

    stringstream ss;
    ss<<filepath<<"\\"<< m_pimpl->fileName;

#if defined EASY_LOG_COVER_LOG && EASY_LOG_COVER_LOG == 0
    m_fileOut.open(ss.str().c_str(), ios::out|ios::binary);
#else
    m_fileOut.open(ss.str().c_str(), ios::out | ios::app|ios::binary);
#endif
    m_pimpl->isinited = true;
    return m_pimpl->fileOut.is_open();
}

void EasyLog::SetLogLevel( LOG_LEVEL level )
{
    m_pimpl->eLevel = level;
}

//void EasyLog::SetOutdateDay(int day)
//{
//    this->outdateday = day;
//}
