/***************************************************************************************************************
 * Copyright © 2020 Guo.Mingbing. All rights reserved.
 * @file Easylog.h
 * @detail 日志库 Guo.Mingbing基于ZhuShuo的EasyLog基础上修改,最终得到当前版本
 *  原地址:https://github.com/zhushuo1992/Easylog.git
 *  支持以下特性:
 *      1. 支持日志级别;
 *      2. 支持流式日志;
 *      3. 支持printf;
 *      4. 支持定义文件大小,文件超过大小后自动切换新文件 Guo.Mingbing 增加
 *      5. 支持设置日志过期时间 (Reserve 暂未实现)
 *      6. 最后封装成动态库,不再是原来的一个头文件
 * @Note:Info 级别不会打印文件名 行号 函数名等信息

 * @Date:2020-4-9 
 * @Version:2.0 
   1. 增加过期日志定义和将条件宏修改为变量并提供接口设置
   2. 增加可自定义前缀用于程序中不同模块等日志可放在不同的文件中

 * @Version:1.0 将原来的头文件封装成动态库,并按照自己的需求修改
 * @Author Guo.Mingbing
 */

#ifndef EASYLOG_H_
#define EASYLOG_H_

#include <string>
#include <sstream>
#include <functional>
#include <memory>

#pragma once

#ifdef WIN32
#else
#   define  sprintf_s   sprintf
#   define  vsnprintf_s vsnprintf//此处因为我用的是mingw，直接用了vsnprintf，请自行判断
#endif

 /** 日志级别定义*/
enum LOG_LEVEL { LOG_TRACE = 0, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };


#include "logger_global.h"

LOGGER_API inline std::string LogEnumToString(LOG_LEVEL l);

typedef std::function< void(const std::string&)> TypeLogNormalCallBack;
typedef std::function< void(const std::string& , LOG_LEVEL, const std::string&)> TypeLogSpecCallBack;

class LOGGER_API EasyLog
{
public:

public:
    /** 单例模式 */
    static std::shared_ptr<EasyLog> GetInstance(const std::string& prefix = "") ;
    virtual ~EasyLog(void);
public:
    /** 写日志操作 */
	void WriteLog(LOG_LEVEL level, const char *pLogText, ...);
	void WriteLog(std::string logText, LOG_LEVEL level = LOG_ERROR);

    /** 设置函数 静态函数 用于统一设置相关配置 */

    /**
    * @brief  :  SetLogDir 设置日志目录
    *
    * @param  :  std::string dir
    * @return :  void
    * @retval :
    */
    void SetLogDir(std::string dir);

    /**
    * @brief  :  SetPrint2StdOut 设置是否同时将日志打印到标准输出
    *
    * @param  :  bool isprint
    * @return :  void
    * @retval :
    */
    static void SetPrint2StdOut(bool isprint);

    /**
    * @brief  :  SetFileMaxSize 设置日志文件大小
    *
    * @param  :  int size 单位M
    * @return :  void
    * @retval :
    */
    static void SetFileMaxSize(int size);

    /**
    * @brief SetLogLevel 设置最小日志级别
    *
    * @param LOG_LEVEL level
    * @return:   void
    */
    static void SetLogLevel(LOG_LEVEL level);

    /**
    * @brief SetOutdateDay 设置日志过期时间
    *
    * @param int day 如果<=0,则认为设置不过期
    * @return:   void
    */
    static void SetOutdateDay(int day);

    /**
     * SetCoverMode
     * 
     * @para: bool iscoverywrite 是否覆盖写入
     * @return:   void
     */
    static void SetCoverMode(bool iscoverywrite);
        
    static std::string SetCallBack(const TypeLogNormalCallBack& func);
    static std::string SetCallBack(const TypeLogSpecCallBack& func);

    static void RemoveCallBack(const std::string& key);

private:
    EasyLog(const std::string& prefix);

    bool Init();

    //检查文件大小
    bool CheckFileSize();

    std::string GenerateFilePath();

    bool ComfirmFolderExists(std::string filepath);

    void DeleteOutdatedFiles();

private:
    class Impl;
    Impl* m_pimpl;
};

#define ELOGGERF(str) EasyLog::GetInstance(str)
#define ELOGGER EasyLog::GetInstance("")

/** 写日志方法 */
#define EWRITE_LOG(LEVEL, FMT, ...) \
{ \
    std::stringstream ss; \
    ss << FMT; \
    if (LEVEL != LOG_INFO) \
    { \
        ss << " (" << __FILE__ << " : " << __FUNCTION__ << " : " << __LINE__ << " )"; \
    } \
    EasyLog::GetInstance("")->WriteLog(LEVEL, ss.str().c_str(), ##__VA_ARGS__); \
}

#define ELOGT( FMT , ... ) EWRITE_LOG(LOG_TRACE, FMT, ##__VA_ARGS__)
#define ELOGD( FMT , ... ) EWRITE_LOG(LOG_DEBUG, FMT, ##__VA_ARGS__)
#define ELOGI( FMT , ... ) EWRITE_LOG(LOG_INFO , FMT, ##__VA_ARGS__)
#define ELOGW( FMT , ... ) EWRITE_LOG(LOG_WARN , FMT, ##__VA_ARGS__)
#define ELOGE( FMT , ... ) EWRITE_LOG(LOG_ERROR, FMT, ##__VA_ARGS__)
#define ELOGA( FMT , ... ) EWRITE_LOG(LOG_ALARM, FMT, ##__VA_ARGS__)
#define ELOGF( FMT , ... ) EWRITE_LOG(LOG_FATAL, FMT, ##__VA_ARGS__)


#endif /* EASYLOG_H_ */
