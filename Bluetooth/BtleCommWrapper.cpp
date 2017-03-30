/*
 * BtleCommWrapper.cpp
 *
 *  Created on: Mar 9, 2017
 *      Author: Zarnowski
 */

#include "BtleCommWrapper.h"
#include <functional>
#include <exception>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>

extern "C" {
    #include "libgatt/att.h"
    #include "libgatt/gatt.h"
    #include "libgatt/bluetooth.h"
    #include "libgatt/gattrib.h"
}

class ReadSyncBlock {
    public:
        uint8_t*    result;
        uint16_t    resultSize;

        ReadSyncBlock(BtleCommWrapper* btleCommWrapper);
        ~ReadSyncBlock();

        void setReady();
        bool isReady();
    private:
        BtleCommWrapper* btleCommWrapper;
        bool             ready;  //write to it should be synchronized over btleCommWrapper->mutex
};

static const char* THREAD_NAME = "BtleCommWrapper";
//bulb BT
//static const char* CHAR_UUID = "0000ffe9-0000-1000-8000-00805f9b34fb";

//HM-10
static const char* CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";

static BtleCommWrapper* btleCommWrapper;

static gpointer threadMain(void *data) {
    GMainLoop* loop = static_cast<GMainLoop*>(data);
    g_main_loop_run(loop);

    return nullptr;
}

ReadSyncBlock::ReadSyncBlock(BtleCommWrapper* btleCommWrapper)
: result(nullptr), resultSize(0), btleCommWrapper(btleCommWrapper), ready(false) {

}

ReadSyncBlock::~ReadSyncBlock() {
    delete[] result;
    result = nullptr;
    btleCommWrapper = nullptr;
}

void ReadSyncBlock::setReady() {
    g_mutex_lock(&btleCommWrapper->mutex);
    ready = true;
    g_mutex_unlock(&btleCommWrapper->mutex);
}

bool ReadSyncBlock::isReady() {
    g_mutex_lock(&btleCommWrapper->mutex);
    bool tmp = ready;
    g_mutex_unlock(&btleCommWrapper->mutex);
    return tmp;
}

BtleCommWrapper::BtleCommWrapper()
: eventLoop(g_main_loop_new(nullptr, false)),
  eventLoopThread(g_thread_new(THREAD_NAME, &threadMain, eventLoop)),
  btleChannel(nullptr),
  btleAttribute(nullptr),
  btleValueHandle(0),
  connectingInProgress(false),
  notificationBuffer(make_shared<std::vector<uint8_t>>()),
  notificationReceived(false) {

    if(btleCommWrapper != nullptr) {
        throw std::logic_error("btleCommWrapper is already created!");
    }
    btleCommWrapper = this;
    g_mutex_init(&mutex);
}

BtleCommWrapper::~BtleCommWrapper() {
    g_main_loop_quit(eventLoop);
    g_main_loop_unref(eventLoop);
    g_thread_join(eventLoopThread); //this also do unref inside!
    g_mutex_clear(&mutex);

    btleCommWrapper = nullptr;
}

void BtleCommWrapper::setConnectingInProgress(bool state) {
    g_mutex_lock(&mutex);
    connectingInProgress = state;
    g_mutex_unlock(&mutex);
}

bool BtleCommWrapper::isConnectingInProgress() {
    g_mutex_lock(&mutex);
    bool tmp = connectingInProgress;
    g_mutex_unlock(&mutex);
    return tmp;
}

void BtleCommWrapper::notificationEventsHandler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
    GAttrib *attrib = (GAttrib *)user_data;
    uint8_t *opdu;
    uint16_t i, olen = 0;
    size_t plen;

    BtleCommWrapper* wrapper = (BtleCommWrapper*)user_data;
    //uint16_t handle = att_get_u16(&pdu[1]);

    switch (pdu[0]) {
        case ATT_OP_HANDLE_NOTIFY:
            {
                std::lock_guard<std::mutex> guard(wrapper->notificationMutex);
                for (i = 3; i < len; i++) {
                    wrapper->notificationBuffer->push_back(pdu[i]);
                }
                wrapper->notificationReceived = true;
            }
            break;
        case ATT_OP_HANDLE_IND:
            for (i = 3; i < len; i++) {
                g_info("%02x ", pdu[i]);
            }
            g_info("\n");
            break;
        default:
            g_info("Invalid opcode\n");
            return;
    }

    if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
        return;

    opdu = g_attrib_get_buffer(attrib, &plen);
    olen = enc_confirmation(opdu, plen);

    if (olen > 0)
        g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
}

