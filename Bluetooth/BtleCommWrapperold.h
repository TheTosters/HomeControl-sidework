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

enum BtleCommWrapperConnectionState {
  bcwcsNone,
  bcwcsConnecting,
  bcwcsConnected,
  bcwcsFailedToConnect,
};

class BtleCommWrapper_old {
    public:
        GMutex          mutex;

        BtleCommWrapper_old();
        virtual ~BtleCommWrapper_old();

        bool    connectTo(const string& address, int timeoutInMs);
        void    disconnect();
        bool    send(const string& data, int timeoutInMs = 3000);
        string  readLine(int timeoutInMs = 3000, bool useNotifications = false);
        bool    isConnected();
        BtleCommWrapperConnectionState getState();
    private:
        GMainLoop*      eventLoop;
        GThread*        eventLoopThread;
        GIOChannel*     btleChannel;
        GAttrib*        btleAttribute;
        guint16         btleValueHandle;
        std::mutex      notificationMutex;
        bool            notificationReceived;
        BtleCommWrapperConnectionState state;
        std::shared_ptr<std::vector<uint8_t>> notificationBuffer;

        void onConnectionSuccess(GIOChannel* channel);
        void onConnectionFailed(GIOChannel* channel, gint errorCode);
        void onDiscoverySuccess(uint16_t handle);
        void onDiscoveryFailed(gint errorCode);

        void setState(BtleCommWrapperConnectionState state);
        bool isConnectingInProgress();
        string readLineFromNotificationBuffer(int timeoutInMs);
        static void connectCallback(GIOChannel *io, GError *err, gpointer user_data);
        static void discoverCharacteristicCallback(GSList *characteristics, uint8_t status, void *user_data);
        static void readValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data);
        static void writeValueCallback(guint8 status, const guint8 *pdu, guint16 plen, gpointer user_data);
        static void notificationEventsHandler(const uint8_t *pdu, uint16_t len, gpointer user_data);

        bool innerConnectTo(const string& address, int timeoutInMs = 3000);
};

#endif /* BtleCommWrapper_hpp */
