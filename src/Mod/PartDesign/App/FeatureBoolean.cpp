/******************************************************************************
 *   Copyright (c)2013 Jan Rheinlaender <jrheinlaender@users.sourceforge.net> *
 *                                                                            *
 *   This file is part of the FreeCAD CAx development system.                 *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Library General Public              *
 *   License as published by the Free Software Foundation; either             *
 *   version 2 of the License, or (at your option) any later version.         *
 *                                                                            *
 *   This library  is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU Library General Public License for more details.                     *
 *                                                                            *
 *   You should have received a copy of the GNU Library General Public        *
 *   License along with this library; see the file COPYING.LIB. If not,       *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,            *
 *   Suite 330, Boston, MA  02111-1307, USA                                   *
 *                                                                            *
 ******************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <BRepAlgoAPI_Fuse.hxx>
# include <BRepAlgoAPI_Cut.hxx>
# include <BRepAlgoAPI_Common.hxx>
# include <BRepAlgoAPI_Section.hxx>
# include <gp_Trsf.hxx>
# include <gp_Pnt.hxx>
# include <gp_Dir.hxx>
# include <gp_Vec.hxx>
# include <gp_Ax1.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#endif

#include "Body.h"
#include "FeatureBoolean.h"

#include <Base/Console.h>
#include <Base/Exception.h>
#include <App/Document.h>

using namespace PartDesign;

namespace PartDesign {

PROPERTY_SOURCE_WITH_EXTENSIONS(PartDesign::Boolean, PartDesign::Feature)

const char* Boolean::TypeEnums[]= {"Fuse","Cut","Common","Section",NULL};

Boolean::Boolean()
{
    ADD_PROPERTY(Type,((long)0));
    Type.setEnums(TypeEnums);

    initExtension(this);
}

short Boolean::mustExecute() const
{
    if (Group.isTouched())
        return 1;
    return PartDesign::Feature::mustExecute();
}

App::DocumentObjectExecReturn *Boolean::execute(void)
{
    // Check the parameters
    const Part::Feature* baseFeature = this->getBaseObject(/* silent = */ true);

    if (!baseFeature) {
        return new App::DocumentObjectExecReturn("Cannot do boolean operation with invalid BaseFeature");
    }

    std::vector<App::DocumentObject*> tools = Group.getValues();
    if (tools.empty())
        return App::DocumentObject::StdReturn;

    // Get the base shape to operate on
    Part::TopoShape baseTopShape = baseFeature->Shape.getShape();
    if (baseTopShape.getShape().IsNull())
        return new App::DocumentObjectExecReturn("Cannot do boolean operation with invalid base shape");

    //get the body this boolean feature belongs to
    Part::BodyBase* baseBody = Part::BodyBase::findBodyOf(this);

    if(!baseBody)
         return new App::DocumentObjectExecReturn("Cannot do boolean on feature which is not in a body");

    TopoDS_Shape result = baseTopShape.getShape();

    // Get the operation type
    std::string type = Type.getValueAsString();

    for (auto tool : tools)
    {
        // Extract the body shape. Its important to get the actual feature that provides the last solid in the body
        // so that the placement will be right
        if(!tool->isDerivedFrom(Part::Feature::getClassTypeId()))
            return new App::DocumentObjectExecReturn("Cannot do boolean with anything but Part::Feature and its derivatives");

        TopoDS_Shape shape = static_cast<Part::Feature*>(tool)->Shape.getValue();
        TopoDS_Shape boolOp;

        // Must not pass null shapes to the boolean operations
        if (result.IsNull())
            return new App::DocumentObjectExecReturn("Base shape is null");

        if (shape.IsNull())
            return new App::DocumentObjectExecReturn("Tool shape is null");

        if (type == "Fuse") {
            BRepAlgoAPI_Fuse mkFuse(result, shape);
            if (!mkFuse.IsDone())
                return new App::DocumentObjectExecReturn("Fusion of tools failed");
            // we have to get the solids (fuse sometimes creates compounds)
            boolOp = this->getSolid(mkFuse.Shape());
            // lets check if the result is a solid
            if (boolOp.IsNull())
                return new App::DocumentObjectExecReturn("Resulting shape is not a solid");
        } else if (type == "Cut") {
            BRepAlgoAPI_Cut mkCut(result, shape);
            if (!mkCut.IsDone())
                return new App::DocumentObjectExecReturn("Cut out failed");
            boolOp = mkCut.Shape();
        } else if (type == "Common") {
            BRepAlgoAPI_Common mkCommon(result, shape);
            if (!mkCommon.IsDone())
                return new App::DocumentObjectExecReturn("Common operation failed");
            boolOp = mkCommon.Shape();
        } else if (type == "Section") {
            BRepAlgoAPI_Section mkSection(result, shape);
            if (!mkSection.IsDone())
                return new App::DocumentObjectExecReturn("Section failed");
            // we have to get the solids
            boolOp = this->getSolid(mkSection.Shape());
            // lets check if the result is a solid
            if (boolOp.IsNull())
                return new App::DocumentObjectExecReturn("Resulting shape is not a solid");
        }

        result = boolOp; // Use result of this operation for fuse/cut of next body
    }

    this->Shape.setValue(getSolid(result));
    return App::DocumentObject::StdReturn;
}

void Boolean::onChanged(const App::Property* prop) {
    
    if(strcmp(prop->getName(), "Group") == 0)
        touch();

    PartDesign::Feature::onChanged(prop);
}

void Boolean::handleChangedPropertyName(Base::XMLReader &reader, const char * TypeName, const char *PropName)
{
    // The App::PropertyLinkList property was Bodies in the past
    Base::Type type = Base::Type::fromName(TypeName);
    if (Group.getClassTypeId() == type && strcmp(PropName, "Bodies") == 0) {
        Group.Restore(reader);
    }
}

}
