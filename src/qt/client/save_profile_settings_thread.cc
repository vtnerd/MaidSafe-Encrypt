/*
 * copyright maidsafe.net limited 2009
 * The following source code is property of maidsafe.net limited and
 * is not meant for external use. The use of this code is governed
 * by the license file LICENSE.TXT found in the root of this directory and also
 * on www.maidsafe.net.
 *
 * You are not free to copy, amend or otherwise use this source code without
 * explicit written permission of the board of directors of maidsafe.net
 *
 *  Created on: May 9, 2009
 *      Author: Team
 */

#include "qt/client/save_profile_settings_thread.h"

// qt
#include <QDebug>

#include <string>
#include <vector>

// core
#include "qt/client/client_controller.h"


SaveProfileSettingsThread::SaveProfileSettingsThread(
    QHash<QString, QString> theHash, QObject* parent)
      : WorkerThread(parent), theHash_(theHash) { }

SaveProfileSettingsThread::~SaveProfileSettingsThread() { }

void SaveProfileSettingsThread::run() {
  qDebug() << "SaveProfileSettingsThread::run";

  maidsafe::PersonalDetails pd =
          maidsafe::SessionSingleton::getInstance()->Pd();

  pd.set_full_name(theHash_["FullName"].toStdString());
  pd.set_birthday(theHash_["BirthDay"].toStdString());
  pd.set_city(theHash_["City"].toStdString());
  pd.set_country(theHash_["Country"].toStdString());
  pd.set_gender(theHash_["Gender"].toStdString());
  pd.set_language(theHash_["Language"].toStdString());
  pd.set_phone_number(theHash_["Phone"].toStdString());

  maidsafe::SessionSingleton::getInstance()->SetPd(pd);

  // TODO(Team): Implement save settings

  if (!theHash_["FullName"].isEmpty()) {
  }

  const bool success = true;

  emit completed(success);
}


