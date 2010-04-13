/*
 * copyright maidsafe.net limited 2008
 * The following source code is property of maidsafe.net limited and
 * is not meant for external use. The use of this code is governed
 * by the license file LICENSE.TXT found in the root of this directory and also
 * on www.maidsafe.net.
 *
 * You are not free to copy, amend or otherwise use this source code without
 * explicit written permission of the board of directors of maidsafe.net
 *
 *  Created on: Nov 13, 2008
 *      Author: Team
 */

#include "maidsafe/vaultbufferpackethandler.h"

namespace maidsafe {

VaultBufferPacketHandler::VaultBufferPacketHandler() : crypto_obj_() {
  crypto_obj_.set_hash_algorithm(crypto::SHA_512);
  crypto_obj_.set_symm_algorithm(crypto::AES_256);
}

bool VaultBufferPacketHandler::CheckMsgStructure(const std::string &ser_message,
                                                 std::string *sender_id,
                                                 MessageType *type) {
  GenericPacket message;

  if (!message.ParseFromString(ser_message))
    return false;

  BufferPacketMessage bpm;
  if (!bpm.ParseFromString(message.data()))
    return false;

  *sender_id = bpm.sender_id();
  *type = bpm.type();
  return true;
}

bool VaultBufferPacketHandler::IsOwner(const std::string &owner_id,
                                       const GenericPacket &gp_info) {
  BufferPacketInfo bpi;
  if (!bpi.ParseFromString(gp_info.data()))
    return false;
  if (bpi.owner() == owner_id)
    return true;
  return false;
}

bool VaultBufferPacketHandler::ValidateOwnerSignature(
    const std::string &public_key,
    const std::string &ser_bufferpacket) {
  BufferPacket bp;
  if (!bp.ParseFromString(ser_bufferpacket))
    return false;
  return crypto_obj_.AsymCheckSig(bp.owner_info(0).data(),
    bp.owner_info(0).signature(), public_key, crypto::STRING_STRING);
}

bool VaultBufferPacketHandler::GetMessages(std::string *ser_bp,
                                           std::vector<std::string> *msgs) {
  BufferPacket bp;
  if (!bp.ParseFromString(*ser_bp))
    return false;
  GenericPacket gp;
  BufferPacketMessage bpm;
  if (bp.messages_size() == 0) {
#ifdef DEBUG
//    printf("VaultBufferPacketHandler::GetMessages - NO messages.\n");
#endif
    return true;
  }
  for (int i = 0; i < bp.messages_size(); i++) {
    gp = bp.messages(i);
    if (bpm.ParseFromString(gp.data())) {
      ValidatedBufferPacketMessage msg;
      msg.set_index(bpm.rsaenc_key());
      msg.set_message(bpm.aesenc_message());
      msg.set_sender(bpm.sender_id());
      msg.set_type(bpm.type());
      msg.set_timestamp(bpm.timestamp());
      std::string ser_msg;
      msg.SerializeToString(&ser_msg);
      msgs->push_back(ser_msg);
    }
  }
  bp.clear_messages();
  if (!bp.SerializeToString(ser_bp))
    return false;
  return true;
}

bool VaultBufferPacketHandler::ClearMessages(std::string *ser_bufferpacket) {
  BufferPacket bp;
  if (!bp.ParseFromString(*ser_bufferpacket)) {
#ifdef DEBUG
    printf("Doesn't parse as a bufferpacket.\n");
#endif
    return false;
  }
  bp.clear_messages();
  bp.SerializeToString(ser_bufferpacket);
  return true;
}

bool VaultBufferPacketHandler::ChangeOwnerInfo(const std::string &ser_gp,
                                               const std::string &public_key,
                                               std::string *ser_packet) {
  if (!ValidateOwnerSignature(public_key, *ser_packet)) {
#ifdef DEBUG
    printf("VBPH::ChangeOwnerInfo - doesn't ValidateOwnerSignature.\n");
#endif
    return false;
  }
  BufferPacket bp;
  bp.ParseFromString(*ser_packet);
  bp.clear_owner_info();
  GenericPacket *gp = bp.add_owner_info();
  if (!gp->ParseFromString(ser_gp)) {
#ifdef DEBUG
    printf("VBPH::ChangeOwnerInfo - ser_gp is not a GenericPacket.\n");
#endif
    return false;
  } else {
    BufferPacketInfo bpi;
    if (!bpi.ParseFromString(gp->data())) {
#ifdef DEBUG
      printf("VBPH::ChangeOwnerInfo - data is not a BufferPacketInfo.\n");
#endif
      return false;
    }
  }
  bp.SerializeToString(ser_packet);
  return true;
}

bool VaultBufferPacketHandler::AddMessage(const std::string &current_bp,
                                          const std::string &ser_message,
                                          const std::string &signed_public_key,
                                          std::string *updated_bp) {
  *updated_bp = "";
  GenericPacket message;
  if (!message.ParseFromString(ser_message)) {
#ifdef DEBUG
    printf("Invalid msg.\n");
#endif
    return false;
  }

  BufferPacket bufferpacket;
  if (!bufferpacket.ParseFromString(current_bp)) {
#ifdef DEBUG
    printf("Invalid bufferpacket.\n");
#endif
    return false;
  }

  // getting message from signed data sent
  BufferPacketMessage bpm;
  if (!bpm.ParseFromString(message.data())) {
#ifdef DEBUG
    printf("Invalid bpmessage.\n");
#endif
    return false;
  }

  // Checking signature or by name of id_package
  std::string public_key;
  //  These are the messages that are not sent with the MPID.
  //  Other new messages sent with the MPID should be here as well.
  if (bpm.type() != INSTANT_MSG && bpm.type() != ADD_CONTACT_RQST) {
    public_key = bpm.sender_public_key();
    std::string id = crypto_obj_.Hash(public_key+signed_public_key, "",
      crypto::STRING_STRING, false);
    if (id != bpm.sender_id()) {
#ifdef DEBUG
      printf("Invalid sender_id.\n");
#endif
      return false;
    }
  } else {
    public_key = bpm.sender_public_key();
  }

  if (!crypto_obj_.AsymCheckSig(message.data(), message.signature(),
    public_key, crypto::STRING_STRING)) {
#ifdef DEBUG
    printf("invalid message signature\n");
#endif
    return false;
  }

  std::string hashed_sender_id = crypto_obj_.Hash(bpm.sender_id(), "",
                                                  crypto::STRING_STRING, false);
  if (bpm.type() != ADD_CONTACT_RQST) {
    BufferPacketInfo bpi;
    bpi.ParseFromString(bufferpacket.owner_info(0).data());
    bool flag = false;
    // TODO(Team#5#): here ther should be no check for user in list
    // if it is decided to accept from all
    for (int i = 0; i < bpi.users_size(); i++) {
      if (bpi.users(i) == hashed_sender_id) {
        i = bpi.users_size();
        flag = true;
      }
    }
    if (!flag) {
#ifdef DEBUG
      printf("unauthorised user %s\n", HexSubstr(hashed_sender_id).c_str());
#endif
      return false;
    }
  }

  GenericPacket *gp = bufferpacket.add_messages();
  gp->set_data(message.data());
  gp->set_signature(message.signature());

  bufferpacket.SerializeToString(updated_bp);
  return true;
}

}   // namespace maidsafe