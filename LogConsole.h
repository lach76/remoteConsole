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

#define DECLARE_MODULE_NAME(x) static const char* MODULE_VAR = #x;

#define	TLOGLEVEL_DBG	1
#define	TLOGLEVEL_WRN	2
#define	TLOGLEVEL_MSG	4
#define	TLOGLEVEL_ERR	8
#define	TLOGLEVEL_SYS	16
#define	TLOGLEVEL_ALL	(TLOGLEVEL_DBG | TLOGLEVEL_WRN | TLOGLEVEL_MSG | TLOGLEVEL_ERR | TLOGLEVEL_SYS)

#define	__FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

typedef int (*fnLogConsoleCallBack)(const int argc, const char **argv);
#define	__MODULE__		MODULE_VAR

#define	TLOG_		TLOG::print
#define	TLOG_DBG(...)		TLOG_(__MODULE__, __FILENAME__, __LINE__, TLOGLEVEL_DBG, __VA_ARGS__)
#define	TLOG_WRN(...)		TLOG_(__MODULE__, __FILENAME__, __LINE__, TLOGLEVEL_WRN, __VA_ARGS__)
#define	TLOG_MSG(...)		TLOG_(__MODULE__, __FILENAME__, __LINE__, TLOGLEVEL_MSG, __VA_ARGS__)
#define	TLOG_ERR(...)		TLOG_(__MODULE__, __FILENAME__, __LINE__, TLOGLEVEL_ERR, __VA_ARGS__)
#define	TLOG_SYS(...)		TLOG_(__MODULE__, __FILENAME__, __LINE__, TLOGLEVEL_SYS, __VA_ARGS__)
#define	TLOG_CMD(...)		TLOG_(NULL, NULL, 0, TLOGLEVEL_SYS, __VA_ARGS__)

#define	TLOG_ADDCMD		TLOG::appendCommandItem

namespace TLOG {
	void	print(const char *module, const char *file, const int line, const int loglevel, const char *format, ...);
	int		appendCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func);
}

#if 0		//	to be unblocked
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
#endif

#endif // __LOGCONSOLE_H__
