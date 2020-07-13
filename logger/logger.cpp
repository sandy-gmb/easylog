// logger.cpp : ���� DLL Ӧ�ó���ĵ���������--
//

#include "logger.h"

#include <sstream>
#include <fstream>
#include <unordered_map>
#include <mutex>

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
#include <iomanip>
#include <stdarg.h>
#include <time.h>

#include <cstdio>

#if defined(_WIN32)
#include <objbase.h>
#elif defined(__linux__)
#include <uuid/uuid.h>
#else
#error "uuid unsupport platform"
#endif

#define GUID_LEN 64


/*********************     ��Ĭ�ϲ���ֵ ��������ز�����ʹ��Ĭ�ϲ���        ***************************************************************/
#define EASY_LOG_DEFAULT_DIR                "log"           /** ��־��Ĭ��Ŀ¼�ļ���  */
#define EASY_LOG_PRINT_LOG                  false           /** Ĭ�ϱ�ʾ���ô�ӡ������̨ */
#define EASY_LOG_DEFAULT_FILE_MAX_SIZE      10              /** Ĭ����־�ļ�����С,��λM */
#define EASY_LOG_DEFAULT_OUTDATEDAY         30              //Ԥʵ�ֹ���Ĭ�Ϲ�����־ʱ�书��
#define EASY_LOG_KEEP_FILE_OPEN             false           //�����Ƿ񱣳���־�ļ���
#define EASY_LOG_DEFAULT_LOG_LEVEL          LOG_TRACE       //Ĭ����־����:TRACE
#define EASY_LOG_DEFAULT_FILE_NAME          "log.txt"       //Ĭ���ļ���
#define EASY_LOG_DEFAULT_LINE_BUFF_SIZE		1024            /** һ�е���󻺳� */

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
    static bool bKeepOpen;                      //�����Ƿ񱣳��ļ���  ��δʹ��
    static std::string dir;                     //��־���Ŀ¼,�ڳ�������Ŀ¼�½���
    static bool bPrint2StdOut;                  //�Ƿ����������̨
    static int fileMaxSize;                     //�ļ�����С
    static int iOutDateDays;                    //��־����ʱ��

    static bool bCoverLog;                      //�Ƿ�׷����־�ļ�
    static bool bFileNameDate;                  //�Ƿ�ʹ�����ڴ����ļ��� ����ʹ��ʱ�䴴���ļ���

    //�ص�����
    static unordered_map<string, function<void(const string&)>> funNormalCallBack;     //���س�����־��Ϣ,�ַ�������������Ϣϵ
    static unordered_map<string, function<void(const string & prifix, LOG_LEVEL, const string&)>> funSpecCallBack; //������ϸ��־��Ϣ,�����Զ��������־��¼

};


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
// date YYYY-MM-DD format ��2019-01-18
// ============================================================
std::string DateStamp()
{
    char str[11];

    // get the time, and convert it to struct tm format
    time_t a = time(0);
    struct tm b;
    localtime_s(&b, &a);//VS���������޸�

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

EasyLog* EasyLog::GetInstance(std::string suffix)
{
    static EasyLog* m_pInstance = new EasyLog();
    return m_pInstance;
}

void EasyLog::WriteLog(LOG_LEVEL level, const char* pLogText, ...)
{
    va_list args;
    char logText[EASY_LOG_DEFAULT_LINE_BUFF_SIZE] = { 0 };
    va_start(args, pLogText);
    vsnprintf(logText, EASY_LOG_DEFAULT_LINE_BUFF_SIZE - 1, pLogText, args);
    WriteLog(logText, level);
}

void EasyLog::WriteLog(std::string logText, LOG_LEVEL level /*= LOG_ERROR*/)
{
    //lock_guard<mutex> lk(m_pimpl->)l
    if (level < m_pimpl->eLevel)
    {//��־���� ���ò���ӡ
        return;
    }
    if (!m_pimpl->isinited && !Init())
    {//TODO:�˴���ʾҲ����Ҫ�޸� ���ϴ�ӡ��־�ᵼ�¿���̨ȫ�Ǵ���Ϣ
        cout << "Log module init failed, please check reason. ";
        return;
    }

    // ����һ��LOG�ַ���
    std::stringstream szLogLine;
    szLogLine << "[" << DateStamp() << "] [" << TimeStamp() << "] [" << LogEnumToString(level) << "] " << logText << std::endl;//�������Ҫ��ĳ�\r\n

    /* ���LOG�ַ��� - �ļ��򿪲��ɹ�������°��ձ�׼��� */
    if (m_pimpl->fileOut.is_open() && Impl::bFileNameDate && !Impl::bCoverLog && !CheckFileSize())
    {
        //����ļ���С �������ָ���ļ���С �������־�ļ�
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
    {//�ļ���ʧ�� ���������̨
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

EasyLog::EasyLog(void)
{
    m_pimpl->dir = EASY_LOG_DEFAULT_DIR;
    m_pimpl->bKeepOpen = EASY_LOG_KEEP_FILE_OPEN;
    m_pimpl->fileMaxSize = EASY_LOG_DEFAULT_FILE_MAX_SIZE;
    m_pimpl->bPrint2StdOut = EASY_LOG_PRINT_LOG;
    m_pimpl->iOutDateDays = EASY_LOG_DEFAULT_OUTDATEDAY;
    m_pimpl->eLevel = EASY_LOG_DEFAULT_LOG_LEVEL;
    m_pimpl->isinited = false;
}

EasyLog::~EasyLog(void)
{
    //WriteLog("------------------ LOG SYSTEM END ------------------ ", EasyLog::LOG_INFO);
    if (m_pimpl->fileOut.is_open())
        m_pimpl->fileOut.close();
}


bool EasyLog::CheckFileSize()
{
    //�����־��С,����ļ���С����10M,��رյ�ǰ�ļ�,���Ե�ǰʱ�����½���һ���ļ�
    if (m_pimpl->fileSize == -1)
    {
        m_pimpl->fileOut.seekp(0, m_pimpl->fileOut.end);
        m_pimpl->fileSize = m_pimpl->fileOut.tellp();
    }
    if (m_pimpl->fileSize >= m_pimpl->fileMaxSize * 1024 * 1024)
    {
        //�ر�ԭ�ļ�
        m_pimpl->fileOut.close();
        //�½��ļ�������־���
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
            ss << m_pimpl->fileNamePrefix << t << "_" << m_pimpl->iIndex << ".txt";
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
    {//�޸���Ŀ¼,����Ҫ
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
        CString strDir(szPath);//���Ҫ������Ŀ¼�ַ���
        //ȷ����'\'��β�Դ������һ��Ŀ¼
        if (strDir.GetAt(strDir.GetLength() - 1) != _T('/'))
        {
            strDir.AppendChar(_T('/'));
        }
        std::vector<CString> vPath;//���ÿһ��Ŀ¼�ַ���
        CString strTemp;//һ����ʱ����,���Ŀ¼�ַ���
        bool bSuccess = false;//�ɹ���־
        //����Ҫ�������ַ���
        for (int i = 0; i < strDir.GetLength(); ++i)
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

        return PathFileExists(filep) == TRUE;
    }
#else//LINUXϵͳ
    DIR* dp;
    if (dp = opendir(filepath.c_str()) == NULL || mkdir(filepath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1)
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

