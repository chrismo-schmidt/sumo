/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2001-2023 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNEDemandElementPlan.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Sep 2023
///
// An auxiliar, asbtract class for plan elements
/****************************************************************************/

#include <utils/gui/windows/GUIAppEnum.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <netedit/GNENet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEViewNet.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/div/GUIDesigns.h>

#include "GNEDemandElementPlan.h"
#include "GNERoute.h"

// ===========================================================================
// static definitions
// ===========================================================================

const double GNEDemandElementPlan::myArrivalPositionDiameter = SUMO_const_halfLaneWidth;

// ===========================================================================
// GNEDemandElement method definitions
// ===========================================================================

GNEDemandElementPlan::GNEDemandElementPlan(GNEDemandElement* planElement, double arrivalPosition) :
    myArrivalPosition(arrivalPosition),
    myPlanElement(planElement) {
}


void
GNEDemandElementPlan::writePlanAttributes(OutputDevice& device) const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // write attributes depending of parent elements
    if (myPlanElement->myTagProperty.hasAttribute(SUMO_ATTR_ROUTE)) {
        device.writeAttr(SUMO_ATTR_ROUTE, myPlanElement->getParentDemandElements().at(1)->getID());
    } else if (myPlanElement->myTagProperty.hasAttribute(SUMO_ATTR_EDGES)) {
        device.writeAttr(SUMO_ATTR_EDGES, myPlanElement->parseIDs(myPlanElement->getParentEdges()));
    } else {
        // write from attribute (if this is the first element)
        if (myPlanElement->getParentDemandElements().at(0)->getPreviousChildDemandElement(myPlanElement) == 0) {
            // check if write edge or junction
            if (tagProperty.planFromEdge()) {
                device.writeAttr(SUMO_ATTR_FROM, myPlanElement->getParentEdges().front()->getID());
            } else if (tagProperty.planFromTAZ()) {
                device.writeAttr(SUMO_ATTR_FROM_TAZ, myPlanElement->getParentAdditionals().front()->getID());
            } else if (tagProperty.planFromJunction()) {
                device.writeAttr(SUMO_ATTR_FROM_JUNCTION, myPlanElement->getParentJunctions().front()->getID());
            }
        }
        // continue writting to attribute
        if (tagProperty.planToEdge()) {
            device.writeAttr(SUMO_ATTR_TO, myPlanElement->getParentEdges().back()->getID());
        } else if (tagProperty.planToJunction()) {
            device.writeAttr(SUMO_ATTR_TO_JUNCTION, myPlanElement->getParentJunctions().back()->getID());
        } else if (tagProperty.planFromBusStop()) {
            device.writeAttr(SUMO_ATTR_BUS_STOP, myPlanElement->getParentAdditionals().back()->getID());
        } else if (tagProperty.planToTrainStop()) {
            device.writeAttr(SUMO_ATTR_TRAIN_STOP, myPlanElement->getParentAdditionals().back()->getID());
        } else if (tagProperty.planToContainerStop()) {
            device.writeAttr(SUMO_ATTR_CONTAINER_STOP, myPlanElement->getParentAdditionals().back()->getID());
        }
    }
    // check if write arrival position
    if ((myPlanElement->myTagProperty.hasAttribute(SUMO_ATTR_EDGES) || tagProperty.planToEdge()) && (myArrivalPosition > 0)) {
        device.writeAttr(SUMO_ATTR_ARRIVALPOS, myArrivalPosition);
    }
}


GUIGLObjectPopupMenu*
GNEDemandElementPlan::getPlanPopUpMenu(GUIMainWindow& app, GUISUMOAbstractView& parent) {
    GUIGLObjectPopupMenu* ret = new GUIGLObjectPopupMenu(app, parent, *myPlanElement);
    // build header
    myPlanElement->buildPopupHeader(ret, app);
    // build menu command for center button and copy cursor position to clipboard
    myPlanElement->buildCenterPopupEntry(ret);
    myPlanElement->buildPositionCopyEntry(ret, app);
    // build menu commands for names
    GUIDesigns::buildFXMenuCommand(ret, "Copy " + myPlanElement->getTagStr() + " name to clipboard", nullptr, ret, MID_COPY_NAME);
    GUIDesigns::buildFXMenuCommand(ret, "Copy " + myPlanElement->getTagStr() + " typed name to clipboard", nullptr, ret, MID_COPY_TYPED_NAME);
    new FXMenuSeparator(ret);
    // build selection and show parameters menu
    myPlanElement->getNet()->getViewNet()->buildSelectionACPopupEntry(ret, myPlanElement);
    myPlanElement->buildShowParamsPopupEntry(ret);
    GUIDesigns::buildFXMenuCommand(ret, ("Cursor position in view: " + toString(getPlanPositionInView().x()) + "," + toString(getPlanPositionInView().y())).c_str(), nullptr, nullptr, 0);
    return ret;
}


GNELane*
GNEDemandElementPlan::getFirstPlanPathLane() const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // get vclass parent
    auto vClassParent = myPlanElement->getParentDemandElements().at(0)->getVClass();
    // get previous plan
    const auto previousPlan = myPlanElement->getParentDemandElements().at(0)->getPreviousChildDemandElement(myPlanElement);
    // first check if this is the first person plan
    if (previousPlan) {
        return previousPlan->getLastPathLane();
    } else {
        // check parents
        if (tagProperty.hasAttribute(SUMO_ATTR_ROUTE)) {
            // route
            return myPlanElement->getParentDemandElements().at(1)->getParentEdges().front()->getLaneByAllowedVClass(vClassParent);
        } else if (tagProperty.hasAttribute(SUMO_ATTR_EDGES) || tagProperty.planFromEdge()) {
            // edges
            return myPlanElement->getParentEdges().front()->getLaneByAllowedVClass(vClassParent);
        } else if (tagProperty.planFromBusStop() || tagProperty.planFromTrainStop() || tagProperty.planFromContainerStop()) {
            // additional
            return myPlanElement->getParentAdditionals().front()->getParentLanes().front();
        } else {
            // in other cases (TAZ, junctions, etc.) return null
            return nullptr;
        }
    }
}


