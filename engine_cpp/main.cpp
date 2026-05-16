#include <chrono>
#include <cstddef>
#include <iostream>
#include <stop_token>
#include <thread>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstring>
#include "ThreadPool.hpp"

int create_and_bind_socket(int port){
    //create a TCP socket
    int listen_fd=socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd==-1) return -1;

    //allow immediate reuse of the port if engine restarted quickly
    int opt=1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT,&opt,sizeof(opt));

    //bind socket to the port
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(port);
    
    if(bind(listen_fd,(struct sockaddr*)&addr,sizeof(addr))==-1) return -1;
    if(listen(listen_fd,SOMAXCONN)==-1) return -1;

    //non-blocking
    int flags=fcntl(listen_fd,F_GETFL,0);
    fcntl(listen_fd,F_SETFL,flags | O_NONBLOCK);

    return listen_fd;
}

class AsyncEventLoop{
private:
    int epoll_fd;
    int server_socket;
    const int MAX_EVENTS=64;

public:
    AsyncEventLoop(){
        epoll_fd=epoll_create1(0);
        server_socket=create_and_bind_socket(8080); 
        if(server_socket==-1) {
            std::cerr<<"failed to open port 8080"<<std::endl;
        }

        struct epoll_event event;
        event.data.fd=server_socket; 
        event.events=EPOLLIN; 
        epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_socket, &event);
        std::cout<<"port 8080"<<std::endl;
    }
    ~AsyncEventLoop() 
    {
        close(epoll_fd); 
        close(server_socket); //shutting down
    }
    //we now pass a reference to the ThreadPool (&pool) into the run loop
    void run(std::stop_token stopToken,ThreadPool& pool){
        std::vector<struct epoll_event> events(MAX_EVENTS);
        std::cout<<"waiting for network traffic"<<std::endl;

        while(!stopToken.stop_requested()){
            int num_ready=epoll_wait(epoll_fd,events.data(),MAX_EVENTS,100);
            if(num_ready==0) continue; 

            for(int i=0;i<num_ready;i++){
                int active_fd=events[i].data.fd;
                if (active_fd==server_socket){
                    int client_fd=accept(server_socket,nullptr,nullptr);
                    
                    struct epoll_event client_event;
                    client_event.data.fd=client_fd;
                    client_event.events=EPOLLIN; 
                    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&client_event);
                }
                else{
                    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,active_fd,nullptr);

                    pool.submit([active_fd]() {
                        char buffer[1024] = {0};
                        
                        //read what is typed
                        int bytes_read = read(active_fd, buffer, sizeof(buffer));
                        
                        if (bytes_read > 0) {
                            // Print it to the server console
                            std::cout << "Customer said: " << buffer;
                            
                            std::string response = "Engine Echo: " + std::string(buffer);
                            write(active_fd, response.c_str(), response.length());
                        }
                        
                        //close connection
                        close(active_fd);
                    });
                }
            }
        }
    }
};

int main(){
    ThreadPool threadPool{std::thread::hardware_concurrency()};
    AsyncEventLoop eventLoop;

    std::jthread loopThread([&](std::stop_token stopToken){
        eventLoop.run(stopToken,threadPool);
    });

    threadPool.submit([]{
        std::cout<<"load generator initialized\n";
    });
    while(true){
        std::this_thread::sleep_for(std::chrono::hours(1)); 
    }
    return 0;
}