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
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>

#include <iostream>
#include <string>
#include <list>

namespace consoleUtils
{
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

namespace LogConsole
{
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

    char *strstrip(const char *string, const char *chars) {
        char *newstr = (char*)malloc(strlen(string) + 1);
        int counter = 0;

        for (; *string; string++) {
            if (!strchr(chars, *string)) {
                newstr[counter] = *string;
                ++counter;
            }
        }

        newstr[counter] = 0;
        return newstr;
    }

    #define RCVBUFSIZE 512   /* Size of receive buffer */
    #define MAXPENDING 5    /* Maximum outstanding connection requests */

    #define SNDBUFSIZE 512

    #define CMDLEN_GROUP    32
    #define CMDLEN_NAME     32
    #define CMDLEN_HELP     128

    #define COLOR_RED     "\x1b[31m"
    #define COLOR_GREEN   "\x1b[32m"
    #define COLOR_YELLOW  "\x1b[33m"
    #define COLOR_BLUE    "\x1b[34m"
    #define COLOR_MAGENTA "\x1b[35m"
    #define COLOR_CYAN    "\x1b[36m"
    #define COLOR_RESET   "\x1b[0m"

    typedef int (*fnLogConsoleCallBack)(const int client, const int argc, const char **argv);
    typedef struct CommandItem {
        char *cmdGroup;
        char *cmdName;
        char *cmdHelp;
        fnLogConsoleCallBack pfnCmdFunc;//int  (*pfnCmdFunc)(const int client, const int argc, const char **argv);

        struct CommandItem *prev;
        struct CommandItem *next;

        struct CommandItem *children;
    } TCommandItem;

    class SimpleThread {
        private:
            pthread_t simpleThread;
            int retval;
        public:
            SimpleThread();
            ~SimpleThread();
            static void *run_ (void *);
            void start ();
            int join ();
            void setRetval (int r) { retval = r; }

            virtual int run ();
    };

    SimpleThread::SimpleThread () {
        //  TODO:
    }
    SimpleThread::~SimpleThread () {
        //  TODO:
    }

    int SimpleThread::run() { 
        return 0; 
    }

    void *SimpleThread::run_(void *pthis_)
    {
        SimpleThread *pthis = (SimpleThread *)pthis_;
        pthis->setRetval (pthis->run ());
        pthread_exit (NULL);
    }

    void SimpleThread::start() {
        pthread_create (&simpleThread, NULL, SimpleThread::run_, (void *)this);
    }

    int SimpleThread::join() {
        pthread_join (simpleThread, NULL);
        return retval;
    }

    class LogConsole: public SimpleThread
    {
        private:
            int mPortNo;
            int mServerSocket;
            TCommandItem *mCurrentGroup;
            TCommandItem *mCommandItems;
            char *mLastUsedCmdGroup;

            int initConsoleDaemon();
            void waitClientConnection();
            void doCommunicate(int client, int server);

            TCommandItem* findGroupItem(TCommandItem *commandItems, const char *find_str);
            TCommandItem* insertCommandItem(TCommandItem *commandItems, TCommandItem *item);
            TCommandItem* makeCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func);
            void displayCommandGroup(const int client, TCommandItem *cmdItem, const char *cmd);
            TCommandItem* findCommandItem(TCommandItem *baseItems, const char *cmdName);

        public:

            LogConsole(const int portNo = 6001);
            void processRecvCommand(const int client, const char *command);
            int changeGroup(const int client, const char *group);

            int run();

            static LogConsole* getInstance();

            int appendCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func);
            void logArgs(const int client, const char *format, ...);

