// Copyright information and license terms for this software can be
// found in the file LICENSE that is included with the distribution

/* powerSupplyRecord.h */

/**
 * @author mrk
 * @date 2013.04.02
 */
#ifndef POWERSUPPLYRECORD_H
#define POWERSUPPLYRECORD_H


#ifdef epicsExportSharedSymbols
#   define powerSupplyEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/timeStamp.h>
#include <pv/alarm.h>
#include <pv/pvTimeStamp.h>
#include <pv/pvAlarm.h>
#include <pv/pvDatabase.h>

#ifdef powerSupplyEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef powerSupplyEpicsExportSharedSymbols
#endif

#include <shareLib.h>


namespace epics { namespace exampleCPP {namespace powerSupply { 

class PowerSupplyRecord;
typedef std::tr1::shared_ptr<PowerSupplyRecord> PowerSupplyRecordPtr;

class epicsShareClass PowerSupplyRecord :
    public epics::pvDatabase::PVRecord
{
public:
    POINTER_DEFINITIONS(PowerSupplyRecord);
    static PowerSupplyRecordPtr create(
        std::string const & recordName);
    virtual ~PowerSupplyRecord();
    virtual void destroy();
    virtual bool init();
    virtual void process();
    
private:
    PowerSupplyRecord(std::string const & recordName,
        epics::pvData::PVStructurePtr const & pvStructure);
    epics::pvData::PVDoublePtr pvCurrent;
    epics::pvData::PVDoublePtr pvPower;
    epics::pvData::PVDoublePtr pvVoltage;
    epics::pvData::PVAlarm pvAlarm;
    epics::pvData::Alarm alarm;
};


}}}

#endif  /* POWERSUPPLYRECORD_H */
