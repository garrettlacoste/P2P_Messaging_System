#include "network.h"

#include <WS2tcpip.h>
#include <codecvt>
#include <locale>

using convert_type = std::codecvt_utf8<wchar_t>;

std::wstring_convert<convert_type, wchar_t> converter;

network::client::client(std::string ipAddr = SELF){
    if(ipAddr == "") throw std::logic_error("No IP provided");
    //Step 1
    std::cout << "Starting WSA\n";
    int WSAError = WSAStartup(WINSOCK_VERSION_NEEDED,&requiredData);
    if(WSAError){
        std::cerr << "ERROR: Winsock dll not found\n";
        throw std::runtime_error("Winsock dll not found");
    }else{
        std::cout << "Status: " << requiredData.szSystemStatus << std::endl; //print WSA status and flush
    }

    //step 2
    std::cout << "Creating socket\n";
    clientSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(clientSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Connecting...";
    socketAddr.sin_port = htons(SERVER_PORT);
    retry:
    socketAddr.sin_family = AF_INET;
    InetPton(AF_INET,ipAddr.c_str(),&socketAddr.sin_addr.s_addr); //there's an eroneous error of "const char* is incompatiable with LPCWSTR", ignore it
    int error = connect(clientSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr));
    if(error == SOCKET_ERROR && (error = WSAGetLastError())){
        if(socketAddr.sin_port != htons(BACKUP_SERVER_PORT)){
            std::cerr << "\nWARNING: Connecting failed(" << error <<")\nIP: " << ipAddr << ":"<< SERVER_PORT <<"\nAttempting backup port\n";
            socketAddr.sin_port = htons(BACKUP_SERVER_PORT);
            goto retry;
        }
        cleanup();
        std::cerr << "ERROR: Error connecting to " << ipAddr << ":" << BACKUP_SERVER_PORT << " with error: " << error << std::endl;
        throw std::runtime_error("Error connecting to server");
    }
    std::cout << "Connection established\n";
}

network::client::client(){
    std::string ipAddr = SELF;
    //Step 1
    std::cout << "Starting WSA\n";
    int WSAError = WSAStartup(WINSOCK_VERSION_NEEDED,&requiredData);
    if(WSAError){
        std::cerr << "ERROR: Winsock dll not found\n";
        throw std::runtime_error("Winsock dll not found");
    }else{
        std::cout << "Status: " << requiredData.szSystemStatus << std::endl; //print WSA status and flush
    }

    //step 2
    std::cout << "Creating socket\n";
    clientSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(clientSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Connecting...";
    socketAddr.sin_port = htons(SERVER_PORT);
    retry:
    socketAddr.sin_family = AF_INET;
    InetPton(AF_INET,ipAddr.c_str(),&socketAddr.sin_addr.s_addr); //there's an eroneous error of "const char* is incompatiable with LPCWSTR", ignore it
    int error;
    if(connect(clientSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr)) == SOCKET_ERROR && (error = WSAGetLastError())){
        if(socketAddr.sin_port != htons(BACKUP_SERVER_PORT)){
            std::cerr << "\nWARNING: Connecting failed(" << error <<")\nIP: " << ipAddr << ":"<< SERVER_PORT <<"\nAttempting backup port\n";
            socketAddr.sin_port = htons(BACKUP_SERVER_PORT);
            goto retry;
        }
        cleanup();
        std::cerr << "ERROR: Error connecting to " << ipAddr << ":" << BACKUP_SERVER_PORT << " with error: " << error << std::endl;
        throw std::runtime_error("Error connecting to server");
    }
    std::cout << "Connection established\n";
}

void network::client::cleanup(){
    if(clientSocket != INVALID_SOCKET) closesocket(this->clientSocket);
}

network::client::~client(){
    cleanup();
}

bool network::client::sendMessage(std::string message){
    // Generate random key and IV for encryption
    std::string key, iv;
    generateSecureKey(key, iv);  // Generate random key and IV

    // Encrypt the message using the generated key and IV
    std::string encryptedMessage = encryptMessage(message, key, iv);
    int error;
    error = send(clientSocket,encryptedMessage.c_str(),encryptedMessage.length(),0);
    if((error == SOCKET_ERROR) && (error = WSAGetLastError())){
        std::cerr << "WARNING: Error sending message to " << inet_ntoa(socketAddr.sin_addr) << ": " << error << std::endl;
        return false;
    }
    return true;
}

network::client::client(WSAData* wsaData){
    std::string ipAddr = SELF;
    requiredData = *wsaData;
    std::cout << "Status: " << requiredData.szSystemStatus << std::endl; //print WSA status and flush

    //step 2
    std::cout << "Creating socket\n";
    clientSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(clientSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Connecting...";
    socketAddr.sin_port = htons(SERVER_PORT);
    retry:
    socketAddr.sin_family = AF_INET;
    InetPton(AF_INET,ipAddr.c_str(),&socketAddr.sin_addr.s_addr); //there's an eroneous error of "const char* is incompatiable with LPCWSTR", ignore it
    int error;
    if(connect(clientSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr)) == SOCKET_ERROR && (error = WSAGetLastError())){
        if(socketAddr.sin_port != htons(BACKUP_SERVER_PORT)){
            std::cerr << "\nWARNING: Connecting failed(" << error <<")\nIP: " << ipAddr << ":"<< SERVER_PORT <<"\nAttempting backup port\n";
            socketAddr.sin_port = htons(BACKUP_SERVER_PORT);
            goto retry;
        }
        cleanup();
        std::cerr << "ERROR: Error connecting to " << ipAddr << ":" << BACKUP_SERVER_PORT << " with error: " << error << std::endl;
        throw std::runtime_error("Error connecting to server");
    }
    std::cout << "Connection established\n";
}

network::client::client(std::string ipAddr = SELF, WSAData* wsaData = nullptr){
    if(ipAddr == "") throw std::logic_error("No IP provided");
    //Step 1
    requiredData = *wsaData;
    std::cout << "Status: " << requiredData.szSystemStatus << std::endl; //print WSA status and flush

    //step 2
    std::cout << "Creating socket\n";
    clientSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(clientSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Connecting...";
    socketAddr.sin_port = htons(SERVER_PORT);
    retry:
    socketAddr.sin_family = AF_INET;
    InetPton(AF_INET,ipAddr.c_str(),&socketAddr.sin_addr.s_addr); //there's an eroneous error of "const char* is incompatiable with LPCWSTR", ignore it
    int error;
    if(connect(clientSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr)) == SOCKET_ERROR && (error = WSAGetLastError())){
        if(socketAddr.sin_port != htons(BACKUP_SERVER_PORT)){
            std::cerr << "\nWARNING: Connecting failed(" << error <<")\nIP: " << ipAddr << ":"<< SERVER_PORT <<"\nAttempting backup port\n";
            socketAddr.sin_port = htons(BACKUP_SERVER_PORT);
            goto retry;
        }
        cleanup();
        std::cerr << "ERROR: Error connecting to " << ipAddr << ":" << BACKUP_SERVER_PORT << " with error: " << error << std::endl;
        throw std::runtime_error("Error connecting to server");
    }
    std::cout << "Connection established\n";
}