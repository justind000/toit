// Copyright (C) 2021 Toitware ApS.
// Use of this source code is governed by a Zero-Clause BSD license that can
// be found in the examples/LICENSE file.

import provision

main:
  print "starting provisioning"
  provision := provision.AP "PROV_ap"
  //provision := Provision.BLE "PROV_ble"
  ret := provision.start
  print (ret ? "Provisioning success" : "Provisioning failed")

  // loop to simulate doing something
  // if the VM exits after provision.start, there won't be time
  // to notify the app of success or failure
  while true:
    sleep --ms=1000