/*
 * Copyright (C) 2016 Alticast Corporation. All Rights Reserved.
 *
 * This software is the confidential and proprietary information
 * of Alticast Corporation. You may not use or distribute
 * this software except in compliance with the terms and conditions
 * of any applicable license agreement in writing between Alticast
 * Corporation and you.
 */

// includes header for TCP/IP Telnet Daemon
#include <stdio.h>  	/* standard in and output*/
#include <sys/socket.h> /* for socket() and socket functions*/
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>

#include <iostream>
#include <string>
#include <list>

namespace remoteConsole
{
    #define RCVBUFSIZE 512   /* Size of receive buffer */
    #define MAXPENDING 5    /* Maximum outstanding connection requests */

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

    class CommandItem
    {
        private:
            char *mCommandName;
            char *mCommandHelp;
            char *mCommandGroup;
        public:
            CommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp);
    };

    CommandItem::CommandItem(const char *cmdGroup, const char *cmdName, const char *cmdHelp) {
        mCommandGroup = strdup(cmdGroup);
        mCommandName = strdup(cmdName);
        mCommandHelp = strdup(cmdHelp);
    }

    class RemoteConsole
    {
        private:
            int mPortNo;
            int mServerSocket;
            std::list<CommandItem*> mCommandList;

            int initConsoleDaemon();
            void waitClientConnection();
            void doCommunicate(int client, int server);
        public:
            RemoteConsole(const int portNo);
    };

    RemoteConsole::RemoteConsole(const int portNo) {
        mPortNo = portNo;
        std::cout << "Remote Console Initialized with port " << portNo << std::endl;

        mCommandList.push_back(new CommandItem("CommandGroup1", "CommandName", "CommandHelp"));
        mCommandList.push_back(new CommandItem("CommandGroup1", "CommandName1", "CommandHelp"));
        mCommandList.push_back(new CommandItem("CommandGroup1", "CommandName2", "CommandHelp"));
        mCommandList.push_back(new CommandItem("CommandGroup1", "CommandName3", "CommandHelp"));

        std::list<CommandItem*>::iterator itor;
        for (itor = mCommandList.begin(); itor != mCommandList.end(); itor++)
            std::cout << "Test" << std::endl;

        initConsoleDaemon();
        waitClientConnection();
    }

    int RemoteConsole::initConsoleDaemon() {
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

    void RemoteConsole::waitClientConnection() {
        struct sockaddr_in clieAddr;
        int clieLen;
        int clieSocket;

        while (1) {
            clieLen = sizeof(clieAddr);
            clieSocket = accept(mServerSocket, (struct sockaddr *) &clieAddr, (socklen_t*)&clieLen);
            if (clieSocket < 0) {
                std::cout << "Error : Fail to connect client..." << std::endl;
                return;
            }

            std::cout << "Connected from " << inet_ntoa(clieAddr.sin_addr) << std::endl;

            // Thread function for below.
            doCommunicate(clieSocket, mServerSocket);
        }
    }

    void RemoteConsole::doCommunicate(int client, int server) {
        char echoBuffer[RCVBUFSIZE];
        char commandBuffer[RCVBUFSIZE], *pszCommandBuffer;
        int  recvMsgSize;

        pszCommandBuffer = commandBuffer;
        *pszCommandBuffer = 0;
        do {
            memset(echoBuffer, RCVBUFSIZE, 0);
            recvMsgSize = recv(client, echoBuffer, RCVBUFSIZE, 0);
            std::cout << recvMsgSize << std::endl;
            hex_dump(echoBuffer, recvMsgSize, std::cout);
            //std::cout << echoBuffer << std::endl;

            if (strncmp(echoBuffer, "cmd", strlen("cmd")) == 0)
                std::cout << "Find CMD" << std::endl;
        } while (recvMsgSize > 0);
        /*
        recvMsgSize = recv(client, echoBuffer, RCVBUFSIZE, 0);
        if (recvMsgSize < 0) {
            std::cout << "Error : receive error " << std::endl;
            return;
        }

        while (recvMsgSize > 0) {
            if (send(client, echoBuffer, recvMsgSize, 0) != recvMsgSize)
                std::cout << "Fail to send data..." << std::endl;

            if ((recvMsgSize == recv(client, echoBuffer, RCVBUFSIZE, 0)) < 0)
                std::cout << "Fail to Recv..." << std::endl;
            else
                hex_dump(echoBuffer, recvMsgSize, std::cout);
                //std::cout << "Received : " << echoBuffer << std::endl;
        }
        */

        close(client);    /* Close client socket */
    }
}

using namespace std;
int main() {
    remoteConsole::RemoteConsole *RC;

    RC = new remoteConsole::RemoteConsole(6001);
}
