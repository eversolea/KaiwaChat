//CS544 - Computer Networks -  The server for the Kaiwa chat protocol
//Ported socket algorithm code only from sample code on this website:
//http://www.rohitab.com/discuss/topic/26991-cc-how-to-code-a-multi-client-server-in-c-using-threads/


#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <winsock.h>
#include <string>
#include <iostream>
#include <vector>
#include <time.h>


//GLOBAL VARIABLES:
int SERVER_VERSION = 1;

//Commands
int VERSION_NEGOTIATION = 1;
int CLIENT_INFO = 2;
int JOIN_CHANNEL = 3;
int SEND_MESSAGE = 4;
int RECIEVE_MESSAGE = 5;
int LEAVE_CHANNEL = 6;
int PING_PONG = 7;
int QUERY = 8;
int QUIT = 0;

//Struct used to organize the chat history storage
struct chatMsg
{
    std::string message;
    std::string user;
    std::string channel;
};

//Chat history will hold all messages
std::vector<chatMsg> chatHistory;

//Debug function to display the contents of incoming and outgoing PDU (references commented out in final build)
void outputDebugInfo(std::string debugCat, std::string prefix, int command, char* parameters)
{
    std::cout << "\n" << debugCat << "\n";
    std::cout << "\tPrefix: " << prefix << "\n";
    std::cout << "\tCommand: " << command << "\n";
    std::cout << "\tParameters: " << parameters << "\n\n";
}

//when std::strings are converted to c-string's, addTerminator ensures that the c-string has a terminating null character
char* addTerminator(char* cstr)
{
    cstr[strlen(cstr)] = '\0';
    return cstr;
}

