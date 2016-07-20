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

#include "LogConsole.h"

using namespace LogConsole;

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define	PRINT_CON(...)	printf(__VA_ARGS__)
#define	PRINT_NET(...)	printf(__VA_ARGS__)

class SimpleThread {
private:
	pthread_t simpleThread;
	int retval;

public:
	SimpleThread() {
		retval = 0;
		memset(&simpleThread, 0, sizeof(pthread_t));
	}

	virtual ~SimpleThread() {
	}

	void start() {
		pthread_create(&simpleThread, NULL, SimpleThread::run_, (void *) this);
	}

	int join() {
		pthread_join(simpleThread, NULL);
		return retval;
	}

	void setRetval(int r) {
		retval = r;
	}

	static void *run_(void *pthis_) {
		SimpleThread *pthis = (SimpleThread *) pthis_;
		pthis->setRetval(pthis->run());
		pthread_exit(NULL);
	}

	virtual int run() {
		//	TODO:
		return 0;
	}
};

class ConnectorDaemon: public SimpleThread {

#define MAXPENDING 5    /* Maximum outstanding connection requests */
#define RCVBUFSIZE 512   /* Size of receive buffer */

private:
	int mPortNo;
	int mServerSocket;
	int mClientSocket;

	int initConsoleDaemon() {
		struct sockaddr_in servAddr;

		mServerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mServerSocket < 0) {
			PRINT_CON("Error : Socket Initialization failed...\n");
			return -1;
		}

		memset(&servAddr, 0, sizeof(servAddr));
		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servAddr.sin_port = htons(mPortNo);
		if (bind(mServerSocket, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
			PRINT_CON("Error : Socket Binding Failed...\n");
			return -1;
		}

		if (listen(mServerSocket, MAXPENDING) < 0) {
			PRINT_CON("Error : Fail to Listen in Server Side..\n");
			return -1;
		}

		PRINT_CON("Telnet Console daemon is now running with port 6001\n");

		return 0;
	}

	void waitClientConnection() {
		struct sockaddr_in clieAddr;
		int clieLen;
		int clieSocket;

		while (1) {
			PRINT_CON("Waiting Connection...\n");
			clieLen = sizeof(clieAddr);
			clieSocket = accept(mServerSocket, (struct sockaddr *) &clieAddr, (socklen_t*) &clieLen);
			if (clieSocket < 0) {
				PRINT_CON("Error : Fail to connect client...\n");
				return;
			}

			PRINT_CON("Connected from %s\n", inet_ntoa(clieAddr.sin_addr));

			// force telnet character mode -
			// send(clieSocket, "\377\375\042\377\373\001", 6, 0);
			// if you allow multiple connection, just contains below function in SimpleThread
			mClientSocket = clieSocket;
			doCommunicate(clieSocket, mServerSocket);
		}

		PRINT_CON("Finish waiting client...\n");
	}

	int readLine_(const int client, char *buffer, const int buffersize) {
		//TODO: Should support Telnet Character mode.
		return recv(client, buffer, buffersize, 0);
	}

	int writeLine_(const int client, char *buffer, const int buffersize) {
		return send(client, buffer, buffersize, 0);
	}

	void doCommunicate(int client, int server) {
		char echoBuffer[RCVBUFSIZE], *pos;
		int readno;

		do {
			writeLine(COLOR_YELLOW "taurus:%s$ " COLOR_RESET, getPrompt());
			readno = readLine_(client, echoBuffer, RCVBUFSIZE);
			echoBuffer[readno] = 0;

			//	Strip CR/LF
			if ((pos = strchr(echoBuffer, 0x0d)) != NULL)
				*pos = '\0';
			if ((pos = strchr(echoBuffer, 0x0a)) != NULL)
				*pos = '\0';

			processCommand(client, echoBuffer);
		} while (readno > 0);

		close(client); /* Close client socket */
	}

public:
	ConnectorDaemon(const int portNo) {
		mPortNo = portNo;
		initConsoleDaemon();
		writeLine("Init Connector\n");
	}

	~ConnectorDaemon() {
		//	TODO:
	}

	int run() {
		waitClientConnection();

		return 0;
	}

	int readLine(char *buffer, const int buffersize) {
		//TODO: Should support Telnet Character mode.
		return readLine_(mClientSocket, buffer, buffersize);
	}

