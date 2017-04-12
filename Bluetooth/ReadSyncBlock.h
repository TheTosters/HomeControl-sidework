/*
 * ReadSyncBlock.h
 *
 *  Created on: Apr 11, 2017
 *      Author: Zarnowski
 */

#ifndef ReadSyncBlock_hpp
#define ReadSyncBlock_hpp

extern "C" {
    #include "glib-2.0/glib.h"
    #include "libgatt/gattrib.h"
}

class ReadSyncBlock {
  public:
          uint8_t*    result;
          uint16_t    resultSize;

          ReadSyncBlock(GMutex* mutex);
          ~ReadSyncBlock();

          void setReady();
          bool isReady();
      private:
          GMutex* mutex;
          bool    ready;  //write to it should be synchronized over mutex
};

#endif /* ReadSyncBlock_hpp */
