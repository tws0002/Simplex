/*
Copyright 2016, Blur Studio

This file is part of Simplex.

Simplex is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Simplex is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Simplex.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "simplex_mayaNode.h"
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MGlobal.h>
#include <string>
#include <iostream>

#define CHECKSTAT(s)  if (!(s)) { (s).perror("attributeAffects"); return (s);}

MTypeId	simplex_maya::id(0x001226C8);

// Attribute list
MObject	simplex_maya::aSliders;
MObject	simplex_maya::aWeights;
MObject	simplex_maya::aDefinition;
MObject	simplex_maya::aMinorUpdate;


simplex_maya::simplex_maya() {
	this->sPointer = new simplex::Simplex();
}
simplex_maya::~simplex_maya() {
	delete this->sPointer;
}

MStatus simplex_maya::compute( const MPlug& plug, MDataBlock& data )
{
	MStatus status;
	unsigned int i;
	if( plug == aWeights ) {
		MArrayDataHandle inputData = data.inputArrayValue(aSliders, &status);
		CHECKSTAT(status);


		std::vector<double> inVec;

		// Read the input value from the handle.		
		for (i = 0; i < inputData.elementCount(); ++i){
			inputData.jumpToArrayElement(i);
			auto valueHandle = inputData.inputValue(&status);
			CHECKSTAT(status);
			inVec.push_back(valueHandle.asDouble());
		}

		if (!simplexIsValid || !this->sPointer->loaded){
			MDataHandle jsonData = data.inputValue(aDefinition, &status);
			CHECKSTAT(status);
			MString ss = jsonData.asString();
			std::string sss(ss.asChar(), ss.length());
			this->sPointer->clear();
			this->sPointer->parseJSON(sss);

			simplexIsValid = true;
			if (this->sPointer->hasParseError){
				cerr << "JSON PARSE ERROR: " << this->sPointer->parseError <<
					" \n    At offset: " << std::to_string(this->sPointer->parseErrorOffset) << "\n";
				return MS::kFailure;
			}
		}

		inVec.resize(this->sPointer->sliderLen());
		
		if (!cacheIsValid){
			cacheIsValid = true;
			//cache = this->sPointer->getTwoPassIndexValues(inVec);
			cache = this->sPointer->getDeltaIndexValues(inVec);
		}

		// Set the output weights
		MArrayDataHandle outputArrayHandle = data.outputArrayValue(simplex_maya::aWeights, &status);
		CHECKSTAT(status);
		size_t min = (cache.size() > outputArrayHandle.elementCount()) ? outputArrayHandle.elementCount() : cache.size();
		for (i = 0; i < min; ++i){
			outputArrayHandle.jumpToArrayElement(i);
			auto outHandle = outputArrayHandle.outputValue(&status);
			CHECKSTAT(status);
			outHandle.setDouble(cache[i]);
		}

		data.setClean(plug);
	} 
	else {
		return MS::kUnknownParameter;
	}

	return MS::kSuccess;
}

MStatus simplex_maya::preEvaluation(const  MDGContext& context, const MEvaluationNode& evaluationNode){
    MStatus status;
    if (context.isNormal()){
        if (evaluationNode.dirtyPlugExists(aDefinition, &status) && status){
            this->simplexIsValid = false;
            this->cacheIsValid = false;
        }
        if (evaluationNode.dirtyPlugExists(aSliders, &status) && status){
            this->cacheIsValid = false;
        }
    }
    else {
        return MS::kFailure;
    }
	return MS::kSuccess;
}

MStatus simplex_maya::setDependentsDirty(const MPlug& plug, MPlugArray& plugArray){
	if (plug == aDefinition){
		this->simplexIsValid = false;
		this->cacheIsValid = false;
	}
	if (plug == aSliders){
		this->cacheIsValid = false;
	}
	return MPxNode::setDependentsDirty(plug, plugArray);
}

void* simplex_maya::creator(){
	return new simplex_maya();
}

MStatus simplex_maya::initialize(){
	MFnNumericAttribute	nAttr;
	MFnTypedAttribute	tAttr;
	MFnStringData	sData;
	MStatus	status, status2;

	// Input sliders
	simplex_maya::aMinorUpdate = nAttr.create("minorUpdate", "mu", MFnNumericData::kBoolean, false, &status);
	CHECKSTAT(status);
	nAttr.setKeyable(false);
	nAttr.setReadable(true);
	nAttr.setWritable(true);
	status = simplex_maya::addAttribute(simplex_maya::aMinorUpdate);
	CHECKSTAT(status);

	simplex_maya::aDefinition = tAttr.create("definition", "d", MFnData::kString, sData.create(&status2), &status);
	CHECKSTAT(status);
	CHECKSTAT(status2);
	tAttr.setStorable(true);
	tAttr.setKeyable(true);
	status = simplex_maya::addAttribute(simplex_maya::aDefinition);
	CHECKSTAT(status);

	// Input sliders
	simplex_maya::aSliders = nAttr.create("sliders", "s", MFnNumericData::kDouble, 0.0, &status);
	CHECKSTAT(status);
	nAttr.setKeyable(true);
	nAttr.setReadable(false);
	nAttr.setWritable(true);
	nAttr.setArray(true);
	nAttr.setUsesArrayDataBuilder(true);
	status = simplex_maya::addAttribute(simplex_maya::aSliders);
	CHECKSTAT(status);

	// Output weights
	simplex_maya::aWeights = nAttr.create("weights", "w", MFnNumericData::kDouble, 0.0, &status);
	CHECKSTAT(status);
	nAttr.setStorable(false);
	nAttr.setReadable(true);
	nAttr.setWritable(false);
	nAttr.setArray(true);
	nAttr.setUsesArrayDataBuilder(true);
	status = simplex_maya::addAttribute(simplex_maya::aWeights);
	CHECKSTAT(status);

	// Set data dependencies
	status = attributeAffects(aSliders, aWeights);
	CHECKSTAT(status);
	status = attributeAffects(aDefinition, aWeights);
	CHECKSTAT(status);

	return MS::kSuccess;
}

