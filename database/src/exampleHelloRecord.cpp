// Copyright information and license terms for this software can be
// found in the file LICENSE that is included with the distribution

/* exampleHello.cpp */

/**
 * @author mrk
 * @date 2013.04.02
 */

#include <pv/standardField.h>

#define epicsExportSharedSymbols
#include <pv/exampleHelloRecord.h>

using namespace epics::pvData;
using namespace epics::pvDatabase;
using std::tr1::static_pointer_cast;
using std::string;

namespace epics { namespace exampleCPP { namespace database {


ExampleHelloRecordPtr ExampleHelloRecord::create(
    string const & recordName)
{
    StandardFieldPtr standardField = getStandardField();
    FieldCreatePtr fieldCreate = getFieldCreate();
    PVDataCreatePtr pvDataCreate = getPVDataCreate();
    StructureConstPtr  topStructure = fieldCreate->createFieldBuilder()->
        addNestedStructure("argument")->
            add("value",pvString)->
            endNested()->
        addNestedStructure("result") ->
            add("value",pvString) ->
            add("timeStamp",standardField->timeStamp()) ->
            endNested()->
        createStructure();
    PVStructurePtr pvStructure = pvDataCreate->createPVStructure(topStructure);

    ExampleHelloRecordPtr pvRecord(
        new ExampleHelloRecord(recordName,pvStructure));
    if(!pvRecord->init()) pvRecord.reset();
    return pvRecord;
}

ExampleHelloRecord::ExampleHelloRecord(
    string const & recordName,
    PVStructurePtr const & pvStructure)
: PVRecord(recordName,pvStructure)
{
}

ExampleHelloRecord::~ExampleHelloRecord()
{
}

void ExampleHelloRecord::destroy()
{
    PVRecord::destroy();
}

bool ExampleHelloRecord::init()
{
    
    initPVRecord();
    PVFieldPtr pvField;
    pvArgumentValue = getPVStructure()->getSubField<PVString>("argument.value");
    if(pvArgumentValue.get()==NULL) return false;
    pvResultValue = getPVStructure()->getSubField<PVString>("result.value");
    if(pvResultValue.get()==NULL) return false;
    pvTimeStamp.attach(getPVStructure()->getSubField("result.timeStamp"));
    return true;
}

void ExampleHelloRecord::process()
{
    pvResultValue->put(string("Hello ") + pvArgumentValue->get());
    timeStamp.getCurrent();
    pvTimeStamp.set(timeStamp);
}

}}}