void BtleCommWrapper::connectCallback(GIOChannel *io, GError *err, gpointer user_data) {
    if (err != nullptr) {
        //g_message("%s", err->message);
        g_warning("BTLE connection failed, reason: %s\n", err->message);
        g_error_free(err);
        btleCommWrapper->onConnectionFailed(io);

    } else {
        printf("BTLE connection established\n");
        btleCommWrapper->onConnectionSuccess(io);
    }
}

void BtleCommWrapper::onConnectionSuccess(GIOChannel* channel) {

    btleChannel = channel;  //store this object
    g_info("Discovering characteristic.\n");
    btleAttribute = g_attrib_new(channel);

    bt_uuid_t uuid;
    if (bt_string_to_uuid(&uuid, CHAR_UUID) < 0) {
        g_warning("BTLE Invalid UUID of characteristic.\n");
        setConnectingInProgress(false);
        return;
    }

    gatt_discover_char(btleAttribute, 0x0001, 0xffff, &uuid, BtleCommWrapper::discoverCharacteristicCallback, NULL);
}

void BtleCommWrapper::onConnectionFailed(GIOChannel* channel) {
    //this channel is no longer needed, destroy it
    g_io_channel_shutdown(channel, false, nullptr);
    g_io_channel_unref(channel);
    btleChannel = nullptr;
    setConnectingInProgress(false);
}

void BtleCommWrapper::discoverCharacteristicCallback(GSList *characteristics, uint8_t status, void *user_data) {
    if (status != 0) {
        g_warning("Discover all characteristics failed\n");
        btleCommWrapper->onDiscoveryFailed();

    } else {
        struct gatt_char *characteristic = (struct gatt_char *) characteristics->data;
        btleCommWrapper->onDiscoverySuccess( characteristic->value_handle );
    }
}

void BtleCommWrapper::onDiscoverySuccess(uint16_t handle) {
    btleCommWrapper->btleValueHandle = handle;

    g_attrib_register(btleAttribute, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
            BtleCommWrapper::notificationEventsHandler, this, NULL);
    g_attrib_register(btleAttribute, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
            BtleCommWrapper::notificationEventsHandler, this, NULL);

    setConnectingInProgress(false);
}

void BtleCommWrapper::onDiscoveryFailed() {
    btleCommWrapper->btleValueHandle = 0;

    //release attribute
    g_attrib_unref(btleAttribute);
    btleAttribute = nullptr;

    //release channel this will also set connectingInProgress to false
    onConnectionFailed(btleChannel);
}

bool BtleCommWrapper::connectTo(const string& address) {
    if (isConnectingInProgress() == true) {
        g_warning("Already in connecting state\n");
        return false;
    }
    setConnectingInProgress(true);

    GError* error;
    //NOTE: this channel will be stored to btleChannel in callback on success connection. Otherwise it will be destroyed
    //in onConnectionFailed, there is no need to assign gatt_connect to field btleChannel
    GIOChannel* tmp = gatt_connect("hci0", address.c_str(), "", "low", 0, 0, BtleCommWrapper::connectCallback, &error);
    if (tmp == nullptr) {
        g_warning("Failed to connect with error: %s", error->message);
        g_error_free(error);
        setConnectingInProgress(false);
        return false;
    }

    //wait until we connect
    g_info("Waiting for connection\n");
    while(true) {
        if (isConnectingInProgress() == false) {
            break;
        }
        usleep(30000);
    }

    return btleValueHandle != 0;
}

