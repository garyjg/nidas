// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/NidsIterators.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DSMService.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/SampleIOProcessor.h>

using namespace nidas::core;
using namespace std;

DSMServerIterator::DSMServerIterator(const Project* obj):
	_servers(&obj->getServers()),
	_serverItr(_servers->begin()),
	_siteIterator(obj->getSiteIterator())
{
}

DSMServerIterator::DSMServerIterator(const Site* obj):
	_servers(&obj->getServers()),
	_serverItr(_servers->begin()),
        _siteIterator()
{}

DSMServerIterator::DSMServerIterator():
	_servers(0),_serverItr(),_siteIterator()
{}

bool DSMServerIterator::hasNext()
{
    if (_servers && _serverItr != _servers->end()) return true;
    for (; _siteIterator.hasNext(); ) {
	const Site* site = _siteIterator.next();
	_servers = &site->getServers();
	_serverItr = _servers->begin();
	if (_serverItr != _servers->end()) return true;
    }
    return false;
}

DSMServiceIterator::DSMServiceIterator(const Project* obj):
    _dsmServerIterator(obj->getDSMServerIterator()),
    _services(0),_dsmServiceItr()
{}

DSMServiceIterator::DSMServiceIterator(const Site* obj):
    _dsmServerIterator(obj->getDSMServerIterator()),
    _services(0),_dsmServiceItr()
{}

DSMServiceIterator::DSMServiceIterator(const DSMServer* obj):
    _dsmServerIterator(),
    _services(&obj->getServices()),
    _dsmServiceItr(_services->begin())
{}

DSMServiceIterator::DSMServiceIterator():
    _dsmServerIterator(),
    _services(0),_dsmServiceItr()
{}

bool DSMServiceIterator::hasNext()
{
    if (_services && _dsmServiceItr != _services->end()) return true;
    for (; _dsmServerIterator.hasNext(); ) {
	const DSMServer* server = _dsmServerIterator.next();
	_services = &server->getServices();
	_dsmServiceItr = _services->begin();
	if (_dsmServiceItr != _services->end()) return true;
    }
    return false;
}

ProcessorIterator::ProcessorIterator(const Project* obj):
    _serviceIterator(obj->getDSMServiceIterator()),
    _dsmIterator(obj->getDSMConfigIterator()),
    _processors(0),_procItr()
{}

ProcessorIterator::ProcessorIterator(const Site* obj):
    _serviceIterator(obj->getDSMServiceIterator()),
    _dsmIterator(obj->getDSMConfigIterator()),
    _processors(0),_procItr()
{}

ProcessorIterator::ProcessorIterator(const DSMServer* obj):
    _serviceIterator(obj->getDSMServiceIterator()),
    _dsmIterator(),
    _processors(0),_procItr()
{}

ProcessorIterator::ProcessorIterator(const DSMService* obj):
    _serviceIterator(),
    _dsmIterator(),
    _processors(&obj->getProcessors()),
    _procItr(_processors->begin())
{}

ProcessorIterator::ProcessorIterator(const DSMConfig* obj):
    _serviceIterator(),
    _dsmIterator(),
    _processors(&obj->getProcessors()),
    _procItr(_processors->begin())
{}

ProcessorIterator::ProcessorIterator():
    _serviceIterator(),
    _dsmIterator(),
    _processors(0),_procItr()
{}

bool ProcessorIterator::hasNext()
{
    if (_processors && _procItr != _processors->end()) return true;
    for (; _serviceIterator.hasNext(); ) {
	const DSMService* service = _serviceIterator.next();
	_processors = &service->getProcessors();
	_procItr = _processors->begin();
	if (_procItr != _processors->end()) return true;
    }
    for (; _dsmIterator.hasNext(); ) {
	const DSMConfig* dsm = _dsmIterator.next();
	_processors = &dsm->getProcessors();
	_procItr = _processors->begin();
	if (_procItr != _processors->end()) return true;
    }
    return false;
}


SiteIterator::SiteIterator(const Project* obj):
    _sites(&obj->getSites()),
    _siteItr(_sites->begin())
{}

SiteIterator::SiteIterator():
    _sites(0),_siteItr()
{}

bool SiteIterator::hasNext()
{
    if (!_sites) return false;
    return _siteItr != _sites->end();
}

DSMConfigIterator::DSMConfigIterator(const Project* obj):
    _siteIterator(obj->getSiteIterator()),
    _dsms(0),_dsmItr()
{}

DSMConfigIterator::DSMConfigIterator(const Site* site):
	_siteIterator(),
        _dsms(&site->getDSMConfigs()),_dsmItr(_dsms->begin())
{}

DSMConfigIterator::DSMConfigIterator():
    _siteIterator(), _dsms(0),_dsmItr()
{}