GNELane*
GNEDemandElementPlan::getLastPlanPathLane() const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // get vclass parent
    auto vClassParent = myPlanElement->getParentDemandElements().at(0)->getVClass();
    // check parents
    if (tagProperty.hasAttribute(SUMO_ATTR_ROUTE)) {
        // route
        return myPlanElement->getParentDemandElements().at(1)->getParentEdges().back()->getLaneByAllowedVClass(vClassParent);
    } else if (tagProperty.hasAttribute(SUMO_ATTR_EDGES) || tagProperty.planToEdge()) {
        // edges
        return myPlanElement->getParentEdges().back()->getLaneByAllowedVClass(vClassParent);
    } else if (tagProperty.planToBusStop() || tagProperty.planToTrainStop() || tagProperty.planToContainerStop()) {
        // additional
        return myPlanElement->getParentAdditionals().back()->getParentLanes().front();
    } else {
        // in other cases (TAZ, junctions, etc.) return null
        return nullptr;
    }
}


void
GNEDemandElementPlan::computePlanPathElement() {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // get vClass
    auto vClass = myPlanElement->getVClass();
    // get path manager
    auto pathManager = myPlanElement->getNet()->getPathManager();
    // continue depending of parents
    if (tagProperty.hasAttribute(SUMO_ATTR_ROUTE)) {
        // calculate consecutive path using route edges
        pathManager->calculateConsecutivePathEdges(myPlanElement, vClass, myPlanElement->getParentDemandElements().at(1)->getParentEdges());
    } else if (tagProperty.hasAttribute(SUMO_ATTR_EDGES)) {
        // calculate consecutive path using edges
        pathManager->calculateConsecutivePathEdges(myPlanElement, vClass, myPlanElement->getParentEdges());
    } else {
        // get previous plan
        const auto previousPlan = myPlanElement->getParentDemandElements().at(0)->getPreviousChildDemandElement(myPlanElement);
        // first check if this is the first person plan
        if (previousPlan == nullptr) {
            // continue depending of parents
            if (tagProperty.planFromJunction() || tagProperty.planToJunction()) {
                // calculate path between junctions
                pathManager->calculatePathJunctions(myPlanElement, vClass, myPlanElement->getParentJunctions());
            } else {
                // get first and last lane
                auto firstLane = myPlanElement->getFirstPathLane();
                auto lastLane = myPlanElement->getLastPathLane();
                // calculate path between first and last lane
                if (firstLane && lastLane) {
                    pathManager->calculatePathLanes(myPlanElement, vClass, {firstLane, lastLane});
                } else {
                    pathManager->calculatePathLanes(myPlanElement, vClass, {});
                }
            }
        } else {
            // check if previousPlan ends in a junction
            if (previousPlan->getTagProperty().planToJunction() && tagProperty.planFromJunction()) {
                // calculate path between junctions
                pathManager->calculatePathJunctions(myPlanElement, vClass, {previousPlan->getParentJunctions().back(), myPlanElement->getParentJunctions().front()});
            } else {
                // get last lanes of both elements
                auto firstLane = previousPlan->getLastPathLane();
                auto lastLane = myPlanElement->getLastPathLane();
                // calculate path between first and last lane
                if (firstLane && lastLane) {
                    pathManager->calculatePathLanes(myPlanElement, vClass, {firstLane, lastLane});
                } else {
                    pathManager->calculatePathLanes(myPlanElement, vClass, {});
                }
            }
        }
    }
    // update geometry
    updatePlanGeometry();
}


void
GNEDemandElementPlan::updatePlanGeometry() {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // only for plans defined between TAZs
    if (tagProperty.planFromTAZ() && tagProperty.planToTAZ()) {
        updateGeometryTAZs();
    }
    // update child demand elements
    for (const auto& demandElement : myPlanElement->getChildDemandElements()) {
        demandElement->updateGeometry();
    }
}


Boundary
GNEDemandElementPlan::getPlanCenteringBoundary() const {
    Boundary planBoundary;
    // add the combination of all parent edges's boundaries
    for (const auto& edge : myPlanElement->getParentEdges()) {
        planBoundary.add(edge->getCenteringBoundary());
    }
    // add the combination of all parent edges's boundaries
    for (const auto& junction : myPlanElement->getParentJunctions()) {
        planBoundary.add(junction->getCenteringBoundary());
    }
    // add the combination of all parent additional's boundaries (stoppingPlaces and TAZs)
    for (const auto& additional : myPlanElement->getParentAdditionals()) {
        planBoundary.add(additional->getCenteringBoundary());
    }
    // if this element is over route, add their boundary
    if (myPlanElement->getParentDemandElements().size() > 1) {
        planBoundary.add(myPlanElement->getParentDemandElements().at(1)->getCenteringBoundary());
    }
    // check if is valid
    if (planBoundary.isInitialised()) {
        return planBoundary;
    } else {
        return myPlanElement->getParentDemandElements().front()->getCenteringBoundary();
    }
}


Position
GNEDemandElementPlan::getPlanPositionInView() const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // continue depending of parents
    if (myPlanElement->myTagProperty.hasAttribute(SUMO_ATTR_ROUTE)) {
        // route
        return myPlanElement->getParentDemandElements().at(1)->getPositionInView();
    } else if (tagProperty.planFromJunction() || tagProperty.hasAttribute(SUMO_ATTR_EDGES)) {
        // first edge
        return myPlanElement->getParentEdges().front()->getPositionInView();
    } else if (tagProperty.planFromJunction()) {
        // first junction
        return myPlanElement->getParentJunctions().front()->getPositionInView();
    } else if (myPlanElement->getParentAdditionals().size() > 0) {
        // first additional
        return myPlanElement->getParentAdditionals().front()->getPositionInView();
    } else {
        // return parent position
        return myPlanElement->getParentDemandElements().front()->getPositionInView();
    }
}


