/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS exampleCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#ifndef ARCHIVERCLIENTRESPONSEHANDLER_H
#define ARCHIVERCLIENTRESPONSEHANDLER_H

#include <string>
#include <vector>

#include "ServiceClient.h"

namespace epics
{

namespace channelArchiverService
{
/**
 * Fields outputted by Channel Archiver client
 */
enum OutputField 
{
    REAL_TIME,
    VALUE,
    DATE,
    ALARM,
    SECONDS_PAST_EPOCH,
    NANO_SECONDS,
    STATUS,
    SEVERITY
};

const int NUMBER_OF_FIELDS = SEVERITY+1;

const std::string outputFieldNames[] = {
    "timePastEpoch(s)",
    "value",
    "Date",
    "Alarm",
    "secsPastEpoch",
    "nsecs",
    "Status",
    "Severity"
    };

/**
 * Parameters according to which the results of the archiver query will be handled.
 */
struct FormatParameters
{
    /**
     * Options for formatting a field.
     */
    enum Format
    {
        DEFAULT,
        SCIENTIFIC,
        FIXED_POINT,
        HEX
    };

    FormatParameters()
      : prefix("#"),
        format(DEFAULT),
        precision(6),
        appendToFile(false),
        printColumnTitles(false),
        transpose(false)
    {
    }

    std::vector<OutputField> outputtedFields;
    std::string filename;
    std::string title;
    std::string prefix;
    Format format;
    int    precision;
    bool   appendToFile;
    bool   printColumnTitles;
    bool   transpose;
};

/**
 * Class to perform the handling of the response from the archive service.
 */
class RequestResponseHandler : public epics::serviceClient::ResponseHandler
{
public:
    POINTER_DEFINITIONS(RequestResponseHandler);
    /**
     * Constructor.
     *
     * @param  parameters       Parameters for the handling the request.
     */
    RequestResponseHandler(const FormatParameters & parameters)
    : m_parameters(parameters), m_ok(true)
    {
    }

   /**
     * Handles the response from the archive service, according to supplied parameters.
     *
     * @param  response         The response sent by service.
     */
    void handle(epics::pvData::PVStructure::shared_pointer const & response);

    bool isOk() { return m_ok; }

    void outputResults();

private:
    void makeStrings(epics::pvData::PVStructure::shared_pointer const & response);


    FormatParameters m_parameters;
    std::vector<std::string> outputFieldValues[NUMBER_OF_FIELDS];
    bool m_ok;
};

}

}

#endif
