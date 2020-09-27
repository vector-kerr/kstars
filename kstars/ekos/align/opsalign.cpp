/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "opsalign.h"

#include "align.h"
#include "fov.h"
#include "kstars.h"
#include "ksnotification.h"
#include "Options.h"
#include "kspaths.h"

#include <KConfigDialog>
#include <QProcess>

namespace Ekos
{
OpsAlign::OpsAlign(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    editFocusProfile->setIcon(QIcon::fromTheme("document-edit"));
    editFocusProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(editFocusProfile,&QAbstractButton::clicked, this, [this]{
        emit needToLoadProfile(kcfg_FocusOptionsProfile->currentIndex());
    });
    editGuidingProfile->setIcon(QIcon::fromTheme("document-edit"));
    editGuidingProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(editFocusProfile,&QAbstractButton::clicked, this, [this]{
        emit needToLoadProfile(kcfg_GuideOptionsProfile->currentIndex());
    });
    editSolverProfile->setIcon(QIcon::fromTheme("document-edit"));
    editSolverProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(editSolverProfile,&QAbstractButton::clicked, this, [this]{
        emit needToLoadProfile(kcfg_SolveOptionsProfile->currentIndex());
    });

    reload();

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
    connect(reloadProfiles, &QAbstractButton::clicked,this, &OpsAlign::reload);

}

void OpsAlign::reload()
{
    QString savedOptionsProfiles = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QString("SavedOptionsProfiles.ini");

    optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    kcfg_FocusOptionsProfile->clear();
    kcfg_GuideOptionsProfile->clear();
    kcfg_SolveOptionsProfile->clear();
    foreach(SSolver::Parameters param, optionsList)
    {
        kcfg_FocusOptionsProfile->addItem(param.listName);
        kcfg_GuideOptionsProfile->addItem(param.listName);
        kcfg_SolveOptionsProfile->addItem(param.listName);
    }
    kcfg_FocusOptionsProfile->setCurrentIndex(Options::focusOptionsProfile());
    kcfg_GuideOptionsProfile->setCurrentIndex(Options::guideOptionsProfile());
    kcfg_SolveOptionsProfile->setCurrentIndex(Options::solveOptionsProfile());
}

void OpsAlign::slotApply()
{
    emit settingsUpdated();
}
}