std::string
GNEDemandElementPlan::getPlanAttribute(SumoXMLAttr key) const {
    // continue depending of key
    switch (key) {
        // Common person plan attributes
        case SUMO_ATTR_ID:
        case GNE_ATTR_PARENT:
            return myPlanElement->getParentDemandElements().at(0)->getID();
        case GNE_ATTR_SELECTED:
            return toString(myPlanElement->isAttributeCarrierSelected());
        case SUMO_ATTR_ARRIVALPOS:
            if (myArrivalPosition < 0) {
                return "";
            } else {
                return toString(myArrivalPosition);
            }
        // edges
        case SUMO_ATTR_FROM:
            return myPlanElement->getParentEdges().front()->getID();
        case SUMO_ATTR_TO:
            return myPlanElement->getParentEdges().back()->getID();
        case SUMO_ATTR_EDGES:
            return myPlanElement->parseIDs(myPlanElement->getParentEdges());
        // junctions
        case SUMO_ATTR_FROM_JUNCTION:
            return myPlanElement->getParentJunctions().front()->getID();
        case SUMO_ATTR_TO_JUNCTION:
            return myPlanElement->getParentJunctions().back()->getID();
        // additionals
        case SUMO_ATTR_FROM_TAZ:
            return myPlanElement->getParentAdditionals().front()->getID();
        case GNE_ATTR_TO_BUSSTOP:
        case GNE_ATTR_TO_TRAINSTOP:
        case SUMO_ATTR_TO_TAZ:
            return myPlanElement->getParentAdditionals().back()->getID();
        // route
        case SUMO_ATTR_ROUTE:
            // get route parent
            if (myPlanElement->isTemplate()) {
                return "";
            } else {
                return myPlanElement->getParentDemandElements().at(1)->getID();
            }
        default:
            throw InvalidArgument(myPlanElement->getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


double
GNEDemandElementPlan::getPlanAttributeDouble(SumoXMLAttr key) const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // declare plan parent
    const auto planParent = myPlanElement->getParentDemandElements().at(0);
    // continue depending of key
    switch (key) {
        case GNE_ATTR_PLAN_GEOMETRY_STARTPOS: {
            // get previous plan
            const auto previousPlan = planParent->getPreviousChildDemandElement(myPlanElement);
            // if we have a previous plan, use their end position
            if (previousPlan) {
                // special case for stoppingPlaces
                if (previousPlan->getTagProperty().planFromBusStop() || 
                    previousPlan->getTagProperty().planFromTrainStop() || 
                    previousPlan->getTagProperty().planFromContainerStop()) {
                    return previousPlan->getParentAdditionals().front()->getAttributeDouble(SUMO_ATTR_ENDPOS);
                } else {
                    return previousPlan->getAttributeDouble(GNE_ATTR_PLAN_GEOMETRY_ENDPOS);
                }
            } else {
                // this is the first plan, then continue depending of parents
                if ((tagProperty.planFromJunction() && tagProperty.planToJunction()) ||
                    (tagProperty.planFromTAZ() && tagProperty.planToTAZ())) {
                    // junctions and TAZs
                    return -1;
                } else {
                    // use parent's depart position
                    return planParent->getAttributeDouble(SUMO_ATTR_DEPARTPOS);
                }
            }
        }
        case GNE_ATTR_PLAN_GEOMETRY_ENDPOS:
            // continue depending of parents
            if ((tagProperty.planFromJunction() && tagProperty.planToJunction()) ||
                (tagProperty.planFromTAZ() && tagProperty.planToTAZ())) {
                // junctions and TAZs
                return -1;
            } else if (tagProperty.planToBusStop() ||
                       tagProperty.planToTrainStop() ||
                       tagProperty.planToContainerStop()) {
                // use start position of the stoppingPlace
                return myPlanElement->getParentAdditionals().back()->getAttributeDouble(SUMO_ATTR_STARTPOS);
            } else {
                // use arrival position
                return myArrivalPosition;
            }
        default:
            throw InvalidArgument(myPlanElement->getTagStr() + " doesn't have a doubleattribute of type '" + toString(key) + "'");
    }
}


Position
GNEDemandElementPlan::getPlanAttributePosition(SumoXMLAttr key) const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // declare plan parent
    const auto planParent = myPlanElement->getParentDemandElements().at(0);
    // continue depending of key
    switch (key) {
        case GNE_ATTR_PLAN_GEOMETRY_STARTPOS: {
            // get previous plan
            const auto previousPlan = planParent->getPreviousChildDemandElement(myPlanElement);
            // if we have a previous plan, use their geometry end position
            if (previousPlan) {
                // special case for stoppingPlaces
                if (previousPlan->getTagProperty().planToBusStop() ||
                    previousPlan->getTagProperty().planToTrainStop() ||
                    previousPlan->getTagProperty().planToContainerStop()) {
                    return previousPlan->getParentAdditionals().front()->getAdditionalGeometry().getShape().back();
                } else {
                    return previousPlan->getAttributePosition(GNE_ATTR_PLAN_GEOMETRY_ENDPOS);
                }
            } else {
                // check parents
                if (tagProperty.planFromJunction()) {
                    // junctions
                    return myPlanElement->getParentJunctions().front()->getPositionInView();
                } else if (tagProperty.planFromTAZ()) {
                    // TAZs
                    return myPlanElement->getParentAdditionals().front()->getPositionInView();
                } else if (tagProperty.planFromBusStop() ||
                           tagProperty.planFromTrainStop() ||
                           tagProperty.planToContainerStop()) {
                    // stoppingPlaces
                    return myPlanElement->getParentAdditionals().front()->getAdditionalGeometry().getShape().back();
                } else {
                    // get first lane
                    const auto firstLane = myPlanElement->getFirstPathLane();
                    // check if first lane exists
                    if (firstLane) {
                        // get lane shape
                        const auto &laneShape = firstLane->getLaneShape();
                        // declare use parent depart position
                        double departPosition = planParent->getAttributeDouble(SUMO_ATTR_DEPARTPOS);
                        // continue depending of arrival position
                        if (departPosition == 0) {
                            return laneShape.front();
                        } else if ((departPosition == -1) || (departPosition >= laneShape.length2D())) {
                            return laneShape.back();
                        } else {
                            return laneShape.positionAtOffset2D(departPosition);
                        }
                    } else {
                        throw ProcessError("firstLane cannot be null");
                    }
                }
            }
        }
        case GNE_ATTR_PLAN_GEOMETRY_ENDPOS: {
            // check parents
            if (tagProperty.planToJunction()) {
                // junctions
                return myPlanElement->getParentJunctions().back()->getPositionInView();
            } else if (tagProperty.planToTAZ()) {
                // taz
                myPlanElement->getParentAdditionals().back()->getPositionInView();
            } else if (tagProperty.planToBusStop() ||
                       tagProperty.planToTrainStop() ||
                       tagProperty.planToContainerStop()) {
                // get additional front shape
                return myPlanElement->getParentAdditionals().back()->getAdditionalGeometry().getShape().front();
            } else {
                // get last lane
                const auto lastLane = myPlanElement->getLastPathLane();
                // check if last lane exists
                if (lastLane) {
                    // get lane shape
                    const auto& laneShape = lastLane->getLaneShape();
                    // continue depending of arrival position
                    if (myArrivalPosition == 0) {
                        return laneShape.front();
                    } else if ((myArrivalPosition == -1) || (myArrivalPosition >= laneShape.length2D())) {
                        return laneShape.back();
                    } else {
                        return laneShape.positionAtOffset2D(myArrivalPosition);
                    }
                } else {
                    WRITE_ERROR("invalid GNE_ATTR_PLAN_GEOMETRY_ENDPOS");
                    // invalid position
                    return Position(0,0);
                }
            }
        }
        default:
            throw InvalidArgument(myPlanElement->getTagStr() + " doesn't have a position attribute of type '" + toString(key) + "'");
    }
}


void
GNEDemandElementPlan::setPlanAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    // continue depending of key
    switch (key) {
        // common attributes
        case SUMO_ATTR_ARRIVALPOS:
        case GNE_ATTR_SELECTED:
            GNEChange_Attribute::changeAttribute(myPlanElement, key, value, undoList);
            break;
        default:
            throw InvalidArgument(myPlanElement->getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEDemandElementPlan::isPlanValid(SumoXMLAttr key, const std::string& value) {
    // continue depending of key
    switch (key) {
        // common attributes
        case SUMO_ATTR_ARRIVALPOS:
            if (value.empty()) {
                return true;
            } else if (GNEAttributeCarrier::canParse<double>(value)) {
                return GNEAttributeCarrier::parse<double>(value) >= 0;
            } else {
                return false;
            }
        case GNE_ATTR_SELECTED:
            return GNEAttributeCarrier::canParse<bool>(value);
        default:
            throw InvalidArgument(myPlanElement->getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEDemandElementPlan::isPlanAttributeEnabled(SumoXMLAttr key) const {
    switch (key) {
        // edges
        case SUMO_ATTR_FROM:
        case SUMO_ATTR_TO:
        case SUMO_ATTR_EDGES:
        // junctions
        case SUMO_ATTR_FROM_JUNCTION:
        case SUMO_ATTR_TO_JUNCTION:
        // additionals
        case SUMO_ATTR_FROM_TAZ:
        case GNE_ATTR_TO_BUSSTOP:
        case GNE_ATTR_TO_TRAINSTOP:
        case SUMO_ATTR_TO_TAZ:
        // route
        case SUMO_ATTR_ROUTE:
            return false;
        default:
            return true;
    }
}


void
GNEDemandElementPlan::setPlanAttribute(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        // Common plan attributes
        case SUMO_ATTR_ARRIVALPOS:
            if (value.empty()) {
                myArrivalPosition = -1;
            } else {
                myArrivalPosition = GNEAttributeCarrier::parse<double>(value);
            }
            break;
        case GNE_ATTR_SELECTED:
            if (GNEAttributeCarrier::parse<bool>(value)) {
                myPlanElement->selectAttributeCarrier();
            } else {
                myPlanElement->unselectAttributeCarrier();
            }
            // check if compute geometry and path
            if (!myPlanElement->isTemplate()) {
                myPlanElement->updateGeometry();
                myPlanElement->computePathElement();
            }
            break;
        default:
            throw InvalidArgument(myPlanElement->getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


std::string
GNEDemandElementPlan::getPlanHierarchyName() const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // declare result
    auto result = myPlanElement->getTagStr() + ": ";
    // continue depending of attributes
    if (tagProperty.hasAttribute(SUMO_ATTR_EDGES)) {
        // edges
        return result + myPlanElement->getParentEdges().front()->getID() + " ... " + myPlanElement->getParentEdges().back()->getID();
    } else if (tagProperty.hasAttribute(SUMO_ATTR_ROUTE)) {
        // route
        return result + myPlanElement->getParentDemandElements().at(1)->getID();
    } else {
        // additional
        if (tagProperty.planFromBusStop() ||
            tagProperty.planFromTrainStop() ||
            tagProperty.planFromContainerStop() ||
            tagProperty.planFromTAZ()) {
            result += myPlanElement->getParentAdditionals().front()->getID();
        }
        // junction
        if (tagProperty.planFromJunction()) {
            result += myPlanElement->getParentJunctions().front()->getID();
        }
        // edge
        if (tagProperty.planFromEdge()) {
            result += myPlanElement->getParentEdges().front()->getID();
        }
        // arrow
        result += " -> ";
        // additional
        if (tagProperty.planToBusStop() ||
            tagProperty.planToTrainStop() ||
            tagProperty.planToContainerStop() ||
            tagProperty.planToTAZ()) {
            result += myPlanElement->getParentAdditionals().back()->getID();
        }
        // junction
        if (tagProperty.planToJunction()) {
            result += myPlanElement->getParentJunctions().back()->getID();
        }
        // edge
        if (tagProperty.planToEdge()) {
            result += myPlanElement->getParentEdges().back()->getID();
        }
        return result;
    }
}


bool
GNEDemandElementPlan::drawPersonPlan() const {
    // get view net
    auto viewNet = myPlanElement->getNet()->getViewNet();
    // check conditions
    if (viewNet->getEditModes().isCurrentSupermodeNetwork() &&
        viewNet->getNetworkViewOptions().showDemandElements() &&
        viewNet->getDemandViewOptions().showAllPersonPlans()) {
        // show all person plans in network mode
        return true;
    } else if (viewNet->getEditModes().isCurrentSupermodeDemand() &&
               viewNet->getDemandViewOptions().showAllPersonPlans()) {
        // show all person plans
        return true;
    } else if (viewNet->getEditModes().isCurrentSupermodeDemand() && myPlanElement->isAttributeCarrierSelected()) {
        // show selected
        return true;
    } else if (viewNet->isAttributeCarrierInspected(myPlanElement->getParentDemandElements().front())) {
        // person parent is inspected
        return true;
    } else if (viewNet->getDemandViewOptions().getLockedPerson() == myPlanElement->getParentDemandElements().front()) {
        // person parent is locked
        return true;
    } else if (viewNet->getInspectedAttributeCarriers().empty()) {
        // nothing is inspected
        return false;
    } else {
        // get inspected AC
        const GNEAttributeCarrier* AC = viewNet->getInspectedAttributeCarriers().front();
        // check condition
        if (AC->getTagProperty().isPersonPlan() && AC->getAttribute(GNE_ATTR_PARENT) == myPlanElement->getAttribute(GNE_ATTR_PARENT)) {
            // common person parent
            return true;
        } else {
            // all conditions are false
            return false;
        }
    }
}


bool
GNEDemandElementPlan::drawContainerPlan() const {
    // get view net
    auto viewNet = myPlanElement->getNet()->getViewNet();
    // check conditions
    if (viewNet->getEditModes().isCurrentSupermodeNetwork() &&
        viewNet->getNetworkViewOptions().showDemandElements() &&
        viewNet->getDemandViewOptions().showAllContainerPlans()) {
        // show all container plans in network mode
        return true;
    } else if (viewNet->getEditModes().isCurrentSupermodeDemand() &&
               viewNet->getDemandViewOptions().showAllContainerPlans()) {
        // show all container plans
        return true;
    } else if (viewNet->getEditModes().isCurrentSupermodeDemand() && myPlanElement->isAttributeCarrierSelected()) {
        // show selected
        return true;
    } else if (viewNet->isAttributeCarrierInspected(myPlanElement->getParentDemandElements().front())) {
        // container parent is inspected
        return true;
    } else if (viewNet->getDemandViewOptions().getLockedContainer() == myPlanElement->getParentDemandElements().front()) {
        // container parent is locked
        return true;
    } else if (viewNet->getInspectedAttributeCarriers().empty()) {
        // nothing is inspected
        return false;
    } else {
        // get inspected AC
        const GNEAttributeCarrier* AC = viewNet->getInspectedAttributeCarriers().front();
        // check condition
        if (AC->getTagProperty().isContainerPlan() && AC->getAttribute(GNE_ATTR_PARENT) == myPlanElement->getAttribute(GNE_ATTR_PARENT)) {
            // common container parent
            return true;
        } else {
            // all conditions are false
            return false;
        }
    }
}


void
GNEDemandElementPlan::drawPlanGL(const GUIVisualizationSettings& s, const RGBColor& planColor) const {
    // get tag property
    const auto tagProperty = myPlanElement->getTagProperty();
    // draw TAZRels
    if (tagProperty.planFromTAZ() && tagProperty.planToTAZ()) {
        // get viewNet
        auto viewNet = myPlanElement->getNet()->getViewNet();
        // get geometry
        auto &geometry = myPlanElement->myDemandElementGeometry;
        // check if boundary has to be drawn
        if (s.drawBoundaries) {
            GLHelper::drawBoundary(myPlanElement->getCenteringBoundary());
        }
        // push GL ID
        GLHelper::pushName(myPlanElement->getGlID());
        // push matrix
        GLHelper::pushMatrix();
        // translate to front
        viewNet->drawTranslateFrontAttributeCarrier(myPlanElement, GLO_TAZ + 1);
        GLHelper::setColor(planColor);
        // draw line
        GUIGeometry::drawGeometry(s, viewNet->getPositionInformation(), geometry, 0.5);
        GLHelper::drawTriangleAtEnd(
            *(geometry.getShape().end() - 2),
            *(geometry.getShape().end() - 1),
            0.5, 0.5, 0.5);
        // pop matrix
        GLHelper::popMatrix();
        // pop name
        GLHelper::popName();
        // inspect contour
        if (viewNet->isAttributeCarrierInspected(myPlanElement)) {
            GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::INSPECT, geometry.getShape(), 0.5, 1, true, true);
        }
        // front contour
        if (viewNet->getFrontAttributeCarrier() == myPlanElement) {
            GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::FRONT, geometry.getShape(), 0.5, 1, true, true);
        }
        // delete contour
        if (viewNet->drawDeleteContour(myPlanElement, myPlanElement)) {
            GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::REMOVE, geometry.getShape(), 0.5, 1, true, true);
        }
        // select contour
        if (viewNet->drawSelectContour(myPlanElement, myPlanElement)) {
            GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::SELECT, geometry.getShape(), 0.5, 1, true, true);
        }
    }
    // check if draw person parent (only over TAZs and junctions)
    if (tagProperty.planFromTAZ() || tagProperty.planFromJunction()) {
        // check if draw person parent
        const GNEDemandElement* personParent = myPlanElement->getParentDemandElements().front();
        if ((personParent->getChildDemandElements().size() > 0) && (personParent->getChildDemandElements().front() == myPlanElement)) {
            personParent->drawGL(s);
        }
    }
    // force draw path
    myPlanElement->getNet()->getPathManager()->forceDrawPath(s, myPlanElement);
}


void
GNEDemandElementPlan::drawPlanPartial(const bool drawPlan, const GUIVisualizationSettings& s, const GNELane* lane, const GNEPathManager::Segment* segment,
                                        const double offsetFront, const double personPlanWidth, const RGBColor& planColor) const {
    // get view net
    auto viewNet = myPlanElement->getNet()->getViewNet();
    // get inspected and front flags
    const bool dottedElement = viewNet->isAttributeCarrierInspected(myPlanElement) || (viewNet->getFrontAttributeCarrier() == myPlanElement);
    // get person parent
    const GNEDemandElement* personParent = myPlanElement->getParentDemandElements().front();
    // check if draw person plan element can be drawn
    if ((planColor.alpha() != 0) && drawPlan && myPlanElement->getNet()->getPathManager()->getPathDraw()->drawPathGeometry(dottedElement, lane, myPlanElement->getTagProperty().getTag())) {
        // get inspected attribute carriers
        const auto& inspectedACs = viewNet->getInspectedAttributeCarriers();
        // get inspected person plan
        const GNEAttributeCarrier* personPlanInspected = (inspectedACs.size() > 0) ? inspectedACs.front() : nullptr;
        // flag to check if width must be duplicated
        const bool duplicateWidth = (personPlanInspected == myPlanElement) || (personPlanInspected == personParent);
        // calculate path width
        const double pathWidth = s.addSize.getExaggeration(s, lane) * personPlanWidth * (duplicateWidth ? 2 : 1);
        // declare path geometry
        GUIGeometry personPlanGeometry;
        // update pathGeometry depending of first and last segment
        if (segment->isFirstSegment() && segment->isLastSegment()) {
            personPlanGeometry.updateGeometry(lane->getLaneGeometry().getShape(),
                                              getPlanAttributeDouble(GNE_ATTR_PLAN_GEOMETRY_STARTPOS),
                                              getPlanAttributeDouble(GNE_ATTR_PLAN_GEOMETRY_ENDPOS),
                                              getPlanAttributePosition(GNE_ATTR_PLAN_GEOMETRY_STARTPOS),
                                              getPlanAttributePosition(GNE_ATTR_PLAN_GEOMETRY_ENDPOS));
        } else if (segment->isFirstSegment()) {
            personPlanGeometry.updateGeometry(lane->getLaneGeometry().getShape(),
                                              getPlanAttributeDouble(GNE_ATTR_PLAN_GEOMETRY_STARTPOS),
                                              -1,
                                              getPlanAttributePosition(GNE_ATTR_PLAN_GEOMETRY_STARTPOS),
                                              Position::INVALID);
        } else if (segment->isLastSegment()) {
            personPlanGeometry.updateGeometry(lane->getLaneGeometry().getShape(),
                                              -1,
                                              getPlanAttributeDouble(GNE_ATTR_PLAN_GEOMETRY_ENDPOS),
                                              Position::INVALID,
                                              getPlanAttributePosition(GNE_ATTR_PLAN_GEOMETRY_ENDPOS));
        } else {
            personPlanGeometry = lane->getLaneGeometry();
        }
        // get color
        const RGBColor& pathColor = myPlanElement->drawUsingSelectColor() ? s.colorSettings.selectedPersonPlanColor : planColor;
        // Start drawing adding an gl identificator
        GLHelper::pushName(myPlanElement->getGlID());
        // Add a draw matrix
        GLHelper::pushMatrix();
        // Start with the drawing of the area traslating matrix to origin
        viewNet->drawTranslateFrontAttributeCarrier(myPlanElement, myPlanElement->getType(), offsetFront);
        // Set color
        GLHelper::setColor(pathColor);
        // draw geometry
        GUIGeometry::drawGeometry(s, viewNet->getPositionInformation(), personPlanGeometry, pathWidth);
        // Pop last matrix
        GLHelper::popMatrix();
        // Draw name if isn't being drawn for selecting
        if (!s.drawForRectangleSelection) {
            myPlanElement->drawName(myPlanElement->getCenteringBoundary().getCenter(), s.scale, s.addName);
        }
        // check if myPlanElement is the last segment
        if (segment->isLastSegment()) {
            // calculate circle width
            const double circleRadius = (duplicateWidth ? myArrivalPositionDiameter : (myArrivalPositionDiameter / 2.0));
            const double circleWidth = circleRadius * MIN2((double)0.5, s.laneWidthExaggeration);
            const double circleWidthSquared = circleWidth * circleWidth;
            // get geometryEndPos
            const Position geometryEndPos = getPlanAttributePosition(GNE_ATTR_PLAN_GEOMETRY_ENDPOS);
            // check if endPos can be drawn
            if (!s.drawForRectangleSelection || (viewNet->getPositionInformation().distanceSquaredTo2D(geometryEndPos) <= (circleWidthSquared + 2))) {
                // push draw matrix
                GLHelper::pushMatrix();
                // Start with the drawing of the area traslating matrix to origin
                viewNet->drawTranslateFrontAttributeCarrier(myPlanElement, myPlanElement->getType());
                // translate to pos and move to upper using GLO_PERSONTRIP (to avoid overlapping)
                glTranslated(geometryEndPos.x(), geometryEndPos.y(), 0);
                // Set person plan color
                GLHelper::setColor(pathColor);
                // resolution of drawn circle depending of the zoom (To improve smothness)
                GLHelper::drawFilledCircle(circleWidth, s.getCircleResolution());
                // pop draw matrix
                GLHelper::popMatrix();
            }
        }
        // check if we have to draw an red arrow or line
        if (segment->getNextSegment() && segment->getNextSegment()->getLane()) {
            // get firstPosition (last position of current lane shape)
            const Position from = lane->getLaneShape().back();
            // get lastPosition (first position of next lane shape)
            const Position to = segment->getNextSegment()->getLane()->getLaneShape().front();
            // push draw matrix
            GLHelper::pushMatrix();
            // Start with the drawing of the area traslating matrix to origin
            viewNet->drawTranslateFrontAttributeCarrier(myPlanElement, myPlanElement->getType());
            // draw child line
            GUIGeometry::drawChildLine(s, from, to, RGBColor::RED, dottedElement || myPlanElement->isAttributeCarrierSelected(), .05);
            // pop draw matrix
            GLHelper::popMatrix();
        }
        // check if we have to draw an red arrow or line
        if (segment->getPreviousSegment() && segment->getPreviousSegment()->getLane()) {
            // get firstPosition (last position of current lane shape)
            const Position from = lane->getLaneShape().front();
            // get lastPosition (first position of next lane shape)
            const Position to = segment->getPreviousSegment()->getLane()->getLaneShape().back();
            // push draw matrix
            GLHelper::pushMatrix();
            // Start with the drawing of the area traslating matrix to origin
            viewNet->drawTranslateFrontAttributeCarrier(myPlanElement, myPlanElement->getType());
            // draw child line
            GUIGeometry::drawChildLine(s, from, to, RGBColor::RED, dottedElement || myPlanElement->isAttributeCarrierSelected(), .05);
            // pop draw matrix
            GLHelper::popMatrix();
        }
        // Pop name
        GLHelper::popName();
        // declare trim geometry to draw
        const auto shape = (segment->isFirstSegment() || segment->isLastSegment()) ? personPlanGeometry.getShape() : lane->getLaneShape();
        // check if mouse is over element
        myPlanElement->mouseWithinGeometry(shape, pathWidth);
        // check if shape dotted contour has to be drawn
        if (dottedElement) {
            // inspect contour
            if (viewNet->isAttributeCarrierInspected(myPlanElement)) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::INSPECT, shape, pathWidth, 1, segment->isFirstSegment(), segment->isLastSegment());
            }
            // front element contour
            if (viewNet->getFrontAttributeCarrier() == myPlanElement) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::FRONT, shape, pathWidth, 1, segment->isFirstSegment(), segment->isLastSegment());
            }
            // delete contour
            if (viewNet->drawDeleteContour(myPlanElement, myPlanElement)) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::REMOVE, shape, pathWidth, 1, segment->isFirstSegment(), segment->isLastSegment());
            }
            // select contour
            if (viewNet->drawSelectContour(myPlanElement, myPlanElement)) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::SELECT, shape, pathWidth, 1, segment->isFirstSegment(), segment->isLastSegment());
            }
        }
    }
    // draw person parent if myPlanElement is the edge first edge and myPlanElement is the first plan
    if (myPlanElement->getParentJunctions().empty() && (myPlanElement->getFirstPathLane()->getParentEdge() == lane->getParentEdge()) &&
            (personParent->getChildDemandElements().front() == myPlanElement)) {
        personParent->drawGL(s);
    }
}


