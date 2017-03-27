/*
 * BtleCommWrapper.h
 *
 *  Created on: Mar 9, 2017
 *      Author: Zarnowski
 */

#ifndef BtleCommWrapper_hpp
#define BtleCommWrapper_hpp
extern "C" {
    #include "glib-2.0/glib.h"
    #include "libgatt/gattrib.h"
}
#include <string>
#include <vector>
#include <memory>
#include <mutex>

using namespace std;

class BtleCommWrapper {
    public:
        GMutex          mutex;

        BtleCommWrapper();
        virtual ~BtleCommWrapper();

        bool    connectTo(const string& address);
        void    disconnect();
        bool    send(const string& data, int timeoutInMs = 3000);
        string  readLine(int timeoutInMs = 3000, bool useNotifications = false);
    private:
        GMainLoop*      eventLoop;
        GThread*        eventLoopThread;
        GIOChannel*     btleChannel;
        GAttrib*        btleAttribute;
        guint16         btleValueHandle;
        bool            connectingInProgress;
        std::shared_ptr<std::vector<uint8_t>> notificationBuffer;
        std::mutex      notificationMutex;
        bool            notificationReceived;

        void onConnectionSuccess(GIOChannel* channel);
        void onConnectionFailed(GIOChannel* channel);
        void onDiscoverySuccess(uint16_t handle);
        void onDiscoveryFailed();

        void setConnectingInProgress(bool state);
        bool isConnectingInProgress();
        string readLineFromNotificationBuffer(int timeoutInMs);
        static void connectCallback(GIOChannel *io, GError *err, gpointer user_data);
        static void discoverCharacteristicCallback(GSList *characteristics, uint8_t status, void *user_data);
        static void readValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data);
        static void writeValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data);
        static void notificationEventsHandler(const uint8_t *pdu, uint16_t len, gpointer user_data);
};

#endif /* BtleCommWrapper_hpp */
