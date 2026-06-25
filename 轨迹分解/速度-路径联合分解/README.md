# 速度-路径联合分解 Demo

本目录给出单长轴飞行加工下的速度-路径联合分解示例。脚本读取 NC/G-code 直线轨迹，把目标轨迹分解为：

```text
target_x[k] = axis_x[k] + galvo_x[k]
target_y[k] = galvo_y[k]
target_vx[k] = axis_vx[k] + galvo_vx[k]
target_vy[k] = galvo_vy[k]
```

其中长轴只承担 X 方向慢速大行程运动，振镜承担 X/Y 快速补偿和局部图形轨迹。

## 文件说明

| 路径 | 说明 |
| --- | --- |
| `源代码/position_velocity_joint_mpc_split_demo.py` | 分解脚本 |
| `源代码/position_velocity_joint_mpc_input.nc` | 原始简单轨迹输入 |
| `源代码/complex_position_velocity_joint_mpc_input.nc` | 复杂轨迹输入，包含回环、波纹和星形段 |
| `源代码/requirements.txt` | Python 依赖 |
| `分解结果/` | 原始简单轨迹的已有结果 |
| `分解结果_复杂轨迹/` | 复杂轨迹 demo 的输出结果 |

## 运行复杂轨迹 Demo

在 `源代码` 目录下执行：

```powershell
python position_velocity_joint_mpc_split_demo.py complex_position_velocity_joint_mpc_input.nc --out-dir "..\分解结果_复杂轨迹"
```

运行后会生成：

| 文件 | 说明 |
| --- | --- |
| `position_velocity_joint_mpc_split_samples.csv` | 每个插补周期的目标、长轴、振镜、误差和激光状态 |
| `position_velocity_joint_mpc_split_report.html` | 汇总报告 |
| `pv_fig_position_split.png` | 位置分解图 |
| `pv_fig_velocity_split.png` | 速度分解图 |
| `pv_fig_galvo_square_field.png` | 振镜视场内 XY 轨迹 |
| `pv_fig_field_overlay.png` | 长轴扫场与目标轨迹叠加 |
| `pv_fig_galvo_velocity.png` | 振镜速度曲线 |
| `pv_fig_reconstruction_error.png` | 重构误差 |
| `pv_fig_axis_limits.png` | 长轴速度/加速度 |
| `pv_fig_laser_output.png` | NC 激光命令与最终激光输出 |

## 当前复杂轨迹结果摘要

最近一次运行的复杂轨迹结果：

| 指标 | 数值 |
| --- | --- |
| 插补周期数 | 6110 |
| 最大位置重构误差 | `8.882e-16` |
| 最大速度重构误差 | `1.421e-14` |
| 最大 `|galvo_x|` | `8.523 mm` |
| 最大 `|galvo_y|` | `10.700 mm` |
| 最大 `|galvo_v|` | `162.976 mm/s` |
| 最大 `|axis_v|` | `110.000 mm/s` |
| 最大 `|axis_a|` | `1175.205 mm/s^2` |
| 振镜视场失败周期 | 0 |
| 振镜速度失败周期 | 0 |
| 轴约束/限幅标记周期 | 659 |
| NC 激光命令周期 | 5871 |
| 最终激光开启周期 | 5309 |

