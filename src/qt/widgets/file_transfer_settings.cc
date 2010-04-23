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
 *  Created on: Jan 23, 2010
 *      Author: Stephen Alexander
 */

#include "qt/widgets/file_transfer_settings.h"
#include "qt/client/client_controller.h"

FileTransferSettings::FileTransferSettings(QWidget* parent) : QWidget(parent) {
  ui_.setupUi(this);
}

FileTransferSettings::~FileTransferSettings() { }
