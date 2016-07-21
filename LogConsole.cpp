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

//using namespace LogConsole;

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define	PRINT_CON(...)	DEFLOG::print(TLOGLEVEL_SYS, "SYS", __VA_ARGS__)

namespace DEFLOG {		//	for AOSP, Logcat binding.

#if 1		//	for emulation, aosp debug
void pl_log_err(const char *tag, const char *msg) {
	printf("[%s] %s", tag, msg);
}
void pl_log_wrn(const char *tag, const char *msg) {
	printf("[%s] %s", tag, msg);
}
void pl_log_msg(const char *tag, const char *msg) {
	printf("[%s] %s", tag, msg);
}
void pl_log_dbg(const char *tag, const char *msg) {
	printf("[%s] %s", tag, msg);
}
#endif
void print_args(const int loglevel, const char *tag, const char *format, va_list ap) {
	int written;
	char buf[512];

	written = vsnprintf(buf, 511, format, ap);
	buf[written] = '\0';

	switch (loglevel) {
	case TLOGLEVEL_ERR:
		pl_log_err(tag, buf);
		break;
	case TLOGLEVEL_WRN:
		pl_log_wrn(tag, buf);
		break;
	case TLOGLEVEL_MSG:
		pl_log_msg(tag, buf);
		break;
	case TLOGLEVEL_DBG:
	default:
		pl_log_dbg(tag, buf);
		break;
	}
}
void print(const int loglevel, const char *tag, const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	print_args(loglevel, tag, format, ap);
	va_end(ap);
}


}

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
			writeLine(COLOR_YELLOW "taurus:%s/$ " COLOR_RESET, getPrompt());
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

	void disconnect() {
		close(mClientSocket);
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

	int writeLine(const char *format, va_list ap) {
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

		if (findstr == NULL)
			return cur;

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

	TCommandItem* findCommandItem(TCommandItem *baseItems, const char *cmdName) {
		TCommandItem *cmdItem;

		if (baseItems == NULL)
			return NULL;

		cmdItem = baseItems->children;
		while (cmdItem) {
			if (strcmp(cmdItem->cmdName, cmdName) == 0)
				return cmdItem;
			cmdItem = cmdItem->next;
		}

		return NULL;
	}

public:
	TCommandItem *mCommandItems;
	TCommandItem *mCurrentGroup;

	CommandTool() {
		mCommandItems = NULL;
		mCurrentGroup = NULL;
		mLastUsedCmdGroup = NULL;
		PRINT_CON("Init CommandTool\n");
	}

	TCommandItem* getRootCommandItems() {
		return mCommandItems;
	}

	void changeCommandGroup(const char *group) {
		TCommandItem *cmdItem = findGroupItem(mCommandItems, group);

		if (cmdItem)
			mCurrentGroup = cmdItem;
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

	bool runCommandInGroup(const char *cmd, const int argc, const char **argv) {
		TCommandItem *cmdItem = findCommandItem(mCurrentGroup, cmd);

		if (cmdItem) {
			if (cmdItem->pfnCmdFunc)
				cmdItem->pfnCmdFunc(argc, (const char**) argv);

			return true;
		}
		return false;
	}

	bool runCommandInWhole(const char *cmd, const int argc, const char **argv) {
		TCommandItem *baseItem = mCommandItems;
		TCommandItem *cmdItem;

		while (baseItem) {
			cmdItem = findCommandItem(baseItem, cmd);
			if (cmdItem) {
				if (cmdItem->pfnCmdFunc)
					cmdItem->pfnCmdFunc(argc, (const char**) argv);

				return true;
			}
			baseItem = baseItem->next;
		}

		return false;
	}

};

class CommandDaemon: public ConnectorDaemon, public CommandTool {
#define	MAXARGS_NUM	20
private:
	int mAcceptedLogLevel;
	char *mAcceptedFiles;
	char *mAcceptedModules;

	void initInternalVariables() {
		mAcceptedLogLevel = TLOGLEVEL_ALL;
		mAcceptedFiles = NULL;
		mAcceptedModules = NULL;
	}

	void runSystemCommand(const char *cmd) {
		FILE *fp;
		char buf[512];

		fp = popen(cmd, "r");
		if (fp == NULL) {
			writeLine("Error : Fail to run command [%s]\n", cmd);
			return;
		}
		while (fgets(buf, sizeof(buf) - 1, fp) != NULL)
			print("%s", buf);
		pclose(fp);
	}

	int processCommand(const int client, const char *buffer) {
		int argc;
		char *argv[MAXARGS_NUM], *str;

		char *temp;
		char *cmd, *args, *tok;
		const char *command = buffer;
		bool result;

		if (strlen(command) == 0)
			return 0;

		temp = strdup(command);
		cmd = strtok_r(temp, " ,", &args);

		argc = 0;
		while ((tok = strtok_r(NULL, " ,", &args)))
			argv[argc++] = tok;

		result = runCommandInGroup(cmd, argc, (const char**) argv) ? true : runCommandInWhole(cmd, argc, (const char**) argv);
		if (!result) {
			print("Error : Can't find command name [%s]\n", cmd);
			runSystemCommand(command);
		}

		free(temp);

		return 0;
	}

	void displayCommandGroup(TCommandItem *cmdItem, const char *cmd) {
		TCommandItem *child;

		print("Group - [%s%s%s]\n", COLOR_CYAN, cmdItem->cmdGroup, COLOR_RESET);
		child = cmdItem->children;
		while (child) {
			if (cmd == NULL) {
				print("%s%16s%s : %s\n", COLOR_YELLOW, child->cmdName, COLOR_RESET, child->cmdHelp);
			} else {
				if (strncmp(child->cmdName, cmd, strlen(cmd)) == 0)
					print("%s%16s%s : %s\n", COLOR_YELLOW, child->cmdName, COLOR_RESET, child->cmdHelp);
			}

			child = child->next;
		}
	}

	char *getPrompt() {
		return (char*) mCurrentGroup->cmdGroup;
	}

	bool isPrintable(const int loglevel, const char *module, const char *file) {
		bool result;

		result = loglevel & mAcceptedLogLevel;

		//	LOGLEVEL_SYS not filtering.
		if (TLOGLEVEL_SYS & loglevel)
			return result;

		if (module && mAcceptedModules) {
			if (strstr(mAcceptedModules, module) == NULL)
				result = false;
		}
		if (file && mAcceptedFiles) {
			if (strstr(mAcceptedFiles, file) == NULL)
				result = false;
		}
		return result;
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

	void printArgs(const char *module, const char *file, const int line, const int loglevel, const char *format, ...) {
		va_list ap;

		if (not isPrintable(loglevel, module, file)) {
			return;
		}

		va_start(ap, format);
		printArgv(module, file, line, loglevel, format, ap);
		va_end(ap);
	}

	void printArgv(const char *module, const char *file, const int line, const int loglevel, const char *format, va_list ap) {
		if (not isPrintable(loglevel, module, file)) {
			return;
		}

		if (module)
			writeLine("[%s]", module);
		if (file)
			writeLine("[%s:%d]", file, line);

		writeLine(format, ap);

		DEFLOG::print_args(loglevel, module, format, ap);
	}

	int printCommandHelp(const char *cmd = NULL) {
		TCommandItem *cmdItem = getRootCommandItems();

		// Display '/' group items      // Root Group will be existed in mCommandItems's final.
		while (cmdItem) {
			displayCommandGroup(cmdItem, cmd);
			cmdItem = cmdItem->next;
		}

		return 0;
	}

};

void TLOG::print(const char *module, const char *file, const int line, const int loglevel, const char *format, ...) {
	va_list ap;
	CommandDaemon *console = CommandDaemon::getInstance();

	va_start(ap, format);
	console->printArgv(module, file, line, loglevel, format, ap);
	va_end(ap);
}

int TLOG::appendCommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp, fnLogConsoleCallBack func) {
	CommandDaemon *console = CommandDaemon::getInstance();

	return console->appendCommandItem(cmdGroup, cmdName, cmdHelp, func);
}

#if 0
int _changeCommandGroup(const int client, const int argc, const char **argv) {
	LogConsole::ConsoleDaemon *Console = LogConsole::ConsoleDaemon::getInstance();

	if (argc < 1)
	return -1;

	Console->changeGroup(client, argv[0]);

	return 0;
}

int _changeConsoleParameters(const int client, const int argc, const char **argv) {
	LogConsole::ConsoleDaemon *Console = LogConsole::ConsoleDaemon::getInstance();

	Console->enableParameters(client, argc, argv);
	return 0;
}

appendCommandItem("/", "help", "Print out all command functions", _printCommandHelp);
appendCommandItem("/", "cd", "Change command group", _changeCommandGroup);
appendCommandItem("/", "enable", "Enable Console parameters", _changeConsoleParameters);
#endif

int __printCommandHelp(const int argc, const char **argv) {
	CommandDaemon *Console = CommandDaemon::getInstance();

	Console->printCommandHelp(NULL);

	return 0;
}

int __disconnectConsole(const int argc, const char **argv) {
	CommandDaemon *Console = CommandDaemon::getInstance();

	Console->disconnect();

	return 0;
}

int __changeCommandGroup(const int argc, const char **argv) {
	CommandDaemon *Console = CommandDaemon::getInstance();

	if (argc < 1)
		Console->changeCommandGroup(NULL);
	else
		Console->changeCommandGroup(argv[0]);

	return 0;
}

int __add2Value(const int argc, const char **argv) {
	int a, b;

	if (argc != 2) {
		TLOG_CMD("Error : argument is not matched\n");
		return -1;
	}

	a = atoi(argv[0]);
	b = atoi(argv[1]);

	TLOG_CMD("%d + %d = %d\n", a, b, a + b);

	return 0;
}

DECLARE_MODULE_NAME("LOGCONSOL")
int main() {
	TLOG_DBG("Test Debug...\n");
	TLOG_ADDCMD(":", "exit", "Disconnect console", __disconnectConsole);
	TLOG_ADDCMD(":", "help", "Print out all command functions", __printCommandHelp);
	TLOG_ADDCMD(":", "ccd", "Change Command Group", __changeCommandGroup);

	TLOG_ADDCMD("sample", "add2", "add two values", __add2Value);
	while (1)
		sleep(10000);
}
