// logger.cpp : ���� DLL Ӧ�ó���ĵ���������--
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

/*********************     ��Ĭ�ϲ���ֵ ��������ز�����ʹ��Ĭ�ϲ���        ***************************************************************/
#define EASY_LOG_DEFAULT_DIR                "log"           /** ��־��Ĭ��Ŀ¼�ļ���  */
#define EASY_LOG_PRINT_LOG                  false           /** Ĭ�ϱ�ʾ���ô�ӡ������̨ */
#define EASY_LOG_DEFAULT_FILE_MAX_SIZE      10              /** Ĭ����־�ļ�����С,��λM */
#define EASY_LOG_DEFAULT_OUTDATEDAY         30              //Ԥʵ�ֹ���Ĭ�Ϲ�����־ʱ�书��
#define EASY_LOG_KEEP_FILE_OPEN             false           //�����Ƿ񱣳���־�ļ���
#define EASY_LOG_DEFAULT_LOG_LEVEL          LOG_TRACE       //Ĭ����־����:TRACE

using namespace std;

class EasyLog::Impl
{
public:
    static unordered_map<std::string, shared_ptr<EasyLog> > mHandle;        //ǰ׺������ӳ��
    mutex fileMutex;                    //��ǰǰ׺���ļ���
    string sPrefix;                     //��ǰ�Զ���ǰ׺
    /** д�ļ� */
    std::ofstream  fileOut;             //����ļ�������
    std::string fileName;               //��ǰʹ�õ��ļ���      ��׼ȫ·����ʽΪ ./log/2019-11-18/08_10_22_01.txt
    std::string fileNamePrefix;         //��ǰʹ�õ��ļ���ǰ׺  ǰ׺�����Զ�ǰ׺���׸��ļ�������ʱ��
    int iIndex;                         //�ļ�����׺ʵ���Ǳ����ļ������� �ļ���С����10M��+1

    long long fileSize;                 //��ǰ�ļ���С

    bool isinited;

    //TODO:���ں���Ϊ���뿪�ز������л�ʹ��,ÿ�ζ���Ҫ���±��� ��˽����ܻ�ı�ĺ��޸�Ϊ������������ú���
    static LOG_LEVEL eLevel;                    //��ǰ����������
    static bool bKeepOpen;                      //�����Ƿ񱣳��ļ���
    static std::string dir;                     //��־���Ŀ¼,�ڳ�������Ŀ¼�½���
    static bool bPrint2StdOut;                  //�Ƿ����������̨
    static int fileMaxSize;                     //�ļ�����С
    static int iOutDateDays;                    //��־����ʱ��
    static bool bCoverLog;                      //�Ƿ�׷����־�ļ�
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
// date YYYY-MM-DD format ��2019-01-18
// ============================================================
std::string DateStamp()
{
    char str[11];

    // get the time, and convert it to struct tm format
    time_t a = time(0);
    struct tm* b = localtime(&a);//VS���������޸�

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
    {//��־���� ���ò���ӡ
        return;
    }
    if(!m_pimpl->isinited && !Init())
    {
        return;
    }

    // ����һ��LOG�ַ���
    std::stringstream szLogLine;
    szLogLine << "[" << DateStamp() <<"] [" << TimeStamp() << "] [" << LOG_STRING[level] << "] " << logText<<std::endl;//�������Ҫ��ĳ�\r\n


#if defined EASY_LOG_DISABLE_LOG && EASY_LOG_DISABLE_LOG == 0
    /* ���LOG�ַ��� - �ļ��򿪲��ɹ�������°��ձ�׼��� */
    if (m_pimpl->fileOut.is_open())
    {
#if defined EASY_LOG_FILE_NAME_DATE && EASY_LOG_FILE_NAME_DATE == 1
#if defined EASY_LOG_FILE_NAME_DATE && EASY_LOG_COVER_LOG != 0
        if(!CheckFileSize())
        {//����Ŀ¼ʧ��,�����־������̨
            std::cout << szLogLine.str();
        }
        
#endif
#endif
        //����ļ���С �������ָ���ļ���С �������־�ļ�
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
    //�����־��С,����ļ���С����10M,��رյ�ǰ�ļ�,���Ե�ǰʱ�����½���һ���ļ�
    if(m_pimpl->fileSize == -1)
    {
        m_pimpl->fileOut.seekp(0, m_pimpl->fileOut.end);
        m_pimpl->fileSize = m_pimpl->fileOut.tellp();
    }
    if(m_pimpl->fileSize >= m_pimpl->fileMaxSize*1024*1024)
    {
        //�ر�ԭ�ļ�
        m_pimpl->fileOut.close();
        //�½��ļ�������־���
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
    {//�޸���Ŀ¼,����Ҫ
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
        CString strDir(szPath);//���Ҫ������Ŀ¼�ַ���
        //ȷ����'\'��β�Դ������һ��Ŀ¼
        if (strDir.GetAt(strDir.GetLength()-1)!=_T('/'))
        {
            strDir.AppendChar(_T('/'));
        }
        std::vector<CString> vPath;//���ÿһ��Ŀ¼�ַ���
        CString strTemp;//һ����ʱ����,���Ŀ¼�ַ���
        bool bSuccess = false;//�ɹ���־
        //����Ҫ�������ַ���
        for (int i=0;i<strDir.GetLength();++i)
        {
            if (strDir.GetAt(i) != _T('/')) 
            {//�����ǰ�ַ�����'/'
                strTemp.AppendChar(strDir.GetAt(i));
            }
            else 
            {//�����ǰ�ַ���'/'
                vPath.push_back(strTemp);//����ǰ����ַ�����ӵ�������
                strTemp.AppendChar(_T('/'));
            }
        }

        //�������Ŀ¼������,����ÿ��Ŀ¼
        std::vector<CString>::const_iterator vIter;
        for (vIter = vPath.begin(); vIter != vPath.end(); vIter++) 
        {
            //���CreateDirectoryִ�гɹ�,����true,���򷵻�false
            bSuccess = CreateDirectory(*vIter, NULL) ? true : false;    
        }

        return PathFileExists(filep)==TRUE;
    }
#else//LINUXϵͳ
    DIR* dp;
    if(dp = opendir(filepath.c_str()) == NULL || mkdir(filepath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO ) == -1)
    {//û���򴴽�Ŀ¼
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
