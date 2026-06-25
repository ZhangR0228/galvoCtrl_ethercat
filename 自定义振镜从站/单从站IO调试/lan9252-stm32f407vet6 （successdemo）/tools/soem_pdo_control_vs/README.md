# Visual Studio 项目说明

打开此解决方案：

```text
tools\soem_pdo_control_vs\soem_pdo_control_vs.sln
```

项目引用：

- 主控程序：`tools\soem_pdo_control.cpp`
- SOEM 源码：`D:\ethercat_master_windows\SOEM`
- SOEM 生成头文件：`D:\ethercat_master_windows\generated`
- WinPcap/Npcap 库：`D:\ethercat_master_windows\SOEM\oshw\win32\wpcap\Lib\x64`

建议配置：

- 平台：`x64`
- 配置：`Debug` 或 `Release`

如果 Visual Studio 提示平台工具集不匹配，右键项目，选择 `Retarget Projects`，使用本机已安装的 MSVC 工具集即可。

运行程序时需要管理员权限，否则 SOEM 可能无法打开 Npcap 网卡。

默认 EtherCAT 网卡写在 `soem_pdo_control.cpp`：

```cpp
\\Device\\NPF_{628E019F-362D-4B55-AFCE-CC522F65376F}
```

也可以在调试参数里传入网卡名。