            int  printCommandHelp(const int client, const char *cmd);
            void runSystemCommand(const int client, const char *cmd);
    };

    LogConsole::LogConsole(const int portNo) {
        mPortNo = portNo;

        mCommandItems = NULL;
        mCurrentGroup = NULL;
        mLastUsedCmdGroup = NULL;

        initConsoleDaemon();
    }

    int LogConsole::run() {
        waitClientConnection();

        return 0;
    }

    LogConsole* LogConsole::getInstance() {
        static LogConsole *_consoleInstance = NULL;
        if (_consoleInstance == NULL) {
            _consoleInstance = new LogConsole();
            _consoleInstance->start();
        }

        return _consoleInstance;
    }

    int LogConsole::initConsoleDaemon() {
        struct sockaddr_in servAddr;

        mServerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mServerSocket < 0) {
            // fail to init socket
            std::cout << "Error : Socket Initial Fail..." << std::endl;
            return -1;
        }

        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servAddr.sin_port = htons(mPortNo);
        if (bind(mServerSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
            std::cout << "Error : Socket Binding Fail..." << std::endl;
            return -1;
        }

        if (listen(mServerSocket, MAXPENDING) < 0) {
            std::cout << "Fail to Listen in Server Side..." << std::endl;
            return -1;
        }

        return 0;
    }

    void LogConsole::waitClientConnection() {
        struct sockaddr_in clieAddr;
        int clieLen;
        int clieSocket;

        while (1) {
            std::cout << "Waiting Connection" << std::endl;
            clieLen = sizeof(clieAddr);
            clieSocket = accept(mServerSocket, (struct sockaddr *) &clieAddr, (socklen_t*)&clieLen);
            if (clieSocket < 0) {
                std::cout << "Error : Fail to connect client..." << std::endl;
                return;
            }

            std::cout << "Connected from " << inet_ntoa(clieAddr.sin_addr) << std::endl;

            // force telnet character mode - 
            // send(clieSocket, "\377\375\042\377\373\001", 6, 0);
            // if you allow multiple connection, just contains below function in SimpleThread
            doCommunicate(clieSocket, mServerSocket);
        }

        std::cout << "Finish Waiting Client" << std::endl;
    }

    void LogConsole::logArgs(const int client, const char *format, ...) {
        va_list ap;
        char    buf[SNDBUFSIZE];
        int     written;

        va_start(ap, format);
        written = vsnprintf(buf, SNDBUFSIZE, format, ap);
        // TODO: Make it beautified.

        va_end(ap);

        send(client, buf, written + 1, 0);
    }

    void LogConsole::doCommunicate(int client, int server) {
        char echoBuffer[RCVBUFSIZE], *pos;
        int  recvMsgSize;

        do {
            logArgs(client, COLOR_YELLOW "taurus:%s$ " COLOR_RESET, mCurrentGroup->cmdGroup);
            recvMsgSize = recv(client, echoBuffer, RCVBUFSIZE, 0);
            echoBuffer[recvMsgSize] = 0;

            if ((pos = strchr(echoBuffer, 0x0d)) != NULL)
                *pos = '\0';
            if ((pos = strchr(echoBuffer, 0x0a)) != NULL)
                *pos = '\0';

            consoleUtils::hex_dump(echoBuffer, recvMsgSize, std::cout);

            processRecvCommand(client, echoBuffer);
        } while (recvMsgSize > 0);

        close(client);    /* Close client socket */
    }

    TCommandItem*   LogConsole::findGroupItem(TCommandItem *commandItems, const char *find_str) {
        TCommandItem *cur;

        cur = commandItems;
        while (cur) {
            if (strcmp(find_str, cur->cmdGroup) == 0)
                return cur;
            cur = cur->next;
        }

        return cur;
    }

    TCommandItem*   LogConsole::insertCommandItem(TCommandItem *commandItems, TCommandItem *item) {
        if (commandItems == NULL) {
            commandItems = item;
        } else {
            commandItems->prev = item;
            item->next = commandItems;
        }

        return item;
    }

    TCommandItem*   LogConsole::makeCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func) {
        TCommandItem *cmdItem;

        cmdItem = (TCommandItem*)calloc(1, sizeof(TCommandItem));

        cmdItem->cmdGroup = strdup(cmdGroup);
        if (cmdName)
            cmdItem->cmdName = strdup(cmdName);
        if (cmdHelp)
            cmdItem->cmdHelp = strdup(cmdHelp);

        cmdItem->pfnCmdFunc = func;

        return cmdItem;
    }

    // Support 1-Depth Command Line.
    int LogConsole::appendCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func) {
        TCommandItem *cmdItem, *groupCmdItem;

        if (cmdGroup == NULL) cmdGroup = mLastUsedCmdGroup;
        if (func == NULL || cmdName == NULL || cmdGroup == NULL) {
            // TODO : Add Error Message
            return -1;
        }

        cmdItem = makeCommandItem(cmdGroup, cmdName, cmdHelp, func);

        mLastUsedCmdGroup = cmdItem->cmdGroup;
        groupCmdItem = findGroupItem(mCommandItems, cmdItem->cmdGroup);
        if (groupCmdItem == NULL) {
            groupCmdItem = makeCommandItem(cmdGroup, NULL, NULL, NULL);
            mCommandItems = insertCommandItem(mCommandItems, groupCmdItem);
        }

        groupCmdItem->children = insertCommandItem(groupCmdItem->children, cmdItem);

        if (mCurrentGroup == NULL) mCurrentGroup = groupCmdItem;

        return 0;
    }

    int LogConsole::changeGroup(const int client, const char *group) {
        TCommandItem *cmdItem;

        cmdItem = findGroupItem(mCommandItems, group);
        if (cmdItem == NULL) {
            return -1;
        }
        mCurrentGroup = cmdItem;

        return 0;
    }

    void LogConsole::displayCommandGroup(const int client, TCommandItem *cmdItem, const char *cmd) {
        TCommandItem *child;

        logArgs(client, "Group - [%s%s%s]\n", COLOR_CYAN, cmdItem->cmdGroup, COLOR_RESET);
        child = cmdItem->children;
        while (child) {
            if (cmd == NULL) {
                logArgs(client, "%s%16s%s : %s\n", COLOR_YELLOW, child->cmdName, COLOR_RESET, child->cmdHelp);
            } else {
                if (strncmp(child->cmdName, cmd, strlen(cmd)) == 0)
                    logArgs(client, "%s%16s%s : %s\n", COLOR_YELLOW, child->cmdName, COLOR_RESET, child->cmdHelp);
            }
            
            child = child->next;
        }
    }

    int  LogConsole::printCommandHelp(const int client, const char *cmd = NULL) {
        TCommandItem *cmdItem;

        // Display '/' group items      // Root Group will be existed in mCommandItems's final.
        cmdItem = mCommandItems;
        while (cmdItem) {
            displayCommandGroup(client, cmdItem, cmd);
            cmdItem = cmdItem->next;
        }
        cmdItem = mCommandItems;
    }

    #define MAXARGS_NUM         20
    // command = "command arg1 arg2 arg3 ..."
    void LogConsole::processRecvCommand(const int client, const char *command) {
        TCommandItem *cmdItem;
        int  argc;
        char *argv[MAXARGS_NUM], *str;

        char *temp;
        char *cmd, *args, *tok;

        if (strlen(command) == 0)
            return;

        temp = strdup(command);
        cmd = strtok_r(temp, " ,", &args);

        argc = 0;
        while ((tok = strtok_r(NULL, " ,", &args)))
            argv[argc++] = tok;

        cmdItem = findCommandItem(mCurrentGroup, cmd);
        if (cmdItem) {
            if (cmdItem->pfnCmdFunc)
                cmdItem->pfnCmdFunc(client, argc, (const char**)argv);
            free(temp);

            return;
        } else {
            //  Search in full command....
            TCommandItem *baseItem = mCommandItems;

            while (baseItem) {
                cmdItem = findCommandItem(baseItem, cmd);
                if (cmdItem) {
                    if (cmdItem->pfnCmdFunc)
                        cmdItem->pfnCmdFunc(client, argc, (const char**)argv);
                    free(temp);

                    return;
                }
                baseItem = baseItem->next;
            }
        }

        logArgs(client, "[ERR] Can't find command name [%s]\n", cmd);
        // if you want to display similar command list, please unblock below
        //logArgs(client, "List of command which is matched with [%s]\n", cmd);
        //printCommandHelp(client, cmd);
        //  if command is not existed in dictionary, try to run it in system
        runSystemCommand(client, command);
        free(temp);
    }

    void LogConsole::runSystemCommand(const int client, const char *cmd) {
        FILE *fp;
        char buf[512];

        fp = popen(cmd, "r");
        if (fp == NULL) {
            logArgs(client, "[ERR] Fail to run command - [%s]", cmd);
            return;
        }
        while (fgets(buf, sizeof(buf) - 1, fp) != NULL)
            logArgs(client, "%s", buf);
        pclose(fp);
    }

    TCommandItem*   LogConsole::findCommandItem(TCommandItem *baseItems, const char *cmdName) {
        TCommandItem *cmdItem = baseItems->children;
        while (cmdItem) {
            if (strcmp(cmdItem->cmdName, cmdName) == 0)
                return cmdItem;
            cmdItem = cmdItem->next;
        }

        return NULL;
    }

}

