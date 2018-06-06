/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2018 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
 * For a list of authors please refer to the file "CREDITS.txt".                   *
 *                                                                                 *
 * This file is part of the Voreen software package. Voreen is free software:      *
 * you can redistribute it and/or modify it under the terms of the GNU General     *
 * Public License version 2 as published by the Free Software Foundation.          *
 *                                                                                 *
 * Voreen is distributed in the hope that it will be useful, but WITHOUT ANY       *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.      *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License in the file   *
 * "LICENSE.txt" along with this file. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                                 *
 * For non-commercial academic use see the license exception specified in the file *
 * "LICENSE-academic.txt". To get information about commercial licensing please    *
 * contact the authors.                                                            *
 *                                                                                 *
 ***********************************************************************************/

#include "voreen/qt/widgets/property/propertyvectorwidget.h"

#include "voreen/core/voreenapplication.h"
#include "voreen/core/voreenmodule.h"

#include "voreen/core/properties/boolproperty.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/cameraproperty.h"
#include "voreen/core/properties/filedialogproperty.h"
#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/fontproperty.h"
#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/matrixproperty.h"
#include "voreen/core/properties/optionproperty.h"
#include "voreen/core/properties/propertyvector.h"
#include "voreen/core/properties/shaderproperty.h"
#include "voreen/core/properties/stringproperty.h"
#include "voreen/core/properties/vectorproperty.h"
#include "voreen/core/properties/volumeurllistproperty.h"
#include "voreen/core/properties/volumeurlproperty.h"

#include "voreen/qt/widgets/customlabel.h"

#include "tgt/logmanager.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QContextMenuEvent>
#include <QFrame>
#include <QMenu>
#include <QScrollArea>


namespace voreen {

PropertyVectorWidget::PropertyVectorWidget(PropertyVector* prop, QWidget* parent)
    : QPropertyWidget(prop, parent),
      property_(prop)
{
    initializePropertyMenu();

    // copy over title widgets generated by superclass
    // and by addVisibilityControls() to header layout
    QHBoxLayout* headerLayout = new QHBoxLayout();
    for (int i=0; i<layout()->count(); ++i)
        headerLayout->addItem(layout()->itemAt(i));
    while (layout()->count() > 0)
        layout()->removeItem(layout()->itemAt(0));

    // re-generate the widgets main layout
    delete layout();
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0,5,0,5);

    // vertical layout of the widget
    QVBoxLayout* widgetLayout = new QVBoxLayout();
    widgetLayout->setSpacing(1);
    layout_->addLayout(widgetLayout);

    // add header layout to widget layout
    widgetLayout->addLayout(headerLayout);

    // layout containing the property widgets
    propertiesLayout_ = new QGridLayout();
    propertiesLayout_->setContentsMargins(5,3,3,3);
    propertiesLayout_->setSpacing(0);

    // scrollarea surrounding the property widgets
    QWidget* scrollWidget = new QWidget();
    scrollWidget->setLayout(propertiesLayout_);
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    widgetLayout->addWidget(scrollArea);

    // add property widgets to properties layout
    for (size_t i=0; i<property_->getProperties().size(); ++i) {
        createAndAddPropertyWidget(property_->getProperties().at(i));
    }

    setFixedHeight(230);
}

void PropertyVectorWidget::initializePropertyMenu() {
    propertyMap_[new QAction("bool", this)] = BOOL;
    propertyMap_[new QAction("button", this)] = BUTTON;
    propertyMap_[new QAction("color", this)] = COLOR;
    propertyMap_[new QAction("float", this)] = FLOAT;
    propertyMap_[new QAction("int", this)] = INT;
    propertyMap_[new QAction("light", this)] = LIGHT;
    propertyMap_[new QAction("string", this)] = STRING;

    propertyMap_[new QAction("intvec2", this)] = INTVEC2;
    propertyMap_[new QAction("intvec3", this)] = INTVEC3;
    propertyMap_[new QAction("intvec4", this)] = INTVEC4;
    propertyMap_[new QAction("floatvec2", this)] = FLOATVEC2;
    propertyMap_[new QAction("floatvec3", this)] = FLOATVEC3;
    propertyMap_[new QAction("floatvec4", this)] = FLOATVEC4;
    propertyMap_[new QAction("floatmat2", this)] = FLOATMAT2;
    propertyMap_[new QAction("floatmat3", this)] = FLOATMAT3;
    propertyMap_[new QAction("floatmat4", this)] = FLOATMAT4;
    //propertyMap_[new QAction("option", this)] = OPTION;
    std::map<QAction*, int>::const_iterator it;
    propertyMenu_ = new QMenu(this);
    for(it = propertyMap_.begin(); it!=propertyMap_.end(); ++it) {
        propertyMenu_->addAction((*it).first);
    }
}