void BtleCommWrapper::disconnect() {
    if (btleAttribute != nullptr) {
        g_attrib_unref(btleAttribute);
        btleAttribute = nullptr;
    }
    if (btleChannel != nullptr) {
        g_io_channel_shutdown(btleChannel, false, nullptr);
        g_io_channel_unref(btleChannel);
        btleChannel = nullptr;
    }
}

void BtleCommWrapper::writeValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data) {
    ReadSyncBlock* result = static_cast<ReadSyncBlock*>(user_data);

    result->resultSize = status;

    if (status != 0) {
        g_warning("Characteristic Write Request failed: %s\n", att_ecode2str(status));
        result->setReady();
        return;
    }

    if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
        g_warning("Protocol error [write]\n");
        result->resultSize = 1;
        result->setReady();
        return;
    }

    result->setReady();
}

bool BtleCommWrapper::send(const string& dataToSend, int timeoutInMs) {
    if (btleAttribute == nullptr || btleValueHandle == 0) {
        printf("Not connected!");
        return false;
    }

    bool result = false;
    gsize plen =  dataToSend.length();
    uint8_t *value = (uint8_t *)g_try_malloc0(plen+1);
    strcpy((char*)value, dataToSend.c_str());
    if (plen != 0) {
        ReadSyncBlock data(this);

        gatt_write_char(btleAttribute, btleValueHandle, value, plen, BtleCommWrapper::writeValueCallback, &data);
        const int sleepTimeMs = 30;
        while (true) {
            if (data.isReady() == true || timeoutInMs <= 0) {
                break;
            }
            usleep(sleepTimeMs * 1000);
            timeoutInMs -= sleepTimeMs;
        }
        g_free(value);
        result = data.resultSize == 0;
    }
    return result;
}

void BtleCommWrapper::readValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data) {
    ReadSyncBlock* result = static_cast<ReadSyncBlock*>(user_data);

    if (plen <= 0 || status != 0) {
        g_warning("Characteristic value/descriptor read failed: %s\n", att_ecode2str(status));
        result->setReady();
        return;
    }

    uint8_t* value = new uint8_t[plen];
    size_t vlen = sizeof(value);
    result->resultSize = dec_read_resp(pdu, plen, value, vlen);
    result->result = value; //we taking ownership of this memory

    if (vlen <= 0) {
        g_warning("Protocol error, vLen <= 0\n");
    }
    result->setReady();
}

bool isItEnter(int i) {
  return i == 13;
}

string BtleCommWrapper::readLineFromNotificationBuffer(int timeoutInMs) {
    string result;
    const int sleepTimeMs = 30;
    while(true)
    {
        {//critical section
            std::lock_guard<std::mutex> guard(notificationMutex);
            if (notificationReceived == false) {
                continue;
            }
            std::vector<uint8_t>::iterator it = std::find_if(notificationBuffer->begin(), notificationBuffer->end(),
                    isItEnter);
            if (it == notificationBuffer->end()) {
                continue;
            }
            it++;
            uint8_t* startAddress = &((*notificationBuffer)[0]);
            size_t size = it - notificationBuffer->begin();
            result.append((const char*)startAddress, size);
            notificationBuffer->erase(notificationBuffer->begin(), it);
            break;
        }
        usleep(sleepTimeMs * 1000);
        timeoutInMs -= sleepTimeMs;
    }
    return result;
}

string BtleCommWrapper::readLine(int timeoutInMs, bool useNotifications) {
    if (useNotifications == true) {
        return readLineFromNotificationBuffer(timeoutInMs);
    }
    ReadSyncBlock data(this);

    //TODO: How to cancel this process in case of timeout?
    gatt_read_char(btleAttribute, btleValueHandle, BtleCommWrapper::readValueCallback, &data);
    const int sleepTimeMs = 30;
    while(true) {
        if (data.isReady() == true || timeoutInMs <= 0) {
            break;
        }
        usleep(sleepTimeMs * 1000);
        timeoutInMs -= sleepTimeMs;
    }

    return data.resultSize == 0 ? "" : string((char*)data.result, 0, data.resultSize);
}
