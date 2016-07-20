/*
 * Copyright (C) 2016 Alticast Corporation. All Rights Reserved.
 *
 * This software is the confidential and proprietary information
 * of Alticast Corporation. You may not use or distribute
 * this software except in compliance with the terms and conditions
 * of any applicable license agreement in writing between Alticast
 * Corporation and you.
 */

/*
    TODO:
        Support Multi-Session
        Support Auto-Complete
*/

// includes header for TCP/IP Telnet Daemon

#ifndef __LOGCONSOLE_H__
#define __LOGCONSOLE_H__

#include <stdio.h>
#include <iostream>

#define DECLARE_MODULE_NAME(x)

#define  LOG_LEVEL_NON  0
#define  LOG_LEVEL_ERR  1
#define  LOG_LEVEL_WRN  2
#define  LOG_LEVEL_MSG  4
#define  LOG_LEVEL_DBG  8
#define  LOG_LEVEL_SYS  16

#define  LOG_LEVEL_ALL  (LOG_LEVEL_ERR | LOG_LEVEL_WRN | LOG_LEVEL_MSG | LOG_LEVEL_DBG | LOG_LEVEL_SYS)

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG__           printConsoleLog
#define LOG_DBG(...)    LOG__(__FILENAME__, __LINE__, LOG_LEVEL_DBG, __VA_ARGS__)
#define LOG_ERR(...)    LOG__(__FILENAME__, __LINE__, LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_MSG(...)    LOG__(__FILENAME__, __LINE__, LOG_LEVEL_MSG, __VA_ARGS__)
#define LOG_WRN(...)    LOG__(__FILENAME__, __LINE__, LOG_LEVEL_WRN, __VA_ARGS__)

#define ConsolePrintf(...)  printConsoleLog(NULL, 0, LOG_LEVEL_SYS, __VA_ARGS__)

typedef int (*fnLogConsoleCallBack)(const int client, const int argc, const char **argv);

int appendCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func);
int printConsoleLog(const char *file, const int line, const int logLevel, const char *format, ...);

namespace consoleUtils {
    template<class Elem, class Traits>
    inline void hex_dump(const void* aData, std::size_t aLength, std::basic_ostream<Elem, Traits>& aStream, std::size_t aWidth = 16)
	{
		const char* const start = static_cast<const char*>(aData);
		const char* const end = start + aLength;
		const char* line = start;
		while (line != end)
		{
			aStream.width(4);
			aStream.fill('0');
			aStream << std::hex << line - start << " : ";
			std::size_t lineLength = std::min(aWidth, static_cast<std::size_t>(end - line));
			for (std::size_t pass = 1; pass <= 2; ++pass)
			{
				for (const char* next = line; next != end && next != line + aWidth; ++next)
				{
					char ch = *next;
					switch(pass)
					{
					case 1:
						aStream << (ch < 32 ? '.' : ch);
						break;
					case 2:
						if (next != line)
							aStream << " ";
						aStream.width(2);
						aStream.fill('0');
						aStream << std::hex << std::uppercase << static_cast<int>(static_cast<unsigned char>(ch));
						break;
					}
				}
				if (pass == 1 && lineLength != aWidth)
					aStream << std::string(aWidth - lineLength, ' ');
				aStream << " ";
			}
			aStream << std::endl;
			line = line + lineLength;
		}
	}
}

namespace LogConsole {
    #define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)
    #define __CLASS_NAME__ className(__PRETTY_FUNCTION__)

    inline std::string methodName(const std::string& prettyFunction)
    {
        size_t colons = prettyFunction.find("::");
        size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
        size_t end = prettyFunction.rfind("(") - begin;

        return prettyFunction.substr(begin,end) + "()";
    }

    inline std::string className(const std::string& prettyFunction)
    {
        size_t colons = prettyFunction.find("::");
        if (colons == std::string::npos)
            return "::";
        size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
        size_t end = colons - begin;

        return prettyFunction.substr(begin,end);
    }




}

#endif // __LOGCONSOLE_H__
