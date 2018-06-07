//
//  main.cpp
//
//
//  Created by Sam Prager on 2/9/15.
//
//

/*
 * Writes a known sequence of bytes then expects to read them back.
 * Run this with a loopback device fitted to one of FTDI's USB-RS232
 * converter cables.
 * A loopback device has:
 *   1.  Receive Data    connected to    Transmit Data
 *   2.  Data Set Ready  connected to    Data Terminal Ready
 *   3.  Ready To Send   connected to    Clear To Send
 *
 * Build with:
 *     gcc main.c -o loopback -Wall -Wextra -lftd2xx -Wl,-rpath /usr/local/lib
 *
 * Run with:
 *     sudo ./loopback
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
