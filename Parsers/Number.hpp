#ifndef _Number_hpp_
#define _Number_hpp_

#include <stdint.h>

//taken from http://stackoverflow.com/a/8058976/2444937 and modified
class Number {
    private:
        enum ValType {
            DoubleType, IntType
        } curType;
        union {
                double doubleVal;
                uint64_t intVal;
        };
    public:
        Number(uint64_t n) :
            curType(IntType), intVal(n) {
        }
        Number(float n) :
            curType(DoubleType), doubleVal(n) {
        }
        Number(double n) :
            curType(DoubleType), doubleVal(n) {
        }
        Number(const Number& n) : curType(n.curType) {
            if (n.curType == DoubleType) {
                doubleVal = n.doubleVal;
            } else {
                intVal = n.intVal;
            }
        }

        int64_t asInt64() {
            return curType == DoubleType ? static_cast<int64_t>(doubleVal) : static_cast<int64_t>(intVal);
        }
  
        uint64_t asUInt64() {
          return curType == DoubleType ? static_cast<uint64_t>(doubleVal) : intVal;
        }
  
        int asInt() {
            return curType == DoubleType ? static_cast<int>(doubleVal) : static_cast<int>(intVal);
        }

        double asDouble() {
            return curType == DoubleType ? doubleVal : static_cast<double>(intVal);
        }

        double asFloat() {
            return curType == DoubleType ? static_cast<float>(doubleVal) : static_cast<float>(intVal);
        }
};

#endif
