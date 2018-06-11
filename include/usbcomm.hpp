#ifndef USBCOMM_HPP
#define USBCOMM_HPP

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
#include <queue>
#include <iomanip>

#include "ftd2xx.h"
// #include "mach/mach.h"
// #include "WinTypes.h"

struct rx_buf{
  std::queue<std::string> data;
  std::mutex mtx;
};


class usbcomm
{

public:
    usbcomm(uint32_t port=0);
    ~usbcomm();
    int init(uint32_t port=0);
    int reset(uint32_t port=0);
    int read(std::string &msg,std::string &status, uint32_t nbytes=0, double timeout=-1.0);
    int write(const std::string &msg, std::string &status);
    int flush(std::string &status);
    int receive(std::string &msg,std::string &status, uint32_t nbytes = 1, double timeout=-1.0);
    int send(const std::string &msg,std::string &resp, double timeout=-1.0);


    int setbaud(unsigned long baudrate);
    void getinfo(std::string &str);
    bool ready();
private:
    bool responsevalid(std::string &resp, uint16_t &id);
    bool messagevalid(std::string &msg, uint16_t &id);
    std::string makeresponse(const std::string & msg, uint16_t id=0);
    std::string makemessage(const std::string & msg, uint16_t id = 0);
    FT_HANDLE _ftHandle;
    unsigned long _baudrate=115200;
    unsigned long _max_attemps = 64000;
    std::string _default_response = "okay";
    std::mutex _transaction_mtx;
    std::mutex _dev_mtx;
    bool _transaction_pending=false;
    uint8_t _transaction_id = 0;
    bool _listening = false;
    bool _dev_ready = false;
    std::shared_ptr<struct rx_buf> _rxbuf;
};


#endif  /* USBCOMM_HPP */