	int writeLine(const char *format, ...) {
		char buf[512];
		int written;
		va_list ap;

		va_start(ap, format);
		written = vsnprintf(buf, 511, format, ap);
		va_end(ap);

		buf[written] = 0;

		return writeLine_(mClientSocket, buf, written + 1);
	}

	int	writeLine(const char *format, va_list ap) {
		char buf[512];
		int written;

		written = vsnprintf(buf, 511, format, ap);
		buf[written] = 0;

		return writeLine_(mClientSocket, buf, written + 1);
	}

	virtual char *getPrompt() = 0;
	virtual int processCommand(const int client, const char *buffer) = 0;
};

class CommandUtils {
public:
	static char *strstrip(const char *string, const char *chars) {
		char *newstr = (char*) malloc(strlen(string) + 1);
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
};

typedef struct CommandItem {
	char *cmdGroup;
	char *cmdName;
	char *cmdHelp;
	fnLogConsoleCallBack pfnCmdFunc; //int  (*pfnCmdFunc)(const int client, const int argc, const char **argv);

	struct CommandItem *prev;
	struct CommandItem *next;

	struct CommandItem *children;
} TCommandItem;

class CommandTool {
private:
	TCommandItem *mCommandItems;
	TCommandItem *mCurrentGroup;
	char *mLastUsedCmdGroup;

	TCommandItem* makeCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func) {
		TCommandItem *cmdItem = (TCommandItem*) calloc(1, sizeof(TCommandItem));

		cmdItem->cmdGroup = strdup(cmdGroup);
		if (cmdName)
			cmdItem->cmdName = strdup(cmdName);
		if (cmdHelp)
			cmdItem->cmdHelp = strdup(cmdHelp);

		cmdItem->pfnCmdFunc = func;

		return cmdItem;
	}

	TCommandItem* findGroupItem(TCommandItem *commandItems, const char *findstr) {
		TCommandItem *cur = commandItems;

		while (cur) {
			if (strcmp(findstr, cur->cmdGroup) == 0)
				return cur;
			cur = cur->next;
		}

		return cur;
	}

	TCommandItem* insertCommandItem(TCommandItem *commandItems, TCommandItem *item) {
		if (commandItems == NULL) {
			commandItems = item;
		} else {
			commandItems->prev = item;
			item->next = commandItems;
		}

		return item;
	}

public:
	CommandTool() {
		mCommandItems = NULL;
		mCurrentGroup = NULL;
		mLastUsedCmdGroup = NULL;
		PRINT_NET("Init CommandTool\n");
	}

	int appendCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func) {
		TCommandItem *cmdItem, *groupCmdItem;

		if (cmdGroup == NULL)
			cmdGroup = mLastUsedCmdGroup;
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

		if (mCurrentGroup == NULL)
			mCurrentGroup = groupCmdItem;

		return 0;
	}
};

class CommandDaemon: public ConnectorDaemon, public CommandTool {
private:
	int mAcceptedLogLevel;

	void initInternalVariables() {
		mAcceptedLogLevel = LOG_LEVEL_ALL;
	}

	int processCommand(const int client, const char *buffer) {
		//	TODO: process command.
		return 0;
	}

	char *getPrompt() {
		return (char*) "Test";
	}

	bool isPrintable(const int loglevel, const char *module, const char *file) {
		return loglevel & mAcceptedLogLevel;
	}

public:
	CommandDaemon(const int portNo = 6001) :
			ConnectorDaemon(portNo) {
		PRINT_CON("Init CommandDaemon\n");
		initInternalVariables();
		start();
	}

	static CommandDaemon* getInstance() {
		static CommandDaemon *_commandInstance = NULL;

		if (_commandInstance == NULL) {
			_commandInstance = new CommandDaemon(6001);
		}

		return _commandInstance;
	}

	void print(const char *format, ...) {
		va_list ap;

		va_start(ap, format);
		writeLine(format, ap);
		va_end(ap);
	}

	void printArgs(const char *module, const char *file, const char *line, const int loglevel, const char *format, ...) {
		va_list ap;

		if (not isPrintable(loglevel, module, file)) {
			return;
		}

		va_start(ap, format);
		printArgv(module, file, line, loglevel, format, ap);
		va_end(ap);
	}

	void printArgv(const char *module, const char *file, const char *line, const int loglevel, const char *format, va_list ap) {
		if (not isPrintable(loglevel, module, file)) {
			return;
		}
	}
};

int main() {
	CommandDaemon *cmdDaemon;

	cmdDaemon = new CommandDaemon();

	while (1)
		sleep(10000);
}