bool DSMConfigIterator::hasNext()
{
    if (_dsms && _dsmItr != _dsms->end()) return true;
    for (; _siteIterator.hasNext(); ) {
	const Site* site = _siteIterator.next();
	_dsms = &site->getDSMConfigs();
	_dsmItr = _dsms->begin();
	if (_dsmItr != _dsms->end()) return true;
    }
    return false;
}

SensorIterator::SensorIterator(const Project* obj):
    _dsmIterator(obj->getDSMConfigIterator()),_sensors(0),_sensorItr()
{}

SensorIterator::SensorIterator(const Site* obj):
    _dsmIterator(obj->getDSMConfigIterator()),_sensors(0),_sensorItr()
{}

SensorIterator::SensorIterator(const DSMConfig* obj):
    _dsmIterator(),_sensors(&obj->getSensors()),_sensorItr(_sensors->begin())
{}

SensorIterator::SensorIterator():
    _dsmIterator(), _sensors(0),_sensorItr()
{}

bool SensorIterator::hasNext()
{
    if (_sensors && _sensorItr != _sensors->end()) return true;
    for (; _dsmIterator.hasNext(); ) {
	const DSMConfig* dsm = _dsmIterator.next();
	_sensors = &dsm->getSensors();
	_sensorItr = _sensors->begin();
	if (_sensorItr != _sensors->end()) return true;
    }
    return false;
}


SampleTagIterator::SampleTagIterator(const Project* obj):
    _sensorIterator(obj->getSensorIterator()),
    _processorIterator(obj->getProcessorIterator()),
    _stags(), _sampleTagItr(_stags.end())
{}

SampleTagIterator::SampleTagIterator(const Site* obj):
    _sensorIterator(obj->getSensorIterator()),
    _processorIterator(obj->getProcessorIterator()),
    _stags(),_sampleTagItr(_stags.end())
{}

SampleTagIterator::SampleTagIterator(const DSMConfig* obj):
    _sensorIterator(obj->getSensorIterator()),
    _processorIterator(),
    _stags(),_sampleTagItr(_stags.end())
{}

SampleTagIterator::SampleTagIterator(const DSMServer* obj):
    _sensorIterator(),
    _processorIterator(obj->getProcessorIterator()),
    _stags(),_sampleTagItr(_stags.end())
{}

SampleTagIterator::SampleTagIterator(const SampleSource* obj):
    _sensorIterator(),
    _processorIterator(),
    _stags(obj->getSampleTags()),
    _sampleTagItr(_stags.begin())
{}

SampleTagIterator::SampleTagIterator():
    _sensorIterator(),
    _processorIterator(),
    _stags(),_sampleTagItr(_stags.end())
{}

bool SampleTagIterator::hasNext()
{
    if (_sampleTagItr != _stags.end()) return true;
    for (; _sensorIterator.hasNext(); ) {
	DSMSensor* sensor = _sensorIterator.next();
	_stags = sensor->getSampleTags();
	_sampleTagItr = _stags.begin();
	if (_sampleTagItr != _stags.end()) return true;
    }
    for (; _processorIterator.hasNext(); ) {
	SampleIOProcessor* proc = _processorIterator.next();
        // these are the requested sample tags, not the processed samples tags
	_stags = proc->getRequestedSampleTags();
        // these are the sample tags, not the requested sample tags
	// _stags = proc->getSampleTags();
	_sampleTagItr = _stags.begin();
	if (_sampleTagItr != _stags.end()) return true;
    }
    return false;
}


VariableIterator::VariableIterator(const Project* obj):
    _sampleTagIterator(obj->getSampleTagIterator()),
    _variables(0), _variableItr()

{}

VariableIterator::VariableIterator(const Site* obj):
    _sampleTagIterator(obj->getSampleTagIterator()),
    _variables(0),_variableItr()
{}

VariableIterator::VariableIterator(const DSMConfig* obj):
    _sampleTagIterator(obj->getSampleTagIterator()),
    _variables(0),_variableItr()
{}

VariableIterator::VariableIterator(const SampleSource* obj):
    _sampleTagIterator(obj->getSampleTagIterator()),
    _variables(0),_variableItr()
{}

VariableIterator::VariableIterator(const SampleTag* stag):
    _sampleTagIterator(),
    _variables(&stag->getVariables()),_variableItr(_variables->begin())
{}

bool VariableIterator::hasNext()
{
    // after the assignment operator apparently:
    //  _variables is non-null, _variableItr is valid and works
    //      _sampleTagIterator is not OK.  Always returns hasNext(), and
    //      points to a sample tag with a _variables vector of size 0
    //
    if (_variables && _variableItr != _variables->end()) return true;
    for (; _sampleTagIterator.hasNext(); ) {
	const SampleTag* stag = _sampleTagIterator.next();
	_variables = &stag->getVariables();
	_variableItr = _variables->begin();
	if (_variableItr != _variables->end()) return true;
    }
    return false;
}

