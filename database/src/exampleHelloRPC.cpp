// Copyright information and license terms for this software can be
// found in the file LICENSE that is included with the distribution

/* exampleHelloRPC.cpp */

/**
 * @author mrk
 * @date 2013.04.02
 */

#include <pv/standardField.h>
#include <sstream>
#define epicsExportSharedSymbols
#include <pv/exampleHelloRPC.h>

using namespace epics::pvData;
using namespace epics::pvDatabase;
using namespace epics::pvAccess;
using std::tr1::static_pointer_cast;
using namespace std;

namespace epics { namespace exampleCPP { namespace database {


RPCService::shared_pointer  ExampleHelloRPC::create()
{
    FieldCreatePtr fieldCreate = getFieldCreate();
    PVDataCreatePtr pvDataCreate = getPVDataCreate();
    StructureConstPtr  topStructure = fieldCreate->createFieldBuilder()->
        add("value",pvString)->
        createStructure();
    PVStructurePtr pvStructure = pvDataCreate->createPVStructure(topStructure);
    ExampleHelloRPCPtr hello(
        new ExampleHelloRPC(pvStructure));
    return hello;
}

ExampleHelloRPC::ExampleHelloRPC(
    PVStructurePtr const & pvResult)
: pvResult(pvResult)
{
}

PVStructurePtr ExampleHelloRPC::request(PVStructurePtr const &pvArgument)
    throw (RPCRequestException)
{
    PVStringPtr pvFrom = pvArgument->getSubField<PVString>("value");
    if(!pvFrom) {
        stringstream ss;
        ss << " expected string subfield named value. got\n" << pvArgument;
        throw RPCRequestException(
         Status::STATUSTYPE_ERROR,ss.str());
    }
    PVStringPtr pvTo = pvResult->getSubField<PVString>("value");
    pvTo->put("Hello " + pvFrom->get());
    return pvResult;
}


}}}
