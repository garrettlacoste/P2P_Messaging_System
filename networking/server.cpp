#include "network.h"

#include <WS2tcpip.h>
#include <codecvt>
#include <locale>

network::server::server(std::string IPAddr = SELF){
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
    serverSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(serverSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Binding\n";
    socketAddr.sin_port = htons(SERVER_PORT);
    retry:
    socketAddr.sin_family = AF_INET;
    InetPton(AF_INET,IPAddr.c_str(),&socketAddr.sin_addr.s_addr); //there's an eroneous error of "const char* is incompatiable with LPCWSTR", ignore it
    WSAError = bind(serverSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr));
    if(WSAError){
        if(socketAddr.sin_port != htons(BACKUP_SERVER_PORT)){
            std::cerr << "WARNING: Binding failed, attempting backup port\n";
            socketAddr.sin_port = htons(BACKUP_SERVER_PORT);
            goto retry;
        }

        cleanup();
        std::cerr << "ERROR: Error binding socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error binding socket");
    }
    int tempSize = sizeof(socketAddr);
    getsockname(serverSocket,(sockaddr*) &socketAddr,&tempSize);
    std::cout << "Server is at: " << inet_ntoa(socketAddr.sin_addr) << "\n";

    //Step 4
    putIntoListen();
}

//The returned SOCKET should be stored somewhere
SOCKET network::server::acceptConnection() const{
    //Step 5
    SOCKET otherParty;
    std::cout << "Waiting for connection (" << inet_ntoa(this->socketAddr.sin_addr) <<")...";

    otherParty = accept(serverSocket,NULL,NULL); //Blocks

    if (otherParty == INVALID_SOCKET){
        int error = WSAGetLastError();
        if(error == 10038){
            std::cout << "ERROR: Invalid server socket: " << inet_ntoa(socketAddr.sin_addr) << ":" << socketAddr.sin_port << "\n";
            throw std::runtime_error("Invalid server socket\n");
        }else{
            std::cout << "\nWARNING: Connection refused: " << WSAGetLastError() << "\n";
        }
    }else{
        std::cout << "\nConnection accepted\n";
    }
    return otherParty;
}

network::server::~server(){
    if(mainServer) cleanup();
}

void network::server::cleanup() const{
    if(serverSocket != INVALID_SOCKET) closesocket(this->serverSocket);
}

network::server::server(SOCKET connectedSocket){
    mainServer = false;
    int socketAddrLen = sizeof(socketAddr);
    getpeername(connectedSocket,(SOCKADDR*)&socketAddr,&socketAddrLen);
    serverSocket = connectedSocket;
    //WSAStartup(network::WINSOCK_VERSION_NEEDED,&requiredData);
}

std::string network::server::receiveMessage() const{
    int error;
    char buffer[BUFFER_SIZE];
    int num_recv = recv(serverSocket,buffer,BUFFER_SIZE,0);
    if(num_recv == SOCKET_ERROR && (error = WSAGetLastError())){
        switch (error){
            case WSAECONNRESET:
                std::cerr << "WARNING: Connection reset\n";
                break;
            case WSANOTINITIALISED:
                std::cerr << "ERROR: WSA not initialised\n";
                throw std::runtime_error("WSA not initialised");
            case WSAENOTSOCK:
                std::cerr << "ERROR: server is not a valid socket\n";
                throw std::runtime_error("Server is not a valid socket");
            default:
                std::cerr << "WARNING: Could not receive message: " << error << std::endl;
                break;
        }
        
        return std::string("");
    }else{
        buffer[num_recv] = '\0';

        // Generate random key and IV for decryption
        std::string key, iv;
        generateSecureKey(key, iv);  // Generate random key and IV

        // Decrypt the received message using the generated key and IV
        std::string decryptedMessage = decryptMessage(std::string(buffer), key, iv);  
        return decryptedMessage;
    }
}

std::string decryptMessage(const std::string& ciphertext, const std::string& key, const std::string& iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create decryption context");

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char*)key.c_str(), (unsigned char*)iv.c_str())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    std::string plaintext(ciphertext.size(), '\0');
    int len = 0, plaintext_len = 0;

    if (1 != EVP_DecryptUpdate(ctx, (unsigned char*)plaintext.data(), &len, (unsigned char*)ciphertext.c_str(), ciphertext.size())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption failed");
    }
    plaintext_len += len;

    if (1 != EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext.data() + plaintext_len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Final decryption step failed");
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintext_len);
    return plaintext;
}

network::server::server(const network::server& copyFrom){
    this->requiredData = copyFrom.requiredData;
    this->serverSocket = copyFrom.serverSocket;
    this->socketAddr = copyFrom.socketAddr;
}

