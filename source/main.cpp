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
#include "usbdev.hpp"

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

    usbdev *usbd = new usbdev(port);
    while (!usbd->ready()){
        usbd->reset(port);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (listener!=0){
        usbd->start_listening();
    }
    if(listener!=1){
    int ind = 0;
    while(1){
            std::stringstream ss;
            ss << "id"<<idnum<<"says hello" << ind;
            std::string msg = ss.str();
            int err = usbd->write(msg);
            if (err){
                std::cout<<"Error. write returned error code: "<<err<<std::endl;
            }
        ind++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    }
    else{
        while(1){

        }
    }
    return 0;
}