// our thread for recving commands - this is what the multiple threads will use
DWORD WINAPI receive_cmds(LPVOID lpParam)
{
    printf("- thread created\r\n");

    // set our socket to the socket passed in as a parameter   
    SOCKET current_client = (SOCKET)lpParam;

    // buffer to hold our recived data
    char buf[2260];
    // buffer to hold our sent data
    char sendData[2260];
    // for error checking 
    int res;
    
    //Statefull variables and client information
    bool pingRecieved = false;
    time_t pingRecievedTime = 0;

    bool awaiting_VersionNegotiation = true;
    int selectedVersion = -1;

    bool awaiting_ClientInformation = true;
    std::string username;
    
    bool joinedChannel = false;
    std::string current_channel;

    int chatHistoryIndex = 0; //The index to the most recent chat we know about b/f a query command


    //PDU Components:
    char prefix_charArray[256]; //256 bytes/charactes
    int command; //4 bytes
    char parameters[2000];


    while (true)
    {
        //recieve message from client
        res = recv(current_client, buf, sizeof(buf), 0); // recv cmds

        Sleep(10);

        if (res == 0)
        {
            MessageBox(0, (LPCWSTR)"error", (LPCWSTR)"error", MB_OK);
            closesocket(current_client);
            ExitThread(0);
        }


        //Timeout if it has been 30 seconds after recieving ping from client in first 4 DFA states
        if (pingRecieved && (awaiting_VersionNegotiation || awaiting_ClientInformation) && (time(NULL) - pingRecievedTime > 30))
        {
            //close the socket associted with this client and end this thread
            closesocket(current_client);
            ExitThread(0);
        }

        
        memcpy(prefix_charArray, buf, 256);
        int command = *(int*)&buf[256];
        char parameters[2000];
        memcpy(&parameters, &buf[260], 2000);

        
        if (!pingRecieved) //STATEFUL: DFA Node 1
        {
            

            if (command == PING_PONG) 
            {
                std::string prefix = "";
                //Ping recieved! Edit the buffer to send a pong (TODO: although we could just the ping back as a pong probably)
                prefix = '\0';
                char prefix_charArray[256];
                strcpy_s(prefix_charArray, prefix.c_str());
                command = PING_PONG;
                parameters[0] = '\0';
                memcpy(&sendData[0], &prefix_charArray, 256);
                memcpy(&sendData[256], &command, 4);
                memcpy(&sendData[260], &parameters, 2000);

                Sleep(10); //send msg back
                send(current_client, sendData, sizeof(sendData), 0);


                //Also send the current server version in a server negotiation PDU 

                prefix = "SERVER";
                strcpy_s(prefix_charArray, prefix.c_str());
                command = VERSION_NEGOTIATION;
                memcpy(&parameters[0], &SERVER_VERSION, 4);
                memcpy(&sendData[0], &prefix_charArray, 256);
                memcpy(&sendData[256], &command, 4);
                memcpy(&sendData[260], &parameters, 2000);

                Sleep(10);
                send(current_client, sendData, sizeof(sendData), 0);

                //Move to the next state of the DFA
                pingRecieved = true;
                pingRecievedTime = time(NULL);
            }


        }
        else
        {
            if (awaiting_VersionNegotiation) //STATEFUL: DFA Node 3
            {
                //Waiting for a PDU from the client with the selected version of KaiChat that we will communicate on
                //Note: Endianness-compatibility is not currently implemented but this is where we would check for endianness
                //and adjust an endianness flag for the rest of this client communication.
                //1 in big endian would be 16777216 in little endian
                if (command == VERSION_NEGOTIATION)
                {
                    time_t t = time(NULL);
                    //Set the selectedVersion variable to the version the client has selected
                    memcpy(&selectedVersion, &parameters, 4);
                    awaiting_VersionNegotiation = false;
                }

            }
            else if (awaiting_ClientInformation) //STATEFUL: DFA Node 4
            {
                //Waiting for a Client Information PDU from the client 
                if (command == CLIENT_INFO)
                {
                    char username_charArray[10];
                    memcpy(&username_charArray, &parameters[0], 10); //Username can't be more than 10 chars/bytes
                    username = addTerminator(username_charArray);
                    //Server-to-Client introductions are now complete - we are officially connected to the client.
                    //Move on to "CONNECTED" DFA node 5
                    awaiting_ClientInformation = false;
                    std::cout << "User " << username << " (" << current_client << ") has succesfully connected with KaiChat version " << selectedVersion << "\n";
                }

            }
            else if (!joinedChannel)
            {
                //The Client and Server are now officially "connected".
                //STATEFUL: Here in DFA node 5, a client can either join a channel, quit, or do nothing.
                //Server debug output:

                if (command == JOIN_CHANNEL)
                {
                    //We technically already have the clients username because we are running multiple threads
                    //So we will just pull the channel from the thread
                    char current_channel_charArray[50];
                    memcpy(&current_channel_charArray, &prefix_charArray[11], 50);
                    current_channel = addTerminator(current_channel_charArray);
                    joinedChannel = true;
                    std::cout << "User " << username << " (" << current_client << ") has joined " << current_channel << "\n";
                }
                else if (command == QUIT)
                {

                    // close the socket associted with this client and end this thread
                    std::cout << "User " << username << " (" << current_client << ") has disconnected from Kaichat\n";
                    closesocket(current_client);
                    ExitThread(0);

                }
            }
            else
            {
                //STATEFUL: The user has entered the Chat server mode (DFA Node 6)
                //Here, the user can send messages, do nothing (probably read messages they recieve), leave the channel, or quit.

                if (command == SEND_MESSAGE)
                {
                    //Client has sent a message
                    //Add this message to a queue to send to all clients
                    chatMsg msg;
                    msg.user = username;
                    msg.channel = current_channel;
                    msg.message = parameters;
                    chatHistory.push_back(msg);

                    std::cout << "Message (" << username << "/" << current_channel << "): " << parameters << "\n";

                }

                else if (command == QUERY)
                {
                    //Send RECIEVE_MSG PDU's to user for each new message on the queue
                    if (chatHistory.size() > chatHistoryIndex)
                    {
                        int numMsgsBehind = chatHistory.size() - chatHistoryIndex;
                        for (int i = 0; i < numMsgsBehind; i++)
                        {
                            std::string msgUser = chatHistory[chatHistoryIndex + i].user;
                            if (msgUser == username) { continue;  } //Skip our own messages - don't need to send those back
                            std::string msgChannel = chatHistory[chatHistoryIndex + i].channel;
                            std::string message = chatHistory[chatHistoryIndex + i].message;
                            int upToDateFlag = 0;
                            char spaceDelim = ' ';
                            command = RECIEVE_MESSAGE;

                            memcpy(&prefix_charArray[0], &msgUser[0], 10);
                            memcpy(&prefix_charArray[10], &spaceDelim, 1);
                            memcpy(&prefix_charArray[11], &msgChannel[0], 50);
                            memcpy(&prefix_charArray[51], &spaceDelim, 1);
                            memcpy(&prefix_charArray[51], &upToDateFlag, 1);
                            memcpy(&parameters[0], &message[0], 2000);

                            memcpy(&sendData[0], &prefix_charArray[0], 256);
                            memcpy(&sendData[256], &command, 4);
                            memcpy(&sendData[260], &parameters[0], 2000);

                            Sleep(10);
                            send(current_client, sendData, sizeof(sendData), 0);
                        }
                        chatHistoryIndex += numMsgsBehind;
                    }
                    //Otherwise, send a RECIEVE_MSG PDU with up-to-date flag on
                    else
                    {
                        int upToDateFlag = 1;
                        command = RECIEVE_MESSAGE;
                        
                        memcpy(&prefix_charArray[51], &upToDateFlag, 1);

                        memcpy(&sendData[0], &prefix_charArray, 256);
                        memcpy(&sendData[256], &command, 4);
                        memcpy(&sendData[260], &parameters, 2000);

                        Sleep(10);
                        send(current_client, sendData, sizeof(sendData), 0);
                    }
                }

                else if (command == LEAVE_CHANNEL)
                {
                    //We are returning to DFA state 5
                    std::cout << "User " << username << " (" << current_client << ") has left the channel " << current_channel << "\n";
                    current_channel = "";
                    joinedChannel = false;
                }
                else if (command == QUIT)
                {
                    // close the socket associated with this client and end this thread
                    std::cout << "User " << username << " (" << current_client << ") in channel " << current_channel << " has disconnected from Kaichat\n";
                    closesocket(current_client);
                    ExitThread(0);
                }
                else
                {
                    //No relevant command is given
                }


            }
        }
    }
}

