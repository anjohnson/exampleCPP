/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS exampleCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <alarm.h>
#include <alarmString.h>
#include <ArrayTools.h>

#include <epicsGetopt.h>
#include <epicsStdlib.h>
#include <epicsTime.h>

#include <RawValue.h>

#include <pv/pvData.h>


using namespace std;
using namespace std::tr1;
using namespace epics::pvData;


#include "types.h"

#include "ArchiverClientResponseHandler.h"

namespace epics
{

namespace channelArchiverService
{

/**
 * Converts an epics alarm status and severity to a string, including
 * archiver special severities.
 *
 * @param  status       Alarm status.
 * @param  severity     Alarm severity. 
 */
std::string MakeAlarmString(short status, short severity)
{
    std::string result;
    char buf[200];

    severity &= 0xfff;
    switch (severity)
    {
    case NO_ALARM:
        result = "NO ALARM";
        return result;
    //  Archiver specials:
    case ARCH_EST_REPEAT:
        sprintf(buf, "Est_Repeat %d", (int)status);
        result = buf;
        return result;
    case ARCH_REPEAT:
        sprintf(buf, "Repeat %d", (int)status);
        result = buf;
        return result;
    case ARCH_DISCONNECT:
        result = "Disconnected";
        return result;
    case ARCH_STOPPED:
        result = "Archive_Off";
        return result;
    case ARCH_DISABLED:
        result = "Archive_Disabled";
        return result;
    }

    if (severity < static_cast<short>(SIZEOF_ARRAY(alarmSeverityString))  &&
        status < static_cast<short>(SIZEOF_ARRAY(alarmStatusString)))
    {
        result = alarmSeverityString[severity];
        result += " ";
        result += alarmStatusString[status];
    }
    else
    {
        sprintf(buf, "%d %d", severity, status);
        result = buf;
    }
    return result;
}


/**
 * Converts an epicsTime to a date string
 *
 * @param  t  the time to convert. 
 * @return The date string.
 */
std::string getDate(epicsTime t)
{
    char buf[1024];
    buf[0] = '\0';
    buf[sizeof(buf)-1] = '\0';
    t.strftime(buf, sizeof(buf)-1, "%c");
    return buf;
}

/**
 * Converts secs past epoch and nsecs to a date string
 *
 * @param  secsPastEpoch seconds past EPICS epoch. 
 * @param  nsecs         nanseconds after second.
 * @return The date string.
 */
std::string getDate(int64_t secsPastEpoch, int32_t nsecs)
{
    epicsTimeStamp ts = { static_cast<epicsUInt32>(secsPastEpoch), static_cast<epicsUInt32>(nsecs)};
    epicsTime t(ts);
    return getDate(t);
}

/**
 * Converts data from array data object to strings according to format parameters 
 * and adds to vector of strings 
 *
 * @param  strings    Array of strings to add to. 
 * @param  arrayData  The array data to add.
 * @param  format     Format used to convert data to string. 
 * @param  precision  Precision used in formating when converting data to string. 
 */
template <typename A>
void dataArrayToVectorOfStrings(vector<string> & strings, const A & arrayData, int length,
                               const FormatParameters::Format format = FormatParameters::DEFAULT, int precision = 6)
{
    strings.reserve(strings.size() + length);
    ostringstream oss;

    switch(format)
    {
    case FormatParameters::SCIENTIFIC:
        oss << showpoint << scientific << setprecision(precision);
        break;

    case FormatParameters::FIXED_POINT:
        oss << showpoint << fixed << setprecision(precision);        
        break;

    case FormatParameters::DEFAULT:
        oss << showpoint  << setprecision(precision);
        break;

    case FormatParameters::HEX:
        oss << hex;
        break;
    }
    for (int i = arrayData.offset; i < arrayData.offset + length; ++i)
    {
        oss << arrayData.data[i];
        strings.push_back(oss.str());
        oss.str("");    
    }
}



/**
 * Class to perform the handling of the response from the archive service.
 */
class RequestResponseHandler
{
public:
/**
 * Constructor.
 *
 * @param  parameters       Parameters for the handling the request.
 */
RequestResponseHandler(const FormatParameters & parameters)
: m_parameters(parameters)
{
}

/**
 * Handles the response from the archive service, according to supplied parameters.
 *
 * @param  response         The response sent by service.
 * @return Status of the call, 0 indicates success, non-zero indicates failure.
 */
int handle(shared_ptr<epics::pvData::PVStructure> response)
{
    //  Handle each of the fields in the archiver query response in turn.

    //  Values.
    PVDoubleArray * values = (PVDoubleArray *)response->getScalarArrayField("value", pvDouble);
    DoubleArrayData valuesArrayData;
    int valuesLength = values->get(0, values->getLength(), &valuesArrayData);

    vector<string> valueStrings;
    dataArrayToVectorOfStrings(valueStrings, valuesArrayData, valuesLength, m_parameters.format, m_parameters.precision);


    //  Seconds.
    PVLongArray * secPastEpochs = (PVLongArray *)response->getScalarArrayField("secPastEpoch", pvLong);
    LongArrayData secPastEpochsArrayData;

    int secPastEpochsLength = secPastEpochs->get(0, secPastEpochs->getLength(), &secPastEpochsArrayData);
    if (secPastEpochsLength != valuesLength)
    {
        cerr << "Data invalid: Secs past epoch and Value lengths don't match." << endl;
        return 1;  
    }

    vector<string> secPastEpochStrings;
    dataArrayToVectorOfStrings(secPastEpochStrings, secPastEpochsArrayData, secPastEpochsLength);


    //  Nanoseconds.
    PVIntArray * nsecs = (PVIntArray *)response->getScalarArrayField("nsec", pvInt);
    IntArrayData nsecsArrayData;
    int nsecsLength = nsecs->get(0, nsecs->getLength(), &nsecsArrayData);
    if (nsecsLength != valuesLength)
    {
        cerr << "Data invalid: nsecs past epoch and Value lengths don't match." << endl;
        return 1;  
    }

    vector<string> nsecStrings;
    dataArrayToVectorOfStrings(nsecStrings, nsecsArrayData, nsecsLength);


    //  Real time in seconds.
    int realTimeLength = min(secPastEpochsLength, nsecsLength);
    vector<string> realTimeStrings;
    realTimeStrings.reserve(realTimeLength);

    {
        ostringstream oss;
        for (int i = 0; i < realTimeLength; ++i)
        {
            oss << secPastEpochsArrayData.data[i]  << ".";
            oss << setfill('0') << setw(9) << nsecsArrayData.data[i];
            realTimeStrings.push_back(oss.str());
            oss.str("");
        }
    }

    //  Dates.
    vector<string> dateStrings;
    int dateLength = min(secPastEpochsLength, nsecsLength);
    dateStrings.reserve(dateLength);

    for (int i = 0; i < dateLength; ++i)
    {     
        string dateString = getDate(secPastEpochsArrayData.data[i], nsecsArrayData.data[i]);
        dateStrings.push_back(dateString);
    }


    //  Alarm status.
    PVIntArray * statuses = (PVIntArray *)response->getScalarArrayField("status", pvInt);
    IntArrayData statusesArrayData;
    int statusesLength = statuses->get(0, statuses->getLength(), &statusesArrayData);
    if (statusesLength != valuesLength)
    {
        cerr << "Data invalid: Alarm Status and Value lengths don't match." << endl;
        return 1;  
    }

    vector<string> statusStrings;
    dataArrayToVectorOfStrings(statusStrings, statusesArrayData, statusesLength, FormatParameters::HEX);


    //  Alarm severity.
    PVIntArray * severities = (PVIntArray *)response->getScalarArrayField("severity", pvInt);
    IntArrayData severitiesArrayData;
    int severitiesLength = severities->get(0, severities->getLength(), &severitiesArrayData);
    if (severitiesLength != valuesLength)
    {
        cerr << "Data invalid: Alarm Severity and Value lengths don't match." << endl;
        return 1;  
    }

    vector<string> severityStrings;
    dataArrayToVectorOfStrings(severityStrings, severitiesArrayData, severitiesLength, FormatParameters::HEX);


    //  Alarm string.
    int alarmStringsLength = std::min(secPastEpochsLength, nsecsLength);
    vector<string> alarmStrings;
    alarmStrings.reserve(alarmStringsLength);

    for (int i = 0; i < valuesLength; ++i)
    {     
        string alarmString = MakeAlarmString(statusesArrayData.data[i], severitiesArrayData.data[i]);
        alarmStrings.push_back(alarmString);
    }


    //  Now output archive data.
    bool outputToFile = m_parameters.filename.compare(string(""));
    std::ofstream outfile;

    if (outputToFile)
    {
        ios_base::openmode openMode = m_parameters.appendToFile ? (ios_base::out | ios_base::app) : ios_base::out;
        outfile.open(m_parameters.filename.c_str(), openMode);
    }

    ostream & out = outputToFile ? outfile : std::cout; 

    //  Print title.
    bool printTitle = m_parameters.title.compare(string(""));
    if (printTitle)
    {
        out << m_parameters.prefix << m_parameters.title << std::endl;
    }

    size_t maxWidthValue         = maxWidth(valueStrings);
    size_t maxWidthSecPastEpoch  = maxWidth(secPastEpochStrings);
    size_t maxWidthnsec          = maxWidth(nsecStrings);
    size_t maxWidthRealTime      = maxWidth(realTimeStrings);
    size_t maxWidthDatesStrings  = maxWidth(dateStrings);
    size_t maxWidthAlarmStrings  = maxWidth(alarmStrings);
    size_t maxWidthStatus        = maxWidth(statusStrings);
    size_t maxWidthSeverity      = maxWidth(severityStrings);

    string columnSpace = "  ";


    //  Print column headers if required.    
    if (m_parameters.printColumnTitles)
    {
        String columnTitle;

        for (size_t i = 0; i < m_parameters.displayedFields.size(); ++i)
        {
            ArchiverField field = m_parameters.displayedFields[i];
            switch(field) 
            {
            case REAL_TIME:
                columnTitle = m_parameters.prefix;
                columnTitle += "timePastEpoch(s)";
                maxWidthRealTime = std::max(maxWidthRealTime, columnTitle.length());
                out << setw(maxWidthRealTime)     << left << columnTitle << columnSpace;                
                break;

            case VALUE:
                columnTitle = m_parameters.prefix;
                columnTitle += "value";
                maxWidthValue = std::max(maxWidthValue, columnTitle.length());
                out << setw(maxWidthValue)        << left << columnTitle << columnSpace;   
                break;

            case DATE:
                columnTitle   = m_parameters.prefix;
                columnTitle  += "Date";
                maxWidthDatesStrings = std::max(maxWidthDatesStrings, columnTitle.length());
                out << setw(maxWidthDatesStrings) << left << columnTitle << columnSpace;   
                break;

            case ALARM:
                columnTitle = m_parameters.prefix;
                columnTitle += "Alarm";
                maxWidthAlarmStrings = std::max(maxWidthAlarmStrings, columnTitle.length());
                out << setw(maxWidthAlarmStrings) << left << columnTitle << columnSpace;   
                break;

            case SECONDS_PAST_EPOCH:
                columnTitle = m_parameters.prefix;
                columnTitle += "secsPastEpoch";
                maxWidthSecPastEpoch = std::max(maxWidthSecPastEpoch, columnTitle.length());
                out << setw(maxWidthSecPastEpoch) << left << columnTitle << columnSpace;  
                break;

            case NANO_SECONDS:
                columnTitle = m_parameters.prefix;
                columnTitle += "nsecs";
                maxWidthnsec = std::max(maxWidthnsec, columnTitle.length());
                out << setw(maxWidthnsec) << left << columnTitle << columnSpace;  
                break;

            case STATUS:
                columnTitle = m_parameters.prefix;
                columnTitle += "Status";
                maxWidthStatus = std::max(maxWidthStatus, columnTitle.length());
                out << setw(maxWidthStatus) << left << columnTitle << columnSpace;  
                break;

            case SEVERITY:
                columnTitle = m_parameters.prefix;
                columnTitle += "Severity";
                maxWidthSeverity = std::max(maxWidthSeverity, columnTitle.length());
                out << setw(maxWidthSeverity) << left << columnTitle << columnSpace;  
                break;
           }
        }
        out << "\n";
    }


    //  Output archive data values. 
    for (int j = 0; j < valuesLength; ++j) 
    {
        for (size_t i = 0; i < m_parameters.displayedFields.size(); ++i)
        {
            ArchiverField field = m_parameters.displayedFields[i];
            switch(field) 
            {
            case REAL_TIME:
                out << setw(maxWidthRealTime)      << right
                    << realTimeStrings[j]          << columnSpace;                
                break;

            case VALUE:
                out << setw(maxWidthValue)         << right
                    << valueStrings[j]             << columnSpace;
                break;

            case DATE:
                out << setw(maxWidthDatesStrings)  <<  left
                    << dateStrings[j]              << columnSpace;
                break;

            case ALARM:
                out << setw(maxWidthAlarmStrings)  <<  left
                    << alarmStrings[j]             << columnSpace;
                break;

            case SECONDS_PAST_EPOCH:
               out << setw(maxWidthSecPastEpoch)  << right
                   << secPastEpochStrings[j]      << columnSpace;
               break;

            case NANO_SECONDS:
                out << setw(maxWidthnsec)          << right
                    << nsecStrings[j]              << columnSpace;
                break;

            case STATUS:
                out << setw(maxWidthStatus)        << right
                    << statusStrings[j]            << columnSpace;
                break;

            case SEVERITY:
                out << setw(maxWidthSeverity)      << right
                    << severityStrings[j]          << columnSpace;
                break;
           }
        }
        out << "\n";
    }
    out.flush();


    if (outputToFile)
    {
        outfile.close();
    }  

    return 0;
}


private:
    FormatParameters m_parameters;
};


/*
  Handle response
*/
int handleResponse(PVStructure::shared_pointer response, const FormatParameters & parameters)
{
   RequestResponseHandler handler(parameters);
   return handler.handle(response);
}

}

}
