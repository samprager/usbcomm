//
//  main.cpp
//
//
//  Created by Sam Prager on 6/5/19.
//
//

/*
 * Example program instantiating usbdev class. This class encapulates 2 way host-to-host USB communication using the D2XX FTDI driver to USB to TTL cables.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include "usbcomm.hpp"

int main(int argc, char *argv[]){
    uint32_t port = 0;
    uint32_t idnum = 0;
    uint32_t listener = 0;
    if (argc>1){
        port = atoi(argv[1]);
    }
    if (argc>2){
        idnum = atoi(argv[2]);
    }
    if (argc>3){
        listener = atoi(argv[3]);
    }

    usbcomm *usbd = new usbcomm(port);
    while (!usbd->ready()){
        usbd->reset(port);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (listener!=0){
        // Receive First
        std::cout<<"Runing as Listener"<<std::endl;
        int ind = 0;
        while (1){
            std::string rxmsg;
            std::string rxstatus;
            int err = usbd->receive(rxmsg,rxstatus,0,20000);
            if (err) std::cout<<"[main] Error. receive returned: "<<err<<std::endl;

            std::cout<<"[main] Received: " <<rxmsg <<". Status: "<<rxstatus<<std::endl<<std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));

            std::stringstream ss;
            ss << "id"<<idnum<<"says hello" << ind;
            std::string msg = ss.str();
            std::string resp;

            err = usbd->send(msg,resp,100);
            if (err) std::cout<<"[main] Error. send returned: "<<err<<std::endl;

            std::cout<<"[main] Sent: " <<msg <<". Response: "<<resp<<std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            ind++;
        }
    }
    else{
        std::cout<<"Runing as Commander"<<std::endl;
        // Send First
        int ind = 0;
        while(1){
            std::stringstream ss;
            ss << "id"<<idnum<<"says hello" << ind;
            std::string msg = ss.str();
            std::string resp;
            int err = usbd->send(msg,resp,100);

            if (err) std::cout<<"[main] Error. send returned : "<<err<<std::endl;

            std::cout<<"[main] Sent: " <<msg <<". Response: "<<resp<<std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            std::string rxmsg;
            std::string rxstatus;
            err = usbd->receive(rxmsg,rxstatus,0,20000);

            if (err) std::cout<<"[main] Error. receive returned: "<<err<<std::endl;

            std::cout<<"[main] Received: " <<rxmsg <<". Status: "<<rxstatus<<std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(5000));

            ind++;
        }
    }
    return 0;
}
