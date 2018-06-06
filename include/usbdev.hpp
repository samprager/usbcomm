#ifndef USBDEV_HPP
#define USBDEV_HPP

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <assert.h>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <functional>
#include <memory>

#include "ftd2xx.h"
// #include "mach/mach.h"
// #include "WinTypes.h"

class usbdev
{

public:
    usbdev(uint32_t port=0);
    ~usbdev();
    int init(uint32_t port=0);
    int reset(uint32_t port=0);
    int read(std::string &msg, uint32_t nbytes = 1, uint32_t max_checks=64000);
    int write(const std::string &msg, bool wait_resp = true);
    int listen(std::function<int(uint32_t)> callback, uint32_t nbytes=1, double timeout=-1.0);
    void stop_listening();
    void start_listening();
    int setbaud(unsigned long baudrate);
    void getinfo(std::string &str);
    bool listening();
    bool ready();
private:
    int listener_callback(uint32_t bytesReceived);
    int resp_callback(uint32_t bytesReceived);
    void start_listening_thread();
    void listen_watchdog();

    FT_HANDLE _ftHandle;
    unsigned long _baudrate=115200;
    unsigned long _max_attemps = 64000;
    std::string _default_response = "message received";
    std::mutex _listen_mtx;
    std::mutex _transaction_mtx;
    std::mutex _dev_mtx;
    // std::vector<std::thread> _rthreads;
    std::shared_ptr<std::thread> _rthread;
    std::shared_ptr<std::thread> _lwthread;
    bool _transaction_pending=false;
    bool _stop_listening=true;
    bool _listening = false;
    bool _dev_ready = false;
    bool _start_listening = false;
};


#endif  /* USBDEV_HPP */