void
GNEDemandElementPlan::drawPlanPartial(const bool drawPlan, const GUIVisualizationSettings& s, const GNELane* fromLane, const GNELane* toLane, const GNEPathManager::Segment* /*segment*/,
                                        const double offsetFront, const double personPlanWidth, const RGBColor& planColor) const {
    // get view net
    auto viewNet = myPlanElement->getNet()->getViewNet();
    // get inspected and front flags
    const bool dottedElement = viewNet->isAttributeCarrierInspected(myPlanElement) || (viewNet->getFrontAttributeCarrier() == myPlanElement);
    // check if draw person plan elements can be drawn
    if ((planColor.alpha() != 0) && drawPlan && myPlanElement->getNet()->getPathManager()->getPathDraw()->drawPathGeometry(false, fromLane, toLane, myPlanElement->getTagProperty().getTag())) {
        // get inspected attribute carriers
        const auto& inspectedACs = viewNet->getInspectedAttributeCarriers();
        // get person parent
        const GNEDemandElement* personParent = myPlanElement->getParentDemandElements().front();
        // get inspected person plan
        const GNEAttributeCarrier* personPlanInspected = (inspectedACs.size() > 0) ? inspectedACs.front() : nullptr;
        // flag to check if width must be duplicated
        const bool duplicateWidth = (personPlanInspected == myPlanElement) || (personPlanInspected == personParent);
        // calculate path width
        const double pathWidth = s.addSize.getExaggeration(s, fromLane) * personPlanWidth * (duplicateWidth ? 2 : 1);
        // get color
        const RGBColor& color = myPlanElement->drawUsingSelectColor() ? s.colorSettings.selectedPersonPlanColor : planColor;
        // Start drawing adding an gl identificator
        GLHelper::pushName(myPlanElement->getGlID());
        // push a draw matrix
        GLHelper::pushMatrix();
        // Start with the drawing of the area traslating matrix to origin
        viewNet->drawTranslateFrontAttributeCarrier(myPlanElement, myPlanElement->getType(), offsetFront);
        // check if draw lane2lane connection or a red line
        if (fromLane && fromLane->getLane2laneConnections().exist(toLane)) {
            // obtain lane2lane geometry
            const GUIGeometry& lane2laneGeometry = fromLane->getLane2laneConnections().getLane2laneGeometry(toLane);
            // Set person plan color
            GLHelper::setColor(color);
            // draw lane2lane
            GUIGeometry::drawGeometry(s, viewNet->getPositionInformation(), lane2laneGeometry, pathWidth);
        } else {
            // Set invalid person plan color
            GLHelper::setColor(RGBColor::RED);
            // draw line between end of first shape and first position of second shape
            GLHelper::drawBoxLines({fromLane->getLaneShape().back(), toLane->getLaneShape().front()}, (0.5 * pathWidth));
        }
        // Pop last matrix
        GLHelper::popMatrix();
        // Pop name
        GLHelper::popName();
        // draw lock icon
        GNEViewNetHelper::LockIcon::drawLockIcon(myPlanElement, myPlanElement->getType(), myPlanElement->getPositionInView(), 0.5);
        // check if shape dotted contour has to be drawn
        if (fromLane->getLane2laneConnections().exist(toLane) && dottedElement) {
            // check if mouse is over element
            myPlanElement->mouseWithinGeometry(fromLane->getLane2laneConnections().getLane2laneGeometry(toLane).getShape(), pathWidth);
            // inspect contour
            if (viewNet->isAttributeCarrierInspected(myPlanElement)) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::INSPECT, fromLane->getLane2laneConnections().getLane2laneGeometry(toLane).getShape(),
                        pathWidth, 1, false, false);
            }
            // front contour
            if (viewNet->getFrontAttributeCarrier() == myPlanElement) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::FRONT, fromLane->getLane2laneConnections().getLane2laneGeometry(toLane).getShape(),
                        pathWidth, 1, false, false);
            }
            // delete contour
            if (viewNet->drawDeleteContour(myPlanElement, myPlanElement)) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::REMOVE, fromLane->getLane2laneConnections().getLane2laneGeometry(toLane).getShape(),
                        pathWidth, 1, false, false);
            }
            // select contour
            if (viewNet->drawSelectContour(myPlanElement, myPlanElement)) {
                GUIDottedGeometry::drawDottedContourShape(s, GUIDottedGeometry::DottedContourType::SELECT, fromLane->getLane2laneConnections().getLane2laneGeometry(toLane).getShape(),
                        pathWidth, 1, false, false);
            }
        }
    }
}


