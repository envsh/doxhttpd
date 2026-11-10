# etapps/istat_menus 系统监控面板 —— 详细设计文档

> 对标 macOS **iStat Menus 6.50**（bjango），Linux root-free 落地子集。
> 范式沿用 etapps/ytdlp、etapps/cpuz 九件套插件（qltox Etapps），
> Qt3.3.8/Qt4.8.7 双构建兼容（Qt3 先行）。

## 0. 产物与形态

| 形态 | 产物 | 落位 |
| --- | --- | --- |
| qltox 插件 | libistat_menus_ui.so | qltox/plugins/uiapps（qt3）· plugins4/uiapps（qt4） |
| 独立 exe | istat_menus_ui | etapps/istat_menus/build-exe（qt3）· build-exe4（qt4） |

plugin 三符号：`plugin_name="iStat Menus"`、`plugin_version="1.0.0"`、
`plugin_description="系统监控面板"`、`plugin_create → createIstatMainWindow`。
qltox Etapps 菜单**零改动**即插即用。

## 1. 数据源清单（本机 i5-4210M 无 root 实测，2026-11）

| 类别 | 采集路径/命令 | 本机实测值样例 | 用途 |
| --- | --- | --- | --- |
| CPU 占用 | `/proc/stat` 每逻辑核 cpuN + 汇总 cpu 行 jiffy 差分 | 4 逻辑核 | per-core usage% |
| 负载/运行时长 | `/proc/loadavg`（列 1-3）、`/proc/uptime`（列 0≈156d） | 0.84 0.82 0.96 | loadavg、uptime |
| CPU 频率 | `/sys/devices/system/cpu/cpuN/cpufreq/scaling_cur_freq`（kHz） | 1199946 | 每核实时 MHz |
| 温度 | `/sys/class/hwmon/hwmon*/`（name+temp*_input/label）：coretemp `Package id 0` / `Core 0/1`；acpitz | 47000（毫℃） | 温度 |
| 电池电压/电流 | `/sys/class/hwmon/hwmon2/`（in0=9319 mV、curr1=0 mA） | 9.319 V | 功率估算 |
| 内存 | `/proc/meminfo`（MemTotal/MemAvailable/SwapTotal/SwapFree/Cached/SReclaimable/Shmem/SwapCached） | Total 5986316 kB | 用量/压力 |
| 内存压力 | `/proc/pressure/memory`（some/full avg10/60/300） | some 0.06 | PSI |
| 磁盘活动 | `/sys/block/sda/stat`（列 1/5=读/写请求，列 3/7=扇区，列 9/13=jiffies） | — | 速率差分 |
| 分区用量 | `df -k`（过滤真实块设备分区）、`lsblk -o NAME,SIZE,TYPE,MOUNTPOINT` | sda1 83.6G / | 列表 |
| 网络 | `/proc/net/dev`（iface: rx bytes 列 1、tx bytes 列 9） | wlan0 4.8G/4.2G | 速率差分、累计 |
| 本机 IP | `ip -brief addr` | wlan0 UP 192.168.49.136/24 | 列表 |
| 连接统计 | `ss -s`（头部 TCP/UDP 段） | — | Network 页统计 |
| 公网 IP | `curl -m 5 https://ifconfig.me`（回退 `https://api.ipify.org`） | 受限时 "—" | best-effort，120s 一次 |
| 电池 | `/sys/class/power_supply/BAT1/`（status/capacity/charge_now/full/full_design/current_now/cycle_count/manufacturer/model_name）、`/sys/class/power_supply/ACAD/online` | charge_full 1651/5040 mAh | 详情页 |
| Top 进程 | `ps -eo pcpu,pmem,comm --no-headers` 排序取前若干行 | — | Top 5 各页复用 |
| GPU | `lspci -nn`（VGA/3D/Display 行） | iGPU 8086:0416 + NV 10de:1299 | 列表；卸载率无 root-free 接口→只列硬件 |

**root-required（不做，仅注记）**：`smartctl -H -A`（实测 "Operation not permitted"）
→ S.M.A.R.T. 无值标注；dmidecode SPD/时序；fan*_input 本机无（标注无风扇）；
其他电压/电流轨 hwmon 无（本机仅 coretemp/acpitz/BAT1 电源三条）。

## 2. 采样算法与公式

### 2.1 CPU 占用（tick = 1000 ms）
```
busy_delta = Δ(user + nice + system + irq + softirq)
total_delta = Δ(user + nice + system + idle + iowait + irq + softirq)
usage% = 100 * busy_delta / total_delta（total_delta=0 时沿用上次值）
per-core：逐 cpuN 行；汇总行 cpu 另算
```

