/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2021 University of Muenster, Germany,                        *
 * Department of Computer Science.                                                 *
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

#include "voreen/qt/mainwindow/menuentities/saveworkspaceasmenuentity.h"

#include "voreen/qt/mainwindow/voreenqtmainwindow.h"
#include "voreen/qt/networkeditor/networkeditor.h"

#include <QAction>


namespace voreen {

SaveWorkspaceAsMenuEntity::SaveWorkspaceAsMenuEntity()
    : VoreenQtMenuEntity()
{}

SaveWorkspaceAsMenuEntity::~SaveWorkspaceAsMenuEntity() {
}

QAction* SaveWorkspaceAsMenuEntity::createMenuAction() {
    QAction* action = new QAction(getIcon(),QWidget::tr(getName().c_str()),0);
    action->setShortcut(QWidget::tr(getShortCut().c_str()));
    QObject::connect(action, SIGNAL(triggered()), this, SLOT(saveWorkspaceAsSlot()));
    return action;
}

void SaveWorkspaceAsMenuEntity::saveWorkspaceAsSlot() {
    tgtAssert(mainWindow_,"No main window assigned!");
    tgtAssert(mainWindow_->getNetworkEditor(),"No NetworkEditor assigned!");
    mainWindow_->writeCanvasMetaData();
    mainWindow_->getNetworkEditor()->serializeTextItems();
    WsHndlr.saveWorkspaceAs();
}

} //namespace