GNEDemandElement::Problem
GNEDemandElementPlan::isPersonPlanValid() const {
    // get previous plan
    const auto previousPlan = myPlanElement->getParentDemandElements().at(0)->getPreviousChildDemandElement(myPlanElement);
    if (previousPlan) {
        // get previous lane
        const auto previousLastLane = previousPlan->getLastPathLane();
        // get first lane
        const auto firstLane = myPlanElement->getLastPathLane();
        // compare edges
        if (previousLastLane && firstLane && (previousLastLane->getParentEdge() != firstLane->getParentEdge())) {
            return GNEDemandElement::Problem::DISCONNECTED_PLAN;
        }
        // in the future, check more elements
    }
    // get next child
    const auto nextPlan = myPlanElement->getParentDemandElements().at(0)->getNextChildDemandElement(myPlanElement);
    if (nextPlan) {
        // get previous lane
        const auto nextFirstLane = nextPlan->getFirstPathLane();
        // get first lane
        const auto lastLane = myPlanElement->getLastPathLane();
        // compare edges
        if (nextFirstLane && lastLane && (nextFirstLane->getParentEdge() != lastLane->getParentEdge())) {
            return GNEDemandElement::Problem::DISCONNECTED_PLAN;
        }
        // in the future, check more elements
    }
    // all ok, then return true
    return GNEDemandElement::Problem::OK;
}


