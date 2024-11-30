
#ifndef NETWORKING_H
#define NETWORKING_H

/*#define ENCRYPTION_KEY "ThisIsAnEncryptionKey123456789" // Replace with securely generated key and DO NOT put the generated key in the repo
#define ENCRYPTION_IV "YourInitVector1234"         // Replace with securely generated IV and DO NOT put the generated IV in the repo*/

#include <WinSock2.h>
#include <iostream>
#include <exception>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

namespace network{
    const static std::string SELF = "127.0.0.1";
    const int SERVER_PORT = 60800;
    const int BACKUP_SERVER_PORT = 60801;
    const WORD WINSOCK_VERSION_NEEDED = WINSOCK_VERSION;
    const int MAX_PARTICIPTANTS = 2; //minimum of 2 
    const int BUFFER_SIZE = 1024;
    const int MAX_MESSAGE_LEN = BUFFER_SIZE;

    extern void generateSecureKey(std::string& key, std::string& iv);

    //these needed to be extern so they wouldn't be defined multiple times
    extern std::string encryptMessage(const std::string& plaintext, const std::string& key, const std::string& iv);

    extern std::string decryptMessage(const std::string& ciphertext, const std::string& key, const std::string& iv);

    class server{
        sockaddr_in socketAddr;
        SOCKET serverSocket = INVALID_SOCKET;
        bool mainServer = true;

        void cleanup() const;
        /*
        1) Init WSA DLL - WSAStartup()
        2) create a socket - socket()
        3) bind the socket - bind()
        4) Listen on the socket - listen()
        5) Accept a connection - accept(),connect()
        6) send and receive data - rect(),send(),recvfrom(),sendto()
        7) Disconnect - closesocket()
        */
        public:
            WSADATA requiredData;

            server(std::string );
            server(const server&);
            server();
            server(SOCKET);
            server(WSAData* );
            ~server();

            SOCKET acceptConnection() const;
            std::string receiveMessage() const;
            void putIntoListen() const;
            int startWSA(WSAData* );
    };

    class client{
        sockaddr_in socketAddr;
        SOCKET clientSocket = INVALID_SOCKET;

        void cleanup();
        /*
        1) init WSA DLL - WSAStartup()
        2) create a socket - socket()
        3) connect to server - connect()
        4) Send and receive data - recv(),send(),recvfrom(),sendto()
        5) Disconnect - closesocket()
        */
        public:
            WSADATA requiredData;

            client(std::string );
            client(std::string, WSAData* );
            client(WSAData* );
            client(const client& );
            client();
            ~client();
            bool sendMessage(std::string);
            bool sendFile(/*file goes here*/);
    };
};

#endif