# LAN9252 + STM32F407 EtherCAT 从站调试说明

本工程是基于 `STM32F407VET6` 和 `LAN9252` 的 EtherCAT 从站 IO 调试工程，使用 Beckhoff SSC 5.12 从站协议栈。工程已经生成 Keil/MDK 目标文件，可通过 ST-LINK 烧录到单片机，然后使用 D 盘的开源 SOEM 主站进行 PDO 扫描、读写和调试。

## 目录说明

- `HL_IO_test1(5.12)/`：STM32CubeMX + Keil MDK 工程目录。
- `HL_IO_test1(5.12)/MDK-ARM/HL-EACT_F4_SSCV5.12/HL-EACT_F4_SSCV5.hex`：当前已编译生成的固件 hex。
- `HL_IO_test1(5.12)/MDK-ARM/test1.uvprojx`：Keil 工程文件。
- `HL_IO_test1(5.12)/SSC/SSC-Device.xml`：从站 ESI/XML 描述文件。
- `HL_IO_test1(5.12)/SSC/Src/SSC-DeviceObjects.h`：对象字典和 PDO 映射定义。
- `ESI模板.xml`：ESI 模板文件。
- `D:\ethercat_master_windows\SOEM`：D 盘开源 SOEM 主站源码。
- `D:\ethercat_master_windows\bin`：已编译的 SOEM 工具目录。

## 硬件准备

1. 使用 ST-LINK 连接 STM32F407：
   - `SWDIO`
   - `SWCLK`
   - `GND`
   - `3.3V` 或目标板自供电
2. 单片机目标板上电。
3. LAN9252 EtherCAT 网口连接到 PC 的独立有线网卡。
4. 建议 EtherCAT 网卡只连接这个从站，不要接入普通局域网交换机。
5. Windows 侧安装 Npcap 或 WinPcap，SOEM 在 Windows 下需要抓包驱动访问网卡。

## 编译固件

如果不需要重新编译，可直接使用已经生成的 hex：

```text
HL_IO_test1(5.12)\MDK-ARM\HL-EACT_F4_SSCV5.12\HL-EACT_F4_SSCV5.hex
```

如需重新编译：

1. 打开 Keil MDK。
2. 打开工程：

```text
HL_IO_test1(5.12)\MDK-ARM\test1.uvprojx
```

3. 选择目标工程并执行 `Build`。
4. 确认生成新的：

```text
HL_IO_test1(5.12)\MDK-ARM\HL-EACT_F4_SSCV5.12\HL-EACT_F4_SSCV5.hex
```

## 通过 ST-LINK 烧录

### 方法一：Keil 直接下载

1. 在 Keil 中打开 `test1.uvprojx`。
2. 点击 `Options for Target`。
3. 在 `Debug` 页选择 `ST-Link Debugger`。
4. 在 `Utilities` 页选择 `Use Debug Driver`。
5. 点击 `Download` 烧录到 STM32F407。
6. 烧录完成后复位目标板。

### 方法二：STM32CubeProgrammer 烧录 hex

打开 STM32CubeProgrammer：

1. 接口选择 `ST-LINK`。
2. 点击 `Connect`。
3. 选择固件文件：

```text
HL_IO_test1(5.12)\MDK-ARM\HL-EACT_F4_SSCV5.12\HL-EACT_F4_SSCV5.hex
```

4. 点击 `Download`。
5. 下载完成后断开连接并复位目标板。

如果已配置 STM32CubeProgrammer 命令行，也可以在 PowerShell 中执行：

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w "HL_IO_test1(5.12)\MDK-ARM\HL-EACT_F4_SSCV5.12\HL-EACT_F4_SSCV5.hex" -v -rst
```

## 当前 PDO 映射

从站对象字典中当前 PDO 为 2 路输出和 2 路输入，按 bit 映射到 1 个字节。

### 主站输出到从站，RxPDO

对象 `0x7010`：

| 对象 | 名称 | 位宽 | 说明 |
| --- | --- | --- | --- |
| `0x7010:01` | `led1` | 1 bit | 主站写输出 bit0 |
| `0x7010:02` | `led2` | 1 bit | 主站写输出 bit1 |

PDO 映射：

```text
0x1601 -> 0x7010:01, 0x7010:02
0x1C12 -> 0x1601
```

### 从站输入到主站，TxPDO

对象 `0x6000`：

| 对象 | 名称 | 位宽 | 说明 |
| --- | --- | --- | --- |
| `0x6000:01` | `switch1` | 1 bit | 主站读输入 bit0 |
| `0x6000:02` | `switch2` | 1 bit | 主站读输入 bit1 |

PDO 映射：

```text
0x1A00 -> 0x6000:01, 0x6000:02
0x1C13 -> 0x1A00
```

因此 SOEM 侧可按 1 字节处理：

```c
/* outputs[0] 写给从站：bit0 = led1，bit1 = led2 */
ctx.grouplist[0].outputs[0] = 0x01;  /* led1=1, led2=0 */
ctx.grouplist[0].outputs[0] = 0x02;  /* led1=0, led2=1 */
ctx.grouplist[0].outputs[0] = 0x03;  /* led1=1, led2=1 */