std::string
GNEDemandElementPlan::getPersonPlanProblem() const {
    // get previous plan
    const auto previousPlan = myPlanElement->getParentDemandElements().at(0)->getPreviousChildDemandElement(myPlanElement);
    if (previousPlan) {
        // get previous lane
        const auto previousLastLane = previousPlan->getLastPathLane();
        // get first lane
        const auto firstLane = myPlanElement->getLastPathLane();
        // compare edges
        if (previousLastLane && firstLane && (previousLastLane->getParentEdge() != firstLane->getParentEdge())) {
            return TLF("Edge '%' is not consecutive with edge '%'", previousLastLane->getParentEdge()->getID(), firstLane->getParentEdge()->getID());
        }
        // in the future, check more elements
    }
    // get next child
    const auto nextPlan = myPlanElement->getParentDemandElements().at(0)->getNextChildDemandElement(myPlanElement);
    if (nextPlan) {
        // get previous lane
        const auto nextFirstLane = nextPlan->getFirstPathLane();
        // get first lane
        const auto lastLane = myPlanElement->getLastPathLane();
        // compare edges
        if (nextFirstLane && lastLane && (nextFirstLane->getParentEdge() != lastLane->getParentEdge())) {
            return TLF("Edge '%' is not consecutive with edge '%'", nextFirstLane->getParentEdge()->getID(), lastLane->getParentEdge()->getID());
        }
        // in the future, check more elements
    }
    // undefined problem
    return "undefined problem";
}


