//system includes
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

//local includes
#include "networking/network.h"

//helper function prototypes
void acceptConnections(network::server*,std::vector<network::server*>&);
network::server* getServer(std::vector<network::server*>& );
void addConnection(std::vector<network::server*>&,network::server*);

int main(int argc,char** argv){
    omp_set_num_teams(1+2+2); //1 Master thread, 1 client and 1 server thread, 1 application thread
    int num_threads;
    std::vector<network::server*> connections = std::vector<network::server*>(); //has race conditions
    std::string connectionIP = network::SELF;
    #ifdef _OPENMP
    num_threads = network::MAX_PARTICIPTANTS;
    #else
    num_threads = 1;
    #endif
    //if(!argc-1){
    //    std::cout << "Please enter IP for local server (blank for self): ";
    //    std::getline(std::cin,temp);
    //    if (temp != "") connectionIP = temp;
    //}else{
    //    connectionIP = argv[1];
    //}
    //int threadID;
    WSAData requiredData;
    //uint8_t active = 0;
    WSAStartup(network::WINSOCK_VERSION_NEEDED,&(requiredData));

    network::server testServer = network::server(&requiredData);
    
    #pragma omp parallel num_threads(num_threads*2+1) private(connectionIP/*,threadID*/)
    {
        connectionIP = network::SELF;
        //threadID = omp_get_thread_num();

        #pragma omp single
        { //if master thread, aka the server or 1 thread
            for(;;){ //main application loop, master should be stuck here

                #pragma omp task //spin off task to create client before blocking to accept connections
                { //this code is NOT executed by the master thread
                    std::cout << "Please enter the connection string (blank for self): ";
                    std::getline(std::cin,connectionIP);
                    network::client* localClient; //if this isn't a pointer it's a different client than the one connected to the server
                    #pragma omp critical
                    {
                        if(connectionIP != "") localClient = new network::client(connectionIP);
                        else localClient = new network::client();
                    }
                    std::string message = "";
                    do{
                        std::getline(std::cin,message);
                    }while(message != "" && localClient->sendMessage(message));
                    std::cout << "Closing client\n";
                    localClient->~client();
                }

                if(connections.size() < (network::MAX_PARTICIPTANTS - 1)){
                    acceptConnections(&testServer,connections);
                    testServer.putIntoListen();
                }

                //connections is modified in emutls_get_address, which I don't have on my PC so idk where the fuck it's coming from
                //The above FIXME is resolved when compiling with optimization set to 03
                #pragma omp task default(none) shared(requiredData,std::cout,connections) //spin off server task once connection is accepted
                {
                    network::server* localServer = getServer(connections);
                    //localServer->startWSA(&requiredData);
                    
                    std::cout << "Connection received:" << requiredData.szSystemStatus << "\n";
                    if(localServer != nullptr){
                        std::string message = "";
                        do{
                            message = localServer->receiveMessage();
                            std::cout << message << "\n";
                        }while(message != "");
                        delete localServer;
                    }
                }
            }
        }
        //other threads at rest here, until assigned a task
    }
    WSACleanup();
}

//helper function implementation
void acceptConnections(network::server* server,std::vector<network::server*>& connections){
    network::server* temp = new network::server(server->acceptConnection());
    #pragma omp critical //so two things don't get pushed at the same time and both end up frankenstein'd
    {
        connections.push_back(temp);
    }
}

void addConnection(std::vector<network::server*>& connections,network::server* server){
    #pragma omp critical
    {
        connections.push_back(server);
    }
}

network::server* getServer(std::vector<network::server*>& availableConnections){
    if(availableConnections.empty()) return nullptr;
    std::vector<network::server*>::iterator temp = availableConnections.end() - 1;
    #pragma omp critical
    {
        //temp = availableConnections.end() - 1; //vector.end points 1 past the end, so subtract 1 to get the last element
        availableConnections.erase(temp);
    }
    return *temp;
}