/* inputs[0] 从从站读回：bit0 = switch1，bit1 = switch2 */
uint8_t input = ctx.grouplist[0].inputs[0];
uint8_t switch1 = input & 0x01;
uint8_t switch2 = (input >> 1) & 0x01;
```

## 使用 D 盘 SOEM 主站

SOEM 工作目录：

```text
D:\ethercat_master_windows
```

已存在工具：

```text
D:\ethercat_master_windows\bin\slaveinfo.exe
D:\ethercat_master_windows\bin\eepromtool.exe
```

### 列出 Windows 网卡

用管理员权限打开 PowerShell：

```powershell
cd /d D:\ethercat_master_windows
.\list_windows_nics.ps1
```

记录 EtherCAT 专用网卡名称，例如：

```text
以太网 2
```

后续命令中的 `<adapter_name>` 替换为实际网卡名。

### 扫描从站和 PDO

```powershell
D:\ethercat_master_windows\bin\slaveinfo.exe "<adapter_name>"
```

查看详细 SDO/PDO 信息：

```powershell
D:\ethercat_master_windows\bin\slaveinfo.exe "<adapter_name>" -sdo
```

正常情况下应能看到 1 个从站，并能看到类似的 PDO：

```text
RxPDO: 0x1601 -> 0x7010:01, 0x7010:02
TxPDO: 0x1A00 -> 0x6000:01, 0x6000:02
```

### 运行 SOEM PDO 周期通信

SOEM 自带 `simple_ng` 示例会进入 OP 状态并周期交换过程数据。若尚未编译该示例，可在 D 盘 SOEM 工程中编译 `samples\simple_ng\simple_ng.c`，或将它加入 `D:\ethercat_master_windows\build_mingw_tools.ps1` 后重新构建。

运行方式：

```powershell
D:\ethercat_master_windows\bin\simple_ng.exe "<adapter_name>"
```

`simple_ng` 会周期打印输出区 `O:` 和输入区 `I:`。本工程的 PDO 很小，通常只看第 1 个字节：

```text
O: 00  I: 00
O: 01  I: 01
O: 03  I: 02
```

其中：

- `O` 的 bit0/bit1 是主站写给从站的 `led1/led2`。
- `I` 的 bit0/bit1 是从站反馈给主站的 `switch1/switch2`。

如果要让 SOEM 主站主动写 PDO，可在 SOEM 示例的周期循环中，在 `ecx_send_processdata()` 之前写输出字节：

```c
static uint8_t pattern = 0;
pattern = (pattern + 1) & 0x03;
ctx.grouplist[0].outputs[0] = pattern;

wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
ecx_send_processdata(&ctx);
```

更稳妥的顺序是：

```c
wkc = ecx_receive_processdata(&ctx, EC_TIMEOUTRET);

uint8_t input = ctx.grouplist[0].inputs[0];
printf("switch1=%d switch2=%d\r\n", input & 0x01, (input >> 1) & 0x01);

ctx.grouplist[0].outputs[0] ^= 0x03;
ecx_send_processdata(&ctx);
```

## EEPROM / SII 注意事项

D 盘已有 EEPROM 相关说明：

```text
D:\ethercat_master_windows\README_EEPROM.md
```

写 EEPROM 前必须先备份：

```powershell
D:\ethercat_master_windows\bin\eepromtool.exe "<adapter_name>" 1 -r D:\ethercat_master_windows\backup\slave1_eeprom.bin
```

只有在确认 Vendor ID、Product Code、Revision、SM2/SM3 和 PDO 大小都与固件一致后，才写入新的 SII：

```powershell
D:\ethercat_master_windows\bin\eepromtool.exe "<adapter_name>" 1 -w D:\ethercat_master_windows\sii\galvo_sii.bin
```

EEPROM/SII 写错可能导致主站无法识别从站。调 PDO 功能时，优先使用当前固件和 `slaveinfo`/`simple_ng` 验证，不要一开始就写 EEPROM。

## 常见问题

### SOEM 找不到从站

1. 确认 PowerShell 以管理员权限运行。
2. 确认 Npcap/WinPcap 已安装。
3. 确认选择的是实际连接 EtherCAT 从站的有线网卡。
4. 禁用该网卡上的普通 IP 网络干扰，或至少保证它只接 EtherCAT 从站。
5. 目标板复位后重新运行 `slaveinfo`。

### 从站不能进入 OP

1. 先运行：

```powershell
D:\ethercat_master_windows\bin\slaveinfo.exe "<adapter_name>" -sdo
```

2. 检查 SM2/SM3 大小是否为 1 字节级别。
3. 检查 PDO 映射是否与 `SSC-DeviceObjects.h` 一致。
4. 如果出现 AL Status Code，根据 SOEM 输出的错误码回查对象字典和 SyncManager 配置。

### PDO 写了没有反应

1. 确认主站已经进入 `OPERATIONAL`。
2. 确认每周期都调用了 `ecx_send_processdata()` 和 `ecx_receive_processdata()`。
3. 确认写的是 `ctx.grouplist[0].outputs[0]` 的 bit0/bit1。
4. 确认从站应用层确实把 `Obj0x7010.Led1/Led2` 绑定到目标 GPIO 或调试变量。

