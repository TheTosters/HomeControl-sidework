/*
 * HciWrapper.cpp
 *
 *  Created on: Feb 23, 2017
 *      Author: Zarnowski
 */
#include "HciWrapper.hpp"
#include <system_error>
#include <algorithm>

#define HCI_STATE_NONE       0
#define HCI_STATE_OPEN       2
#define HCI_STATE_SCANNING   3
#define HCI_STATE_FILTERING  4

#define EIR_FLAGS                   0X01
#define EIR_NAME_SHORT              0x08
#define EIR_NAME_COMPLETE           0x09
#define EIR_MANUFACTURE_SPECIFIC    0xFF

BTLEDevice::BTLEDevice(std::string address, std::string name)
: address(address), name(name) {

}
BTLEDevice::BTLEDevice(const BTLEDevice& source)
: address(source.address), name(source.name) {

}

bool BTLEDevice::operator ==(const BTLEDevice& rhs) {
    return address == rhs.address;
}

HciWrapper::HciWrapper(HciWrapperListener& delegate)
: device_id(0), device_handle(0), state(0), has_error(0), delegate(delegate) {
    open_default_hci_device();
    if (has_error) {
        printf("ERROR: %s\n", error_message);
        throw std::system_error();
    }
}

HciWrapper::~HciWrapper() {
    close_hci_device();
}

void HciWrapper::dumpError() {
    if (has_error == TRUE) {
        printf("%s\n", error_message);
    }
}
bool HciWrapper::startScan() {
    if (hci_le_set_scan_parameters(device_handle, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0x00, 1000) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message), "Failed to set scan parameters: %s", strerror(errno));
        return false;
    }

    if (hci_le_set_scan_enable(device_handle, 0x01, 1, 1000) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message), "Failed to enable scan: %s", strerror(errno));
        return false;
    }

    state = HCI_STATE_SCANNING;

    // Save the current HCI filter
    socklen_t olen = sizeof(original_filter);
    if (getsockopt(device_handle, SOL_HCI, HCI_FILTER, &original_filter, &olen) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message), "Could not get socket options: %s", strerror(errno));
        return false;
    }

    // Create and set the new filter
    struct hci_filter new_filter;

    hci_filter_clear(&new_filter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
    hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);

    if (setsockopt(device_handle, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message), "Could not set socket options: %s", strerror(errno));
        return false;
    }

    state = HCI_STATE_FILTERING;

    delegate.onScanStart();

    return true;
}

void HciWrapper::scanLoop() {
    //printf("Begin of scan loop!\n");
    bool error = false;
    int counter = 10000;
    while (error != true && counter > 0) {
        counter --;
        int len = 0;
        unsigned char buf[HCI_MAX_EVENT_SIZE];
        while ((len = read(device_handle, buf, sizeof(buf))) < 0) {
            counter--;
            if (counter < 0) {
                //printf("End of scan loop!\n");
                return;
            }
            if (errno == EINTR) {
                return;
            }
            if (errno == EAGAIN) {
                usleep(100);
                continue;
            }
           return;
        }

        evt_le_meta_event *meta = (evt_le_meta_event *) (buf + (1 + HCI_EVENT_HDR_SIZE));
        len -= (1 + HCI_EVENT_HDR_SIZE);

        if (meta->subevent != EVT_LE_ADVERTISING_REPORT) {
            continue;
        }

        le_advertising_info *info = (le_advertising_info *) (meta->data + 1);

//        printf("Event: %d\n", info->evt_type);
//        printf("Length: %d\n", info->length);

        if (info->length == 0) {
            continue;
        }

        int current_index = 0;
        int data_error = 0;

        while (!data_error && current_index < info->length) {
            size_t data_len = info->data[current_index];

            if (data_len + 1 > info->length) {
                printf("EIR data length is longer than EIR packet length. %d + 1 > %d", data_len, info->length);
                data_error = 1;
            } else {
                scan_process_data(info->data + current_index + 1, data_len, info);
                //get_rssi(&info->bdaddr, current_hci_state);
                current_index += data_len + 1;
            }
        }
    }
//    printf("End of scan loop!\n");
}

void HciWrapper::addToDevicePool(BTLEDevice& device) {
    if (std::find(foundDevices.begin(), foundDevices.end(), device) != foundDevices.end() ) {
        return;
    }
    foundDevices.push_back(device);
    delegate.onNewDeviceFound(device);
}

void HciWrapper::scan_process_data(uint8_t *data, size_t data_len, le_advertising_info *info) {
    if (data[0] == EIR_NAME_SHORT || data[0] == EIR_NAME_COMPLETE) {
        size_t name_len = data_len;
        char *name = (char*) malloc(name_len + 1);
        memset(name, 0, name_len + 1);
        memcpy(name, &data[1], name_len);

        char addr[19];
        ba2str(&info->bdaddr, addr);
        addr[18] = 0;

        BTLEDevice device(addr, name);
        addToDevicePool(device);

        free(name);
    } /* not needed now
      else if (data[0] == EIR_FLAGS) {
        printf("Flag type: len=%d\n", data_len);
        unsigned int i;
        for (i = 1; i < data_len; i++) {
            printf("\tFlag data: 0x%0X\n", data[i]);
        }
    } else if (data[0] == EIR_MANUFACTURE_SPECIFIC) {
        printf("Manufacture specific type: len=%d\n", data_len);

        // TODO int company_id = data[current_index + 2]

        unsigned int i;
        for (i = 1; i < data_len; i++) {
            printf("\tData: 0x%0X\n", data[i]);
        }
    } else {
        printf("Unknown type: type=%X\n", data[0]);
    } */

}

void HciWrapper::stopScan() {
    if (state == HCI_STATE_FILTERING) {
        state = HCI_STATE_SCANNING;
        setsockopt(device_handle, SOL_HCI, HCI_FILTER, &original_filter, sizeof(original_filter));
    }

    if (hci_le_set_scan_enable(device_handle, 0x00, 1, 1000) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message), "Disable scan failed: %s", strerror(errno));
    }

    state = HCI_STATE_OPEN;
    delegate.onScanStop();
}

void HciWrapper::open_default_hci_device() {
    device_id = hci_get_route(NULL);

    if ((device_handle = hci_open_dev(device_id)) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message), "Could not open device: %s", strerror(errno));
        return;
    }

    // Set fd non-blocking
    int on = 1;
    if (ioctl(device_handle, FIONBIO, (char *) &on) < 0) {
        has_error = TRUE;
        snprintf(error_message, sizeof(error_message),
                "Could set device to non-blocking: %s", strerror(errno));
        return;
    }

    state = HCI_STATE_OPEN;
}

void HciWrapper::close_hci_device()
{
  if(state == HCI_STATE_OPEN) {
    hci_close_dev(device_handle);
  }
}

void HciWrapper::clearFoundDevices() {
    foundDevices.clear();
}

std::vector<BTLEDevice> HciWrapper::getFoundDevices() {
    return std::vector<BTLEDevice>(foundDevices);
}
