/*
 * HciWrapper.hpp
 *
 *  Created on: Feb 23, 2017
 *      Author: Zarnowski
 */

#ifndef HciWrapper_hpp
#define HciWrapper_hpp

#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

class HciWrapper {
    public:
        HciWrapper();
        ~HciWrapper();

        bool startScan();
        void scanLoop();
        void stopScan();
        void dumpError();
    private:
        int device_id;
        int device_handle;
        struct hci_filter original_filter;
        int state;
        int has_error;
        char error_message[1024];

        void open_default_hci_device();
        void close_hci_device();
        void scan_process_data(uint8_t *data, size_t data_len, le_advertising_info *info);
};

#endif /* HciWrapper_hpp */