void
GNEDemandElementPlan::updateGeometryTAZs() {
    // remove from grid
    myPlanElement->getNet()->removeGLObjectFromGrid(myPlanElement);
    // get both TAZs
    const GNEAdditional* TAZA = myPlanElement->getParentAdditionals().front();
    const GNEAdditional* TAZB = myPlanElement->getParentAdditionals().back();
    // check if this is the same TAZ
    if (TAZA == TAZB) {
        // declare ring
        PositionVector ring;
        // declare first point
        std::pair<double, double> p1 = GLHelper::getCircleCoords().at(GLHelper::angleLookup(0));
        // add 8 segments
        for (int i = 0; i <= 8; ++i) {
            const std::pair<double, double>& p2 = GLHelper::getCircleCoords().at(GLHelper::angleLookup(0 + i * 45));
            // make al line between 0,0 and p2
            PositionVector line = {Position(), Position(p2.first, p2.second)};
            // extrapolate
            line.extrapolate(3, false, true);
            // add line back to ring
            ring.push_back(line.back());
            // update p1
            p1 = p2;
        }
        // make a copy of ring
        PositionVector ringCenter = ring;
        // move ring to first geometry point
        ring.add(TAZA->getAdditionalGeometry().getShape().front());
        myPlanElement->myDemandElementGeometry.updateGeometry(ring);
    } else {
        // calculate line between to TAZ centers
        PositionVector line = {TAZA->getAttributePosition(SUMO_ATTR_CENTER), TAZB->getAttributePosition(SUMO_ATTR_CENTER)};
        // check line
        if (line.length() < 1) {
            line = {TAZA->getAttributePosition(SUMO_ATTR_CENTER) - 0.5, TAZB->getAttributePosition(SUMO_ATTR_CENTER) + 0.5};
        }
        // calculate middle point
        const Position middlePoint = line.getLineCenter();
        // get closest points to middlePoint
        Position posA = TAZA->getAdditionalGeometry().getShape().positionAtOffset2D(TAZA->getAdditionalGeometry().getShape().nearest_offset_to_point2D(middlePoint));
        Position posB = TAZB->getAdditionalGeometry().getShape().positionAtOffset2D(TAZB->getAdditionalGeometry().getShape().nearest_offset_to_point2D(middlePoint));
        // check positions
        if (posA == Position::INVALID) {
            posA = TAZA->getAdditionalGeometry().getShape().front();
        }
        if (posB == Position::INVALID) {
            posB = TAZB->getAdditionalGeometry().getShape().front();
        }
        // update geometry
        if (posA.distanceTo(posB) < 1) {
            myPlanElement->myDemandElementGeometry.updateGeometry({posA - 0.5, posB + 0.5});
        } else {
            myPlanElement->myDemandElementGeometry.updateGeometry({posA, posB});
        }
    }
    // add into grid again
    myPlanElement->getNet()->addGLObjectIntoGrid(myPlanElement);
}

/****************************************************************************/