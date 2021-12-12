// Copyright (C) 2021 Justin Decker All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the lib/LICENSE file.

PROVISION_SCHEME_BLE_ ::= 0
PROVISION_SCHEME_AP_ ::= 1
PROVISION_SCHEME_CONSOLE_ ::= 2 // unimplemented at this time

/**
Service for implementing Espressif's Unified Provisioning for BLE.
*/
class BLE:
  scheme_/int
  service_name_/string
  service_key_/string
  proof_of_possession_/string
  protection_/int

  constructor service_name/string proof_of_possession/string="" protection/int=1:
    scheme_ = PROVISION_SCHEME_BLE_
    service_name_ = service_name
    service_key_ = ""
    proof_of_possession_ = proof_of_possession
    protection_ = protection

  start -> bool:
    return provision_init_ scheme_ protection_ proof_of_possession_ service_name_ service_key_

/**
Constructs an AP provisioning service.
*/
class AP:
  scheme_/int
  service_name_/string
  service_key_/string
  proof_of_possession_/string
  protection_/int

  constructor service_name/string service_key/string="" protection/int=1:
    scheme_ = PROVISION_SCHEME_AP_
    service_name_ = service_name
    service_key_ = service_key
    proof_of_possession_ = ""
    protection_ = protection

  start -> bool:
    return provision_init_ scheme_ protection_ proof_of_possession_ service_name_ service_key_

provision_init_ scheme protection proof_of_possession service_name service_key:
  #primitive.provision.init