### 2.2 网络速率
```
rate = (bytes_now - bytes_prev) / dt（dt=1 tick）；回卷（<prev）按 0 处理
humanize：<1K → B/s；<1M → KB/s；否则 MB/s
```

### 2.3 磁盘速率
```
KB/s = Δsectors * 512 / dt；IOPS = Δ(列1+列5) / dt；in_flight 取原值（列 11）
```

### 2.4 电池
```
health% = charge_full / charge_full_design * 100（本机 1651/5040 = 32.7%，<80% 标黄）
功率 W  = in0(mV 借 μV 口径注意缩放) * curr1(μA)；udisks 交叉验证不引入
```

### 2.5 humanize 公共函数
`IstmHumanBytes(int64)` / `IstmHumanRate`：十进制 KB/MB（iStat Menus 口径），本地纯函数。

## 3. 刷新节拍（单 QTimer 1000ms 驱动分级）

| 周期 | 项 |
| --- | --- |
| 每 tick 1s | 顶条快照、CPU/Net/Disk 差分采样、IstatGraph push、当前活动页数值刷新 |
| 每 10 tick | Top 进程、df/lsblk、Sensors 全量表、Battery 详解 |
| 每 120 tick | 公网 IP 探测（失败立即显 "—"，不重试排栈） |
| 页不可见 | 仅非当前页停历史重绘（数据照采），currentChanged 驱动 |

## 4. IstatGraph 控件（Step2 引入）

```cpp
class IstatGraph : public QFrame {
    Q_OBJECT
public:
    void setRing(int cap);                 /* 默认 60 点 */
    void addSample(double v01);            /* [0,1]，越界 clamp */
    void paintEvent(QPaintEvent*);
private:
    QList<double> m_vals;
    int m_head;
};
```
- 绘制：等宽竖条 bar-chart；条色 CPU 绿 / Mem 蓝 / Net 青（上下双段 rx/tx） / Disk 橙；
- 仅 QPainter::drawLine/drawRect + QPen（Qt3/Qt4 双通），不依赖 stylesheet；
- 高度固定 42px，宽度随布局 stretch 拉伸；双列（上下行）模式用于 Network。
- 数据线程：tick 主线程 push（上游 /proc 差分均轻读，无工作线程）；
- 析构：QFrame 挂 UI 树自动销毁，Logic 定时器先 stop 再解挂避免野指针。

## 5. UI 指针包 IstatUi（逐页字段规划，结构演进式）

| 页 | 控件（Step） | 表头列 |
| --- | --- | --- |
| 顶条 | topCpu/topMem/topNet/topTemp/topBat（Step1 存在，Step2/3/5 点亮） | — |
| CPU(Step2) | usage 大字 + 总图；per-core 表 4 行（核名/频率/占用/温度）；loadavg/up/微码行；TopTable（进程/CPU%/MEM%） | Core, MHz, %, °C |
| Memory(Step2) | usage 大字+折算条；明细 kv（used/available/cached/SReclaimable/shmem）；swap 区（sda6+zram 分列）；PSI some/full avg10； 历史图；TopTable | — |
| Disk(Step4) | 分区表（Device/Type/Size/Used%/Mount）；sda 活动（KB/s 大字+IOPS+图）；SMART 注记 | Device, Type, Size, Used, Mount |
| Network(Step3) | IF 表（Iface/State/IP/RX 速率/TX 速率/累计 RX/累计 TX）；公网 IP 行 + ss 统计行；双列图 | Iface, State, IP, Rx, Tx |
| Sensors(Step5) | hwmon 表（Chip/Label/Value）；功率估算注记 | Chip, Label, Value |
| Battery(Step5) | kv 行（status/capacity/charge_now/full/health%/功率W/cycles/tech/manufacturer/model + ACAD）；健康<80% 黄字；电量史图 | — |
| About(Step6) | 版本、对照 iStat 6.50 实现范围说明、"需 root 未实现项"列表 | — |
| Notifications(Step6) | 无专用页：顶条相应段加 "⚠"，页内 note 行追加一次时间戳提示；最近 10 条归 About | — |

## 6. Notifications 规则（Step6）

```
阈值默认：CPU>90 连续 30s / 内存>90 连续 30s / 任一分区 >90% / 电量<15 且未接 AC
10s 检查一次；触发后同项 5min 内不重复；About 页保留最近 10 条历史提示
```

