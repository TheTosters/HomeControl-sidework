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
#include <string>
#include <vector>

class BTLEDevice {
    public:
        std::string address;
        std::string name;
        bool operator ==(const BTLEDevice& rhs);

        BTLEDevice(std::string address, std::string name);
        BTLEDevice(const BTLEDevice& source);
};

class HciWrapperListener {
    public:
        virtual ~HciWrapperListener() = default;
        virtual void onScanStart() = 0;
        virtual void onScanStop() = 0;
        virtual void onNewDeviceFound(const BTLEDevice& device) = 0;
};

class HciWrapper {
    public:
        HciWrapper(HciWrapperListener& delegate);
        ~HciWrapper();

        bool startScan();
        void scanLoop();
        void stopScan();
        void dumpError();
        void clearFoundDevices();
        std::vector<BTLEDevice> getFoundDevices();
        static void destroyAllConnections();
        static void restartBTLE();
    private:
        int device_id;
        int device_handle;
        struct hci_filter original_filter;
        int state;
        int has_error;
        char error_message[1024];
        std::vector<BTLEDevice>  foundDevices;
        HciWrapperListener& delegate;

        void open_default_hci_device();
        void close_hci_device();
        void scan_process_data(uint8_t *data, size_t data_len, le_advertising_info *info);
        void addToDevicePool(BTLEDevice& device);
};

#endif /* HciWrapper_hpp */