network::server::server(){
    //Step 1
    std::cout << "Starting WSA\n";
    int WSAError = WSAStartup(WINSOCK_VERSION_NEEDED,&requiredData);
    if(WSAError){
        std::cerr << "ERROR: Winsock dll not found\n";
        throw std::runtime_error("Winsock dll not found");
    }else{
        std::cout << "Status: " << requiredData.szSystemStatus << std::endl; //print WSA status and flush
    }

    int getAddrInfoReturn;
    addrinfo* res = NULL;
    addrinfo hints;
    bool attempt = false;

    ZeroMemory(&hints,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    getAddrInfoReturn = getaddrinfo(NULL,std::to_string(SERVER_PORT).c_str(),&hints,&res);
    retry:
    if(getAddrInfoReturn){
        if(!attempt){
            std::cerr << "WARNING: Could not resolve server name: " << getAddrInfoReturn << "\n";
            getAddrInfoReturn = getaddrinfo(NULL,std::to_string(BACKUP_SERVER_PORT).c_str(),&hints,&res);
            attempt = !attempt;
            goto retry;
        }
        std::cerr << "ERROR: Failed to resolve server name: " << getAddrInfoReturn << std::endl;
        throw std::runtime_error("Failed to resolve server name");
    }
    sockaddr_in* temp = ((sockaddr_in*) res->ai_addr);
    std::cout << "Found " << inet_ntoa(temp->sin_addr) << ":" << (attempt ? BACKUP_SERVER_PORT : SERVER_PORT) << "\n";
    socketAddr = *((sockaddr_in*) res->ai_addr);

    //step 2
    std::cout << "Creating socket\n";
    serverSocket = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(serverSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Binding\n";
    WSAError = bind(serverSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr));
    if(WSAError){
        cleanup();
        std::cerr << "ERROR: Error binding socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error binding socket");
    }
    int tempSize = sizeof(socketAddr);
    getsockname(serverSocket,(sockaddr*) &socketAddr,&tempSize);
    std::cout << "Server is at: " << inet_ntoa(socketAddr.sin_addr) << "\n";

    //Step 4
    std::cout << "Listening\n";
    WSAError = listen(serverSocket,MAX_PARTICIPTANTS-1);
    if(WSAError){
        cleanup();
        std::cerr << "ERROR: Cannot listen: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Cannot listen");
    }
}

void network::server::putIntoListen() const{
    std::cout << "Listening\n";
    int WSAError = listen(serverSocket,MAX_PARTICIPTANTS-1);
    if(WSAError){
        WSAError = WSAGetLastError();
        cleanup();
        std::cerr << "ERROR: Cannot listen: " << WSAError << std::endl;
        throw std::runtime_error("Cannot listen");
    }
}

network::server::server(WSAData* wsaData){

    requiredData = *wsaData;
    std::cout << "Status: " << requiredData.szSystemStatus << std::endl; //print WSA status and flush

    int getAddrInfoReturn;
    addrinfo* res = NULL;
    addrinfo hints;
    bool attempt = false;

    ZeroMemory(&hints,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    getAddrInfoReturn = getaddrinfo(NULL,std::to_string(SERVER_PORT).c_str(),&hints,&res);
    retry:
    if(getAddrInfoReturn){
        if(!attempt){
            std::cerr << "WARNING: Could not resolve server name: " << getAddrInfoReturn << "\n";
            getAddrInfoReturn = getaddrinfo(NULL,std::to_string(BACKUP_SERVER_PORT).c_str(),&hints,&res);
            attempt = !attempt;
            goto retry;
        }
        std::cerr << "ERROR: Failed to resolve server name: " << getAddrInfoReturn << std::endl;
        throw std::runtime_error("Failed to resolve server name");
    }
    sockaddr_in* temp = ((sockaddr_in*) res->ai_addr);
    std::cout << "Found " << inet_ntoa(temp->sin_addr) << ":" << (attempt ? BACKUP_SERVER_PORT : SERVER_PORT) << "\n";
    socketAddr = *((sockaddr_in*) res->ai_addr);

    //step 2
    std::cout << "Creating socket\n";
    serverSocket = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(serverSocket==INVALID_SOCKET){
        cleanup();
        std::cerr << "ERROR: Error creating socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error creating socket");
    }

    //step 3
    std::cout << "Binding\n";
    int WSAError = bind(serverSocket,(SOCKADDR*)&socketAddr,sizeof(socketAddr));
    if(WSAError){
        cleanup();
        std::cerr << "ERROR: Error binding socket: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Error binding socket");
    }
    int tempSize = sizeof(socketAddr);
    getsockname(serverSocket,(sockaddr*) &socketAddr,&tempSize);
    std::cout << "Server is at: " << inet_ntoa(socketAddr.sin_addr) << "\n";

    //Step 4
    std::cout << "Listening\n";
    WSAError = listen(serverSocket,MAX_PARTICIPTANTS-1);
    if(WSAError){
        cleanup();
        std::cerr << "ERROR: Cannot listen: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("Cannot listen");
    }
}

int network::server::startWSA(WSAData* wsaData){
    requiredData = *wsaData;
    int startupError = WSAStartup(network::WINSOCK_VERSION_NEEDED,&(this->requiredData)); //crashes
    return startupError;
}