//The main function for the concurrent KawiaChat server
int main()
{
    printf("Welcome to the Multi-threaded TCP server - KaiwaChat\r\nInformation will appear below as clients connect to the server.\n");

    // our masterSocket(socket that listens for connections)
    SOCKET sock;

    // for our thread
    DWORD thread;

    WSADATA wsaData;
    sockaddr_in server;

    // start winsock
    int ret = WSAStartup(0x101, &wsaData); // use highest version of winsock avalible

    if (ret != 0)
    {
        return 0;
    }

    // fill in winsock struct ... 
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(29200); //Uses TCP Port 29200

    // create our socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET)
    {
        return 0;
    }

    // bind our socket to a port (port 29200)
    if (bind(sock, (sockaddr*)&server, sizeof(server)) != 0)
    {
        return 0;
    }

    // listen for a connection  
    if (listen(sock, 5) != 0)
    {
        return 0;
    }

    // socket that we send and recv data on
    SOCKET client;

    sockaddr_in from;
    int fromlen = sizeof(from);

    // loop forever 
    while (true)
    {
        // accept connections
        client = accept(sock, (struct sockaddr*)&from, &fromlen);
        printf("Client connected");

        // create our recv_cmds thread and parse client socket as a parameter
        CreateThread(NULL, 0, receive_cmds, (LPVOID)client, 0, &thread);
    }

    // shutdown winsock
    closesocket(sock);
    WSACleanup();

    // exit
    return 0;
}