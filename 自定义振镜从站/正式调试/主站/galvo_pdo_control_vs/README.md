# galvo_pdo_control

Visual Studio x64 console project for writing galvo trajectory data to the EtherCAT slave through SOEM PDOs.

Open:

```text
主站\galvo_pdo_control_vs\galvo_pdo_control.sln
```

Build configuration:

```text
Debug|x64 or Release|x64
```

The project uses the local SOEM source tree:

```text
主站\SOEM-master\SOEM-master
```

Useful commands:

```text
galvo_pdo_control.exe --list-adapters
galvo_pdo_control.exe --dry-run
galvo_pdo_control.exe --ifname "\\Device\\NPF_{...}" --cycle-ms 1 --field-mm 11
```

PDO layout expected by the program:

```text
RxPDO, master to slave, 8 bytes:
  int16  x_code
  int16  y_code
  uint16 sequence
  uint16 control_flags

TxPDO, slave to master, 6 bytes:
  uint16 status
  uint16 last_sequence
  uint16 frame_counter
```

Run the EtherCAT command prompt as Administrator so Npcap can open the selected adapter.