## 7. 文件架构

```
etapps/istat_menus/
├─ DESIGN.md           本文档（Step 落盘后同步补充“已实现”勾）
├─ plugin.pro/exe.pro  TARGET=istat_menus_ui；plugin Qt4 DESTDIR→plugins4/uiapps
├─ buildqt3.sh/qt4.sh  /opt/qt338sh · /opt/qt487sh；make -j1；顺次执行不并行
├─ main.cpp            三符号 + createIstatMainWindow
├─ exe.cpp             QApplication + show + exec（appsetup 仅 exe.pro）
├─ logic.h             IstatUi 指针包（全裸指针）+ IstatLogic(QObject, Q_OBJECT)
├─ logic.cpp           helpers（Istm 前缀）+ 采样 + onTick 分级调度 + fill*Page
└─ mainwin.cpp         顶条 + 7 页构建 + logic 接线（纯视图，无采集）
```

helpers（本地 static，前缀 Istm，cpuz 同源拷贝）：
`IstmReadFile`、`IstmRun`（qRuncmdCaptureOuterr 包装）、
`IstmSub / IstmRow / IstmValue / IstmNote / IstmBig / IstmTableInit /
IstmHumanBytes / IstmHumanRate`、`qUtf8Bytes`（Qt3 utf8 / Qt4 toUtf8）；
公共兼容层走 compat34/compatcore34（qNewBoxLayout、qSetLayoutMargin、qGetHomePath）。

## 8. 分步实施（每步完整 diff 预览 → y 落盘；qt3→qt4 顺次构建 + 冒烟）

| Step | 落盘 | 验收 |
| --- | --- | --- |
| 1 ✅（2026-11-10） | 九件套骨架 + DESIGN.md，7 占位页 + 顶条空 lab + 1s 定时器空转 | 双构建 exit=0 + exe 冒烟 |
| 2 ✅ | IstatGraph（60 点环形 QFrame）+ CPU 页（stat 差分/每核 MHz/温度/topology 映射/Top 表）+ Memory 页（meminfo/PSI//proc/swaps 分设备/zram+disk swap 注记）+ 顶条 CPU/MEM/Temp 点亮 | 频率/温度/图与 cat 数值对齐 |
| 3 ✅ | Network 页（per-IF 6 列表/双向双色速率图/本机 IP（ip -brief，10s）/公网 IP 120s best-effort/ss 统计/总速率大字）+ 顶条 NET 点亮 | 速率与 /proc/net/dev 差分对 |
| 4 ✅ | Disk 页（df -kP 真实分区表 5 列/10s、主块设备探测 nvme→sda、活动速率 KB/s+IOPS+in_flight 差分图 orange/SMART root 注记） | KB/s 量级复核 |
| 5 ✅ | Sensors 页（hwmon temp/in/curr/fan 全表扫描+无风扇注记）+ Battery 页（status/capacity/charge 三元/健康率/功率W/循环/厂型 + ACAD + 电量史图 blue）+ 顶条 🔋 点亮 | health%=32.7% 本机复核 |
| 6 ✅ | Notifications（CPU>90 或内存>90 连续 30s/分区>=90%/电量<15 且未接 AC；触发后 5min 冷却；About 页存最近 10 条）+ About 页充实 + 双终验 | 阈值触发注记可见 |
| 7 ✅ | Step7 增补：顶条本地时钟（Time 模块）+ About 世界钟 6 城（固定 UTC offset，无 DST 注记）+ CPU 页 GPU 行（lspci -knn 型号+driver，一次性）+ 网络流量 >5MB/s 连续 30s 告警（5min 冷却）+ 顶条电池三态图标（⚡充电/🔌AC 未充/🔋释电）；Weather 维持未接（官方付费源，本机公网受限） | 顶条时间走动 + iGPU/NV 两行 GPU + 大流量触发 About 记录 |

## 9. 风格与红线（沿用项目惯例）

- CJK/emoji 一律 `qFromUtf8()`；禁止 `\x` 十六进制 emoji；纯 ASCII 可裸用；
- 无 try-catch、无 throw；所有错误经返回值或空串 "—" 传递；
- if/for/while 恒 `{}`；include 顺序：logic.h → compat34.h → Qt → std；
- 不动 `build.sh` / go 服务端 / qltox 菜单代码（零改动即插即用）；
- 不 auto-commit（等用户指示）；moc_*/build-* 产物不入库。