int test_func(const int client, const char *args) {
    printf("----> TestOnly");
    return 0;
}

#define LogConsole_AppendCommand(a, b, c, d)    {LogConsole::LogConsole::getInstance()->appendCommandItem(a, b, c, d);}
#define LogConsole_Print(client, ...)           {LogConsole::LogConsole::getInstance()->logArgs(client, __VA_ARGS__);}

int printCommandHelp(const int client, const int argc, const char **argv) {
    LogConsole::LogConsole *Console = LogConsole::LogConsole::getInstance();

    Console->printCommandHelp(client);

    return 0;
}

int changeCommandGroup(const int client, const int argc, const char **argv) {
    LogConsole::LogConsole *Console = LogConsole::LogConsole::getInstance();

    if (argc < 1)
        return -1;

    Console->changeGroup(client, argv[0]);
}

int testAdd2Values(const int client, const int argc, const char **argv) {
    LogConsole_Print(client, "%d\n", argc);
}

int testAdd3Values(const int client, const int argc, const char **argv) {
    for (int i = 0; i < argc; i++)
        LogConsole_Print(client, "%d - [%s]\n", i, argv[i]);
}

void initConsoleLog() {
    LogConsole::LogConsole *console = LogConsole::LogConsole::getInstance();

    LogConsole_AppendCommand("/", "help", "Print out all command functions", printCommandHelp);
    LogConsole_AppendCommand("/", "cd", "Change Command Group", changeCommandGroup);
    LogConsole_AppendCommand("/Test", "add2", "add two values", testAdd2Values);
    LogConsole_AppendCommand("/", "add3", "add three values", testAdd3Values);
}

using namespace std;
int main() {
    char szGroupName[64], szCmdName[32], szHelpName[32];
    int i, j, k;

    initConsoleLog();
    /*
    for (i = 0; i < 10; i++) {
        sprintf(szGroupName, "Group%d", i);
        for (j = 0; j < 10; j++) {
            sprintf(szCmdName, "Cmd%d-%d", i, j);
            LogConsole_AppendCommand(szGroupName, szCmdName, "TestHelp", test_func);
        }
    }
    */

    printCommandHelp(0, 0, NULL);

    while (1) {
        sleep(10000);
    }
}