void PropertyVectorWidget::updateFromPropertySlot() {
}

void PropertyVectorWidget::setProperty(PropertyVector* /*change*/) {

}

void PropertyVectorWidget::createAndAddPropertyWidget(Property* prop) {

    if (!VoreenApplication::app()) {
        LERRORC("voreen.qt.ProcessorPropertiesWidget", "VoreenApplication not instantiated");
        return;
    }

    PropertyWidget* propWidget = VoreenApplication::app()->createPropertyWidget(prop);
    if (propWidget)
        prop->addWidget(propWidget);

    QPropertyWidget* qPropWidget = dynamic_cast<QPropertyWidget*>(propWidget);
    if (qPropWidget) {
        qPropWidget->setMinimumWidth(250);
        CustomLabel* nameLabel = qPropWidget->getOrCreateNameLabel();
        int row = propertiesLayout_->rowCount();
        if(nameLabel) {
            propertiesLayout_->addWidget(nameLabel, row, 1);
            propertiesLayout_->addWidget(qPropWidget, row, 2);
        } else {
            propertiesLayout_->addWidget(qPropWidget, row, 1);
        }
    }
    else {
        LERRORC("voreen.qt.PropertyVectorWidget", "Unable to create property widget");
    }
}

void PropertyVectorWidget::createAndAddPropertyWidgetByAction(QAction* /*action*/) {
/*#ifdef VRN_MODULE_BASE
    std::map<QAction*, int>::const_iterator it;
    it = propertyMap_.find(action);
    PropertyContainer* pc = dynamic_cast<PropertyContainer*>(prop_->getOwner());
    if(it != propertyMap_.end()) {
        switch ((*it).second)
        {
        case BOOL:
            pc->addNewProperty(new BoolProperty("bool", "bool"));
            break;
        case BUTTON:
            pc->addNewProperty(new ButtonProperty("button", "button"));
            break;
        case COLOR:
            pc->addNewProperty(new FloatVec4Property("floatvec4", "floatvec4", tgt::vec4(0, 0, 0, 0)));
            break;
        case FLOAT:
            pc->addNewProperty(new FloatProperty("float", "float"));
            break;
        case INT:
            pc->addNewProperty(new IntProperty("int", "int"));
            break;
        case STRING:
            pc->addNewProperty(new StringProperty("string", "string", "string"));
            break;
        case INTVEC2:
            pc->addNewProperty(new IntVec2Property("intvec2", "intvec2", tgt::ivec2(0, 0)));
            break;
        case INTVEC3:
            pc->addNewProperty(new IntVec3Property("intvec3", "intvec3", tgt::ivec3(0, 0, 0)));
            break;
        case INTVEC4:
            pc->addNewProperty(new IntVec4Property("intvec4", "intvec4", tgt::ivec4(0, 0, 0, 0)));
            break;
        case FLOATVEC2:
            pc->addNewProperty(new FloatVec2Property("floatvec2", "floatvec2", tgt::vec2(0, 0)));
            break;
        case FLOATVEC3:
            pc->addNewProperty(new FloatVec3Property("floatvec3", "floatvec3", tgt::vec3(0, 0, 0)));
            break;
        case FLOATVEC4:
            pc->addNewProperty(new FloatVec4Property("floatvec4", "floatvec4", tgt::vec4(0, 0, 0, 0)));
            break;
        case FLOATMAT2:
            pc->addNewProperty(new FloatMat2Property("floatmat2", "floatmat2", tgt::mat2(0,0,0,0)));
            break;
        case FLOATMAT3:
            pc->addNewProperty(new FloatMat3Property("floatmat3", "floatmat3", tgt::mat3(0,0,0,0,0,0,0,0,0)));
            break;
        case FLOATMAT4:
            pc->addNewProperty(new FloatMat4Property("floatmat4", "floatmat4", tgt::mat4(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)));
            break;
        default:
            break;
        }
    //propertyMap_[new QAction("option", this)] = OPTION;

    }
#endif */
}

void PropertyVectorWidget::propertyAdded() {
    createAndAddPropertyWidget(property_->getProperties().at(property_->getProperties().size()-1));
}

void PropertyVectorWidget::contextMenuEvent(QContextMenuEvent* /*e*/) {
/*#ifdef VRN_MODULE_BASE
    if(dynamic_cast<PropertyContainer*>(prop_->getOwner())) {
        QAction* ac = propertyMenu_->exec(e->globalPos());

        if(ac != 0) {
            createAndAddPropertyWidgetByAction(ac);
            propertyAdded();
        }
    }
#endif */
}

} // namespace
