/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS exampleCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include <float.h>

/* EPICS Archiver Includes */
#include <epicsVersion.h>
#include <alarm.h>
#include <epicsMath.h>
#include <AutoPtr.h>
#include <MsgLogger.h>
#include <RegularExpression.h>
#include <BinaryTree.h>
#include <AutoIndex.h>
#include <SpreadsheetReader.h>
#include <LinearReader.h>
#include <PlotReader.h>

#include <pv/rpcServer.h>

#include "ArchiverServiceRPC.h"
#include "common.h"


using namespace epics::pvData;
using namespace epics::pvAccess;

namespace epics
{

namespace channelArchiverService
{

ArchiverServiceRPC::~ArchiverServiceRPC() {}

namespace
{

/**
 * Converts a string to a Long.
 * Throws an exception if conversion impossible.
 *
 * @param  str  the argument for the rpc
 * @return      the result of the conversion.
 */
int64_t toLong(const std::string & str)
{
    int64_t result = 0;
    std::stringstream ss(str);
    if (!(ss >> result))
    {
        throw RPCRequestException(Status::STATUSTYPE_ERROR,
            "Cannot convert string " + str + " to Long");
    }
    
    std::string remainder;
    if ((ss >> remainder) && remainder != "")
    {
        throw RPCRequestException(Status::STATUSTYPE_ERROR,
            "Cannot convert string " + str + " to Long");
    }

    return result;
}


}

/**
 * Fills in the list of table column labels
 */
void LabelTable(PVStructure::shared_pointer pvResult)
{
    PVStringArray::svector labels;
    labels.push_back("value");
    labels.push_back("secondsPastEpoch");
    labels.push_back("nanoseconds");
    labels.push_back("status");
    labels.push_back("severity");
    getStringArrayField(pvResult, "labels")->replace(freeze(labels));
}

ArchiverServiceRPC::ArchiverServiceRPC(char * indexFilename)
{
	indexes.push_back(indexFilename);
}

ArchiverServiceRPC::ArchiverServiceRPC(const std::vector<std::string> & indexFilenames)
{
	indexes.resize(indexFilenames.size());
	std::copy(indexFilenames.begin(), indexFilenames.end(), indexes.begin());
}

/**
 * Queries the EPICS R-Tree Channel Archiver, returning raw samples
 */
PVStructure::shared_pointer ArchiverServiceRPC::queryRaw(
    epics::pvData::PVStructure::shared_pointer const & pvArgument,
    std::string & name, 
    const epicsTimeStamp & t0,
    const epicsTimeStamp & t1,
    int64_t maxRecords)
{
    /* Create the result pvStructure */

    PVStructure::shared_pointer pvResult(
        getPVDataCreate()->createPVStructure(makeArchiverResponseStructure(*getFieldCreate())));

    LabelTable(pvResult);

    /* The result table is built up as one STL vector per column */
    
    PVDoubleArray::svector values;
    PVLongArray::svector   secPastEpoch;
    PVIntArray::svector    nsec;
    PVIntArray::svector    stats;
    PVIntArray::svector    sevrs;

    int64_t recordCount = 0;

    for (std::vector<std::string>::const_iterator it = indexes.begin();
    	 it != indexes.end(); ++it)
    {
        /* Open the Index */
		AutoPtr<Index> index(new AutoIndex());

		try
		{
			index->open(it->c_str(), true);
		}
		catch(GenericException & e)
		{
			std::cout << e.what() << std::endl;
			throw RPCRequestException(Status::STATUSTYPE_ERROR, e.what());
		}

		const epicsTime start = t0;
		const epicsTime end = t1;

		/* Create a Database Cursor */

		AutoPtr<DataReader> reader(new RawDataReader(*index));

		/* Seek to the first sample at or before 'start' for the named channel */
		const RawValue::Data *data = 0;
		try
		{
			data = reader->find(stdString(name.c_str()), &start);
		}
		catch(...)
		{
			throw RPCRequestException(Status::STATUSTYPE_ERROR, "Error querying archive");
		}

		/* find returns the reading immediately before start, unless start date is
		   before first reading in archive, so skip to next.*/
		if((data != 0) && (RawValue::getTime(data) < start))
		{
			data = reader->next();
		}


		/* Fill the table */

		for(; recordCount < maxRecords; recordCount++)
		{
			if(data == 0)
			{
				break;
			}
			double value;

			/* missing support for waveforms and strings */

			RawValue::getDouble(reader->getType(), reader->getCount(), data, value, 0);
			epicsTimeStamp t = RawValue::getTime(data);

			if(end < t)
			{
				break;
			}

			int status = RawValue::getStat(data);
			int severity = RawValue::getSevr(data);

			values.push_back(value);
			secPastEpoch.push_back(t.secPastEpoch);
			nsec.push_back(t.nsec);
			stats.push_back(status);
			sevrs.push_back(severity);

			data = reader->next();

		}
    }
        
    /* Pack the table into the pvStructure using some STL helper functions */

    PVStructurePtr resultValues = pvResult->getStructureField("value");

    getDoubleArrayField(resultValues, "value")->replace(freeze(values));
    getLongArrayField(resultValues, "secondsPastEpoch")->replace(freeze(secPastEpoch));
    getIntArrayField(resultValues, "nanoseconds")->replace(freeze(nsec));
    getIntArrayField(resultValues, "status")->replace(freeze(stats));
    getIntArrayField(resultValues, "severity")->replace(freeze(sevrs));
        
    return pvResult;
}


/**
 * Queries the EPICS R-Tree Channel Archiver, returning raw samples
 */
epics::pvData::PVStructure::shared_pointer ArchiverServiceRPC::request(
    epics::pvData::PVStructure::shared_pointer const & pvArgument
    ) throw (RPCRequestException)
{    
    /* Unpack the request type */
    std::string name;
    int64_t start = 0;
    int64_t end   = std::numeric_limits<int32_t>::max();
    int64_t maxRecords = 1000000000; // limit to 1e9 values unless another number is specified

    bool isNTQuery = false;

    if ( (pvArgument->getSubField("path")) && (pvArgument->getStringField("path")) 
      && (pvArgument->getSubField("query")) && (pvArgument->getStructureField("query")))
    {
        isNTQuery = true;
    }

    epics::pvData::PVStructure::shared_pointer const & query
        = isNTQuery ? pvArgument->getStructureField("query") : pvArgument;

    if (query->getStringField(nameStr))
    {
        name = query->getStringField(nameStr)->get();
        if (name == "")
        {
            throw RPCRequestException(Status::STATUSTYPE_ERROR, "Empty channel name");
        }
    }
    else
    {
        throw RPCRequestException(Status::STATUSTYPE_ERROR, "No channel name");
    }
        
    if ((query->getSubField(startStr)) && (query->getStringField(startStr)))
    {
        start = toLong((query->getStringField(startStr)->get()));
    }

    if ((query->getSubField(endStr)) && (query->getStringField(endStr)))
    {
        end = toLong((query->getStringField(endStr)->get()));
    }

    if ((query->getSubField(countStr)) && (query->getStringField(countStr)))
    {
        maxRecords = toLong((query->getStringField(countStr)->get()));
    }

    epicsTimeStamp t0, t1;

    t0.secPastEpoch = start;
    t0.nsec = 0;
    t1.secPastEpoch  = end;
    t1.nsec = 0;

    return queryRaw(pvArgument, name, t0, t1, maxRecords);
}

}

}
