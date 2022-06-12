//CS544 - Computer Networks -  The server for the Kaiwa chat protocol

//Ported socket algorithm code only from sample code on this website:
//http://www.rohitab.com/discuss/topic/26991-cc-how-to-code-a-multi-client-server-in-c-using-threads/

#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <conio.h>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <vector>

int CLIENT_VERSION = 1;
int SOCKET_READ_TIMEOUT_SEC = 1;

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


//DECLARATIONS
    //error trapping signals
#define SIGINT 2
#define SIGKILL 9
#define SIGQUIT 3
// SOCKETS
SOCKET sock, client;



//Signal Handle if it crashes
void s_handle(int s)
{
    if (sock)
        closesocket(sock);
    if (client)
        closesocket(client);
    WSACleanup();
    Sleep(1000);
    std::cout << "EXIT SIGNAL :" << s;
    exit(0);
}

//Handles Errors and outputs error message before passing to signal handle
void s_cl(const char* a, int x)
{
    std::cout << a;
    s_handle(x + 1000);
}

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

//The main function for the KaiwaChat Client
int main()
{
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    SetConsoleTitle(L".:: KaiwaChat Client - Version 1.0 ::. ");

    
    //Declarations
    DWORD poll;
    int res, i = 1, port = 999;
    char buf[2260];
    char ip[15];
    WSADATA data;

    //Sets up signal handling (similar to try/catch)
    signal(SIGINT, s_handle);
    signal(SIGKILL, s_handle);
    signal(SIGQUIT, s_handle);

    std::cout << "\t\tWelcome to KaiwaChat\n\t\tVersion 1\n";

    std::cout << "\n\n\t\tEnter IP to connect to chat server: ";
    std::string ip_str;
    std::getline(std::cin, ip_str);

    sockaddr_in ser;
    sockaddr addr;


    ser.sin_family = AF_INET;
    ser.sin_port = htons(29200);                    //Set the port
    ser.sin_addr.s_addr = inet_addr(ip_str.c_str());      //Set the address we want to connect to

    memcpy(&addr, &ser, sizeof(SOCKADDR_IN));


    //Initializing Winsock
    res = WSAStartup(MAKEWORD(1, 1), &data);      //Start Winsock
    std::cout << "\n\nWSAStartup"
        << "\nVersion: " << data.wVersion
        << "\nDescription: " << data.szDescription
        << "\nStatus: " << data.szSystemStatus << "\n";

    if (res != 0)
        s_cl("WSAStarup failed", WSAGetLastError());


    //Creating a TCP Socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);       //Create the socket
    if (sock == INVALID_SOCKET)
        s_cl("Invalid Socket ", WSAGetLastError());
    else if (sock == SOCKET_ERROR)
        s_cl("Socket Error)", WSAGetLastError());
    else
        std::cout << "Socket Established" << "\n";


    //Connect to remote server
        res = connect(sock, &addr, sizeof(addr));               //Connect to the server
    if (res != 0)
    {
        s_cl("SERVER UNAVAILABLE", res);
    }
    else
    {
        std::cout << "\nConnected to Server!\n\n";
        memcpy(&ser, &addr, sizeof(SOCKADDR));
    }

    char RecvdData[2260] = "";
    int ret;
    bool pongNotRecieved = true;
    time_t timeSentPing = 0;
    time_t timeRecievedPong = 0;
    bool pingSent = false;

    bool versionNegotiation = true; //become false once version negotiation is completed
    int version = -1;

    bool joinedChannel = false;
    std::string channel = "";
    std::string username = "";

    time_t lastUpdate = 0;

    //PDU Components:
    std::string prefix; //256 bytes/charactes
    int command; //4 bytes
    char parameters[2000];

    //Loop to send to and recieve messages from server
    while (true)
    {

        if (pongNotRecieved) //STATEFUL: DFA node 1
        {
            if (!pingSent)
            {
                timeSentPing = time(NULL);
                //Send a ping to the server

                //We will set the prefix, command, and command parameter components of our PDU,
                prefix = '\0';
                char prefix_charArray[256];
                strcpy_s(prefix_charArray, prefix.c_str());
                command = PING_PONG;
                parameters[0] = '\0'; 

                //Copy the PDU components into our PDU buffer,
                memcpy(&buf[0], &prefix_charArray, 256);
                memcpy(&buf[256], &command, 4);
                memcpy(&buf[260], &parameters, 2000);

                Sleep(5);
                //And send the PDU Buffer.
                res = send(sock, buf, sizeof(buf), 0);

                pingSent = true;
            }
            else
            {
                //Wait for a ping PDU back from the server
                ret = recv(sock, RecvdData, sizeof(RecvdData), 0);
                if (ret > 0) //if 0, server terminated connection
                {
                    //If we recieved a PDU from the server, we split the PDU into the three components: Prefix, command, and commmand parameters
                    char prefix_charArray[256];
                    memcpy(prefix_charArray, &RecvdData[0], 256);
                    prefix = addTerminator(prefix_charArray);
                    command = *(int*)&RecvdData[256];
                    memcpy(&parameters, &RecvdData[260], 2000);

                    //If the PDU is a Pong from the server (a response from the server)
                    if (command == PING_PONG)
                    {
                        //Advance to the 2nd node of the DFA
                        pongNotRecieved = false; 
                    }
                }

                //Timeout if it has been 30 seconds since ping to server was sent
                if ((time(NULL) - timeSentPing > 30))
                {
                    std::cout << "\nHaving trouble connecting - please restart the client.\n";
                    //go back to beginning of DFA
                    closesocket(client);
                    client = 0;
                    break;
                }
            }
        }
        else
        {
            if (versionNegotiation) //STATEFUL: DFA Node 2
            {
                timeRecievedPong = time(NULL);
                //Wait for a Verision Negotation PDU from the server 
                ret = recv(sock, RecvdData, sizeof(RecvdData), 0);
                if (ret > 0) 
                {
                    char prefix_charArray[256];
                    memcpy(prefix_charArray, &RecvdData[0], 256);
                    prefix = addTerminator(prefix_charArray);
                    command = *(int*)&RecvdData[256];
                    memcpy(&parameters, &RecvdData[260], 2000);

                    //outputDebugInfo("Server's Version Negotation", prefix, command, parameters);

                    //Check if the recieved PDU is a Version Negotation PDU (which will tell us the Server's version #)
                    if (command == 1 && prefix == "SERVER")
                    {
                        //This version negotiation message came from the server
                        int serverVersion;
                        memcpy(&serverVersion, &parameters[0], 4);

                        
                        //Send a version negotation PDU back to the server
                        //This PDU will select the version of KaiChat for the client and server to communicate on
                        prefix = '\0';
                        char prefix_charArray[256];
                        strcpy_s(prefix_charArray, prefix.c_str());
                        command = VERSION_NEGOTIATION;

                        if (serverVersion >= CLIENT_VERSION)
                        {
                            parameters[0] = CLIENT_VERSION;
                        }
                        else
                        {
                            //Server Version is lower than Client version: Communicate on the server's version
                            parameters[0] = serverVersion;
                        }


                        memcpy(&buf[0], &prefix_charArray, 256);
                        memcpy(&buf[256], &command, 4);
                        memcpy(&buf[260], &parameters, 2000);

                        Sleep(5);
                        res = send(sock, buf, sizeof(buf), 0);

                        //outputDebugInfo("client->server Version Negotation", prefix, command, parameters);

                        //Version Negotiation is complete.
                        versionNegotiation = false;

                        //Next, we want to send our Client information using a CLIENT_INFO PDU to the server
                        
                        //Prompt the user for a desired username
                        bool validUsernameChosen = false;
                        std::cout << "\n Enter your username (10 characters max): ";

                        //Make sure username is 10 characters or less
                        while (!validUsernameChosen)
                        {
                            std::getline(std::cin, username);
                            if (username.length() > 10)
                            {
                                std::cout << "\n\n ERROR: Username must be 10 characters or less.";
                                std::cout << "\n Please try again: ";
                            }
                            else
                            {
                                validUsernameChosen = true;
                            }
                        }

                        //Compose and send the Client Info PDU
                        prefix = '\0';  
                        strcpy_s(prefix_charArray, prefix.c_str());
                        command = CLIENT_INFO;
                        parameters[0] = '\0';
                        const char * username_charArray = username.c_str();
                        memcpy(&parameters[0], &username_charArray[0], 10); //Username can't be more than 10 chars/bytes

                        memcpy(&buf[0], &prefix_charArray, 256);
                        memcpy(&buf[256], &command, 4);
                        memcpy(&buf[260], &parameters, 2000);

                        Sleep(5);
                        res = send(sock, buf, sizeof(buf), 0);

                        //outputDebugInfo("Seding Client Info:", prefix, command, parameters);


                    }

                }

                //Timeout if it has been 30 seconds since version negotiation PDU from server was expected
                if ((time(NULL) - timeRecievedPong > 30))
                {
                    std::cout << "\nHaving trouble connecting - please restart the client\n";
                    //go back to beginning of DFA
                    closesocket(client);
                    client = 0;
                    break;
                }
            }
            else
            {
                if (!joinedChannel)
                {
                    //Succesfully connected to server!
                    //Here the user can join a channel, do nothing, or quit the KaiwaChat client
                    std::cout << "Succesfully connected to the server as " << username << "!\n";
                    std::cout << "Enter a channel to join starting with @ (you can also quit at anytime with !quit): ";
                    std::getline(std::cin, channel);
                    if (channel == "!quit")
                    {
                        //Quit the program
                        printf("\nThank you for using KaiChat! \n");

                        //Compose and send the Quit PDU
                        prefix = '\0';
                        char prefix_charArray[256];
                        strcpy_s(prefix_charArray, prefix.c_str());
                        command = QUIT;
                        parameters[0] = '\0';

                        memcpy(&buf[0], &prefix_charArray, 256);
                        memcpy(&buf[256], &command, 4);
                        memcpy(&buf[260], &parameters, 2000);

                        Sleep(5);
                        res = send(sock, buf, sizeof(buf), 0);


                        Sleep(40);
                        closesocket(client);
                        client = 0;
                        break;
                    }
                    else
                    {
                        //User would like to join a channel.
                        //No need to confirm if channel exists or not - If channel doesn't exist, server will create it. 

                        //Compose and send the Join Channel PDU to the Server
                        char prefix_charArray[256];
                        //Compose the prefix (username + ' ' + channel)
                        char spaceDelim = ' ';
                        const char* username_charArray = username.c_str();
                        const char* channel_charArray = channel.c_str();
                        memcpy(&prefix_charArray[0], &username_charArray[0], 10);
                        memcpy(&prefix_charArray[10], &spaceDelim, 1);
                        memcpy(&prefix_charArray[11], &channel_charArray[0], 50);

                        

                        command = JOIN_CHANNEL;
                        parameters[0] = '\0';

                        memcpy(&buf[0], &prefix_charArray, 256);
                        memcpy(&buf[256], &command, 4);
                        memcpy(&buf[260], &parameters, 2000);

                        Sleep(5);
                        res = send(sock, buf, sizeof(buf), 0);

                        //outputDebugInfo("Seding Join Channel msg:", prefix, command, parameters);

                        std::cout << "You are in the " << channel << " channel. (You can use !leave to leave the channel). Press space to start typing a message and press enter to send your message.\n ";
                        lastUpdate = time(NULL);
                        joinedChannel = true;
                    }
                }
                else
                {
                    //STATEFUL: Client is in chat server mode (DFA Node 6)

                    //disable timeout so we don't get stuck waiting for the server
                    DWORD timeout = SOCKET_READ_TIMEOUT_SEC * 1000;
                    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

                    //If user pressed the Enter key
                    if (GetKeyState(VK_SPACE) < 0)
                    {
                        std::cout << "[" << username << "]> ";
                        std::string message;
                        std::getline(std::cin, message);

                        if (message == "!quit" || message == " !quit")
                        {
                            //Quit the program
                            printf("\nThank you for using KaiChat! \n");

                            //Compose and send the Quit PDU
                            prefix = '\0';
                            char prefix_charArray[256];
                            strcpy_s(prefix_charArray, prefix.c_str());
                            command = QUIT;
                            parameters[0] = '\0';

                            memcpy(&buf[0], &prefix_charArray, 256);
                            memcpy(&buf[256], &command, 4);
                            memcpy(&buf[260], &parameters, 2000);

                            Sleep(5);
                            res = send(sock, buf, sizeof(buf), 0);


                            Sleep(40);
                            closesocket(client);
                            client = 0;
                            break;
                        }
                        else if (message == "!leave" || message == " !leave")
                        {
                            joinedChannel = false;
                            //Compose and send the Quit PDU
                            prefix = '\0';
                            char prefix_charArray[256];
                            strcpy_s(prefix_charArray, prefix.c_str());
                            command = LEAVE_CHANNEL;
                            parameters[0] = '\0';

                            memcpy(&buf[0], &prefix_charArray, 256);
                            memcpy(&buf[256], &command, 4);
                            memcpy(&buf[260], &parameters, 2000);

                            Sleep(5);
                            res = send(sock, buf, sizeof(buf), 0);


                            //re-enable timeout on sockets so we can recieve server commands with ease
                            struct timeval time_val_struct = { 0 };
                            time_val_struct.tv_sec = 0;
                            time_val_struct.tv_usec = 0;
                            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&time_val_struct, sizeof(time_val_struct));


                        }
                        else
                        {
                            //Compose and Send a SEND_MESSAGE PDU to the server
                            char prefix_charArray[256];
                            prefix_charArray[0] = '\0';
                            prefix[0] = '\0';
                            command = SEND_MESSAGE;
                            const char* msg_charArray = message.c_str();
                            //if the message is more than 2000 characters long, truncate it
                            int size = sizeof(msg_charArray) < 2000 ? 2000 : sizeof(msg_charArray);
                            memcpy(&parameters[0], &msg_charArray[0], size);

                            memcpy(&buf[0], &prefix_charArray, 256);
                            memcpy(&buf[256], &command, 4);
                            memcpy(&buf[260], &parameters[0], 2000);

                            Sleep(5);
                            res = send(sock, buf, sizeof(buf), 0);

                            //outputDebugInfo("Seding Send_msg pdu:", prefix_charArray, command, parameters);
                        }
                    }

                    //Send a query message to recieve new message updates
                    else if (lastUpdate + 3 < time(NULL))
                    //else if (GetKeyState(VK_RETURN) < 0)
                    {
                        //Compose and Send a QUERY PDU to the server
                        char prefix_charArray[256];
                        prefix_charArray[0] = '\0';
                        command = QUERY;
                        parameters[0] = '\0';

                        memcpy(&buf[0], &prefix_charArray, 256);
                        memcpy(&buf[256], &command, 4);
                        memcpy(&buf[260], &parameters, 2000);

                        Sleep(5);
                        res = send(sock, buf, sizeof(buf), 0);


                        //Check for a RECIEVE_MSG PDU from the server 
                        ret = recv(sock, RecvdData, sizeof(RecvdData), 0);
                        if (ret > 0)
                        {
                            int upToDateFlag = 0;
                            char prefix_charArray[256];
                            memcpy(prefix_charArray, &RecvdData[0], 256);
                            //Pull the upToDateFlag first
                            memcpy(&upToDateFlag, &prefix_charArray[51], 1);
                            prefix = addTerminator(prefix_charArray);
                            command = *(int*)&RecvdData[256];
                            memcpy(&parameters, &RecvdData[260], 2000);

                            if (command == RECIEVE_MESSAGE)
                            {
                                //If upToDateFlag is false, then we are not up-to-date because we don't have the message contained in this PDU.
                                if (!upToDateFlag)
                                {
                                    char username_charArray[10];
                                    char channel_charArray[50];
                                    memcpy(&username_charArray[0], &prefix_charArray[0], 10);
                                    memcpy(&channel_charArray[0], &prefix_charArray[11], 50);

                                    std::string msgChannel = addTerminator(channel_charArray);
                                    //If the channel associated with this chat message corresponds to our current channel:
                                    if (msgChannel == channel)
                                    {
                                        //Display it for the user
                                        std::cout << "[" << username_charArray << "]> " << parameters << "\n";
                                    }
                                }
                            }
                        }

                    }

                
                }
            }
            //Reseting PDU components after every PDU is sent
            prefix[0] = '\n';
            command = -1;
            parameters[0] = '\0'; //TODO: does this reset all the parameters?

        }

    }

    //Close the socket
    closesocket(client);
    //Calling WSACleanup becuase my program has finished using Winsock
    WSACleanup();
}
