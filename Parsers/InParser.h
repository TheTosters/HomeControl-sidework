/*
 * InParser.h
 *
 *  Created on: Mar 24, 2017
 *      Author: Zarnowski
 */

#ifndef InParser_hpp
#define InParser_hpp
#include <memory>
#include <string>
#include <vector>
#include <string.h>
#include "Number.hpp"

using namespace std;

class InParser;

typedef enum {
    RemoteCommandArgumentType_NONE,
    RemoteCommandArgumentType_DIGIT,
    RemoteCommandArgumentType_STRING,
    RemoteCommandArgumentType_DIGIT_SEQUENCE,
    RemoteCommandArgumentType_STRING_SEQUENCE,
    RemoteCommandArgumentType_DIGIT_MULTI_SEQUENCE,
    RemoteCommandArgumentType_STRING_MULTI_SEQUENCE
} RemoteCommandArgumentType;

class RemoteCommand {
    friend class InParser;
    public:
        bool operator==(const char* rhs);
        bool operator==(const string& rhs);
  
        const RemoteCommandArgumentType getArgType();
  
        const Number argument(int index = 0);
        const int64_t argumentAsInt(int index = 0);
        const uint64_t argumentAsUInt(int index = 0);
        const double argumentAsDouble(int index = 0);
        shared_ptr<string> stringArgument(int index = 0);
  
        shared_ptr<vector<Number> > getDigitSequence(int index = 0);
        shared_ptr<vector<shared_ptr<string> > > getStringSequence(int index = 0);
  
        const unsigned long argumentsCount();
    private:
        char cmd[4];
        RemoteCommandArgumentType argType;
        vector<Number> numericValues;
        vector<shared_ptr<string> >  stringValues;
        vector<shared_ptr<vector<Number> > > numericSeries;
        vector<shared_ptr<vector<shared_ptr <string> > > > stringSeries;
};

class InParser {
    public:
        InParser();
        virtual ~InParser();
        shared_ptr<RemoteCommand> parse(shared_ptr<string> data);
    private:
        void parseCmd(istringstream& stream, shared_ptr<RemoteCommand> outCmd);
        void handleStringArgument(istringstream& stream, shared_ptr<RemoteCommand> outCmd);
        shared_ptr<string> handleSingleString(istringstream& stream);
        void handleDigitArgument(istringstream& stream, shared_ptr<RemoteCommand> outCmd);
        Number handleSingleDigit(istringstream& stream);
        void handleSequence(istringstream& stream, shared_ptr<RemoteCommand> outCmd);
};

#endif /* InParser_hpp */
