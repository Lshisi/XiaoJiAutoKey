# 引擎自动化回归

本工具把模式规则和历史回归条件沉淀为可重复执行的命令行检查，不启动正式程序，不依赖界面操作，不使用 QtTest 或测试类体系。

**工具构建成功不代表用例通过；以对应驱动、场景目录和执行器版本的实际执行报告为准。**

## 维护契约

### 维护入口与行为预期

- 不编写测试类，不引入 QtTest 测试类体系；沿用 `verification/engine/` 的函数式检查、数据化场景和输出记录适配器，直接调用生产引擎，不复制一套模式实现。
- 按键模式规则见 `docs/key-mode-rules.md`，自动化覆盖、运行方式和边界见 `docs/engine-verification.md`。行为预期以已确认的产品规则为准，不根据错误实现反推预期，也不恢复版本专属手工回归清单。
- 修改按键模式、宏、暂停/挂起、按压优先、共享修饰键、来源仲裁、WASD 保护、武学辅助或冲突校验时，必须同步维护相关单用、组合、复杂状态和非法配置场景；修复 BUG 时保留稳定场景 ID，将问题沉淀为可重复执行的回归条件。
- 常见矩阵维护在 `verification/scenarios/generate-catalog.mjs`，重新生成后将 `verification/scenarios/catalog.json` 一起提交；不要只改生成结果。新增模式须扩展单用矩阵、两两组合及优先策略、冲突预期、随机状态模型和覆盖文档，不能只验证新增模式的独立路径。
- 断言须检查实际输出、按下/抬起配对、共享组件/来源归属、暂停/停止后的静默和最终释放，不能只看运行标志。最终 `ReleaseAll` 只是安全兜底，不能代替场景正常释放的证据。

### 执行授权与结果判定

- 默认不主动运行测试；仅在用户明确要求时执行现有场景，不因修改代码、构建、提交或推送而自动启动验证。验证工具不得接入正式程序启动流程、默认构建执行步骤或后台定时任务。
- `recording` 仅证明生产引擎输出请求和状态逻辑，不代表 DD/小叽驱动通过。用户要求真实驱动验证时，DD 和小叽使用同一适用场景串行执行，入口为 `scripts/verify-engine.ps1`，必须显式启用 `-AllowRealInput`；执行前确认正式程序及其他执行器已关闭，并说明键鼠拦截和 Escape 中止方式。
- 只使用已安装、已运行且权限就绪的驱动环境；不擅自提权、安装/卸载驱动、启动驱动服务、修改签名或系统安全设置，不启动正式界面或游戏来补足环境。
- 构建成功、列出场景和记录后端通过均不能算真实驱动通过。结论必须对应实际驱动、场景定义、执行器版本和报告；`NOT_VERIFIED`、`BLOCKED`、`CONTAMINATED`、`RUNNING`、中止及超时均不得计为通过。
- 保留失败、种子、操作序列、事件和场景快照，不自动重跑用例掩盖失败，不通过放宽断言、关闭污染检测或删除场景换取通过。修正场景或执行器后显式复测，保留原始结果并说明变更；报告写入的有界 I/O 重试不属于用例重跑。
- 引擎验证不等同于真实物理设备 WM_INPUT、完整应用注入反馈过滤、真实前台进程识别、目标游戏接收或反作弊兼容性验证；未实际验证的范围必须明确保留，不能声称穷举所有配置和线程交错。

### 用户配置与发布隔离

- 不改写用户 `config.json`；配置迁移仅操作临时副本，前后核对受保护文件，发现变化只报告，不擅自恢复用户文件。
- 验证构建及执行报告默认放在已忽略的 `build-qt6/engine-verification/`，与主程序 Release 隔离。提交执行器、场景、去用户化样本和文档，不提交本机报告、捕获日志或生成的二进制，也不将验证工具及其产物放入发布 zip。

## 组成

- `verification/engine/`：执行器、记录型输出适配器、通用断言、历史回归函数。
- `verification/fixtures/`：去用户化的回归配置和提示音迁移样本，不依赖用户配置或旧构建目录。
- `verification/scenarios/catalog.json`：受版本管理的数据化用例，包含初始配置、操作步骤、预期输出、分类和模式标签。
- `verification/scenarios/generate-catalog.mjs`：维护常见场景矩阵的生成源；修改后运行 `node verification/scenarios/generate-catalog.mjs` 并一起提交 JSON。执行检查不需要 Node.js。
- `scripts/verify-engine.ps1`：构建、筛选、串行调度、超时保护和报告入口。

构建使用独立的 `build-qt6/engine-verification/`，默认结果放在其 `runs/时间-随机标识/` 下；与主程序 Release 目录分离，不属于发布包。没有新增默认构建任务、启动检查或后台定时任务。

驱动沿用 `plugins/driver-dd/` 和 `plugins/driver-xiaoji/` 的正式插件构建目标，通过生产 `NativePluginDriver` 连接，不再引用迁移前的 `src/core/DdDriver.cpp` 等路径。构建只生成插件及其管理组件，不执行安装或启动服务；真实驱动运行时将所选插件复制到该轮报告目录的 `plugin/`，插件数据也隔离在该目录内。

## 分层与结论

| 后端/入口 | 实际验证 | 不代表什么 |
| --- | --- | --- |
| `recording` | 当前生产 RepeaterEngine、MacroEngine、InputDriverRouter 的状态和输出请求，接内存记录适配器 | 没有连接 DD/小叽，没有向系统发送输入；不是驱动通过 |
| `dd`、`xiaoji` | 相同适用场景通过生产插件适配器使用正式驱动，低级键鼠钩子捕获并拦截系统事件 | 不是插件中心安装/升级、目标游戏接收、Raw Input 或反作弊兼容性验证 |
| K3 raw-signal | 公开 RawInputManager 信号进入生产协调器 | 绕过 WM_INPUT 解析和实际设备筛选 |
| K3 real-hook | 真实驱动事件进入生产 GlobalInputHook | 不等同于完整应用自身输入过滤闭环 |
| A 系列 | 武学助手的 3.1 秒引擎计时、取消、关闭短按和手动接管 | 不证明游戏已经进入/退出连发 |

仍使用生产线程和真实时间，不引入第二套模式实现或虚拟调度器。记录适配器只记录组件输出，不重新实现选择、暂停、共享组件或来源仲裁。

## 覆盖范围

数据化目录共 333 项：单场景 138、组合场景 90、复杂场景 52、配置规则校验 52、带种子随机游走 1。另保留历史引擎回归 71 项和声音配置迁移 2 项；真实钩子项能否执行取决于所选后端。数量表示已定义的检查，不表示已执行通过。

| 分组 | 内容 |
| --- | --- |
| `single` | 五种按键模式分别使用普通键、左右 Ctrl/Shift/Alt、修饰组合和常用鼠标键；启停、暂停、挂起、恢复、停止后重启 |
| `single` | 独立选择/防误停、切换防抖/跳过停用段/记忆位置、停用/空配置、最小与长间隔、组合两种释放顺序、WASD 保护 |
| `single` | 宏有限循环、开关/按住触发、鼠标/组合触发、点击/保持/按下与松开/等待/双向滚轮、停用步骤 |
| `composite` | 五种模式全部 15 种无序两两组合（含同模式），分别检查并发、按压优先、排除按压优先 |
| `composite` | 共享修饰键、单独修饰键长按与普通连发；五种按键模式分别和两种宏触发模式组合，同键/不同键、两个启动顺序、来源接管 |
| `complex` | 多个暂停条件、按压优先和挂起叠加，避免提前恢复；暂停中替换绑定或停止；五种模式与宏一起运行 |
| `complex` | 各宏动作中暂停/挂起后恢复或停止；点击/滚轮不重复；武学助手引擎时序 |
| `validation` | 同主键模式冲突矩阵、共享/不兼容修饰键、重复项、停用项、触发歧义；宏未释放/无对应按下/重复按下/非法时长/滚轮/保留组合等 |
| `stress` | 固定种子的状态模型随机游走：选择、切段、防抖、按压、暂停、挂起、停止重启，检查模型状态及实际输出 |
| `regression` | 已有 K1–K5、G1–G5、M1–M4 回归条件，保留稳定 ID；G6 游戏行为明确标为未验证 |
| `config` | C1 旧声音字段转换、新字段优先级、音量 0、自定义路径、保存后重读；只操作临时副本，不播放声音 |
| `external` | WM_INPUT、完整应用反馈过滤、真实前台识别和游戏接收：当前均为 NOT_VERIFIED |

每个数据化用例结束都会停止两种引擎并检查捕获流无遗留按下；停止后安静窗口、共享修饰键保持、逻辑选择和来源归属有各自的显式断言。最终驱动 ReleaseAll 是安全兜底，不把它当成场景本身正常释放的证据。

这是对**已定义规则和常见组合**的覆盖，不是任意数量配置、无限操作排列、每个线程交错和全部外部环境的数学穷举。配置校验覆盖的是共用模式冲突和宏验证函数，不代表所有 UI 编辑入口或控制键占用校验均已覆盖。

## 使用

在仓库根目录的 PowerShell 运行。默认使用项目规定的独立 CMake 和静态 Qt；其他安装位置可传入 `-CMakePath`、`-QtPrefixPath`。工具仅支持 Windows。

只构建，不执行任何场景：

```powershell
.\scripts\verify-engine.ps1 -BuildOnly
```

列出筛选结果，不连接驱动、不注册钩子、不执行场景：

```powershell
.\scripts\verify-engine.ps1 -List
.\scripts\verify-engine.ps1 -List -Suite composite
```

先用记录后端运行短回归，再运行完整引擎目录：

```powershell
.\scripts\verify-engine.ps1 -Suite single -Tag smoke
.\scripts\verify-engine.ps1 -Driver recording -Suite all
```

按分组、标签或 ID 子串筛选；筛选条件取交集，大小写敏感：

```powershell
.\scripts\verify-engine.ps1 -Suite composite
.\scripts\verify-engine.ps1 -Tag longPress
.\scripts\verify-engine.ps1 -Case K2-02-mod160
.\scripts\verify-engine.ps1 -Case P-toggle-hold-priority
```

在**已经安装并启动对应驱动的管理员 PowerShell** 中串行检查 DD、小叽：

```powershell
.\scripts\verify-engine.ps1 -Driver dd,xiaoji -Suite all -AllowRealInput
```

驱动默认不会运行，脚本和执行器两层都要求显式允许。开始前关闭正式程序及其他执行器，松开所有键。真实驱动轮期间暂时不要操作键鼠；键盘及鼠标按钮/滚轮在捕获期间会被拦截，Escape 请求中止。不要在真实驱动轮期间进行其他工作。

不自动提权、不安装/卸载或启动驱动服务、不更改签名/安全设置。DD 要求现有 `dd.key.96688`、`dd.mou.96688` 服务已经运行；可用 `-DdRuntimeDirectory` 指定 DD DLL 所在目录，默认为 `plugins/driver-dd/driver/`。所选运行库只复制到该轮的插件副本，不覆盖源码、已安装插件或用户配置。

随机序列复现和显式多轮执行：

```powershell
.\scripts\verify-engine.ps1 -Suite stress -Seed 20261123
.\scripts\verify-engine.ps1 -Case S-toggle-keyboard -Repeat 3
```

`-Repeat` 保留每轮独立结果；不因后一次通过而覆盖前一次失败，没有隐式自动重试。随机游走使用固定种子重建动作顺序，实际线程调度时间仍可能不同。

数据化场景失败时保存 `repro-*.json`。用原报告里的种子运行该独立目录：

```powershell
.\scripts\verify-engine.ps1 -CatalogPath '<上次输出目录>\repro-xxxxxxxxxxxx.json' -Suite complex -Seed 117
```

指定 `-CatalogPath` 时只执行该文件中的数据化场景，不混入历史回归或外部环境占位项；仍可按分组、ID 和标签筛选。复现时选择该场景实际所属分组，或直接用 `-Suite all`。直接调用执行器时，对应参数为 `--catalog-only`。当前保存原场景和实际随机操作日志，**尚未实现自动缩减最小失败序列**；可先编辑复现副本缩减步骤，再纳入固定回归。

## 报告与退出码

每次创建新目录，不覆盖上次结果：

- `manifest.json`：提交、工作区是否有改动、执行器/场景 SHA-256、参数、每驱动每轮结果，以及受保护文件前后哈希。
- `catalog.snapshot.json`：本轮使用的原始场景目录快照；后续修正目录时仍可按原定义审计旧结果。
- `report.md`：本轮各驱动状态摘要；详细覆盖矩阵位于各驱动的 `results.json`。
- `results.json`：每项预期、实际状态快照、时间线、断言数、清理情况、按模式/分组统计、种子和未覆盖范围。真实驱动的 `driver-connect` 元数据包含插件 ID、版本及实际加载 DLL 的 SHA-256。执行中按场景检查点保存，异常退出可能留下 RUNNING，不能算通过。
- `events.csv`：记录型输出或真实钩子事件；两者在报告中明确区分。
- `owners.csv` 与 `sourceStages`：历史同键仲裁场景的来源采样及实际输出节奏交叉检查。
- `console.log`、`stderr.log`：原始执行日志。

| 退出码 | 含义 |
| ---: | --- |
| 0 | 所选检查通过，或仅列举（报告会写 LIST_ONLY，不会写通过） |
| 1 | 断言失败 |
| 2 | 中止、基础设施错误或超时；强制结束后的释放状态不能保证 |
| 3 | 所需权限或驱动环境未就绪 |
| 4 | 有 NOT_VERIFIED，不能把整轮称为完整通过 |
| 5 | 捕获受非场景输入污染；安全中止可能同时产生退出码 2 |
| 6 | 筛选未匹配任何场景 |

多个状态并存时优先返回失败/中止；完整目录包含 G6 和外部环境未验证项，因此即使全部可执行检查通过，也会明确返回未完整验证，而非假装全绿。

记录后端不能执行真实全局钩子 K3 子项，这些项报告 NOT_VERIFIED。真实驱动轮能执行这部分，但依然不包含完整应用反馈链或真实 WM_INPUT。

## 安全与稳定性

- 不读写运行配置内容，只对常见配置位置、场景目录和样本做前后哈希比对；发现变化只报告，不擅自恢复用户文件。
- 每个场景有超时，外部脚本还有进程看门狗；超时先请求取消，再做有界等待。被迫终止时明确报告释放未确认，并停止后续驱动轮，不能保证设备或系统失效时仍能释放。
- 真实驱动串行运行，执行器用命名互斥对象防止多个验证进程同时运行；脚本检查正式程序未运行。验证期间不要另行启动正式程序。
- 不用连续重试掩盖不稳定。捕获出现用例之外的键时保存事件并标为 CONTAMINATED，后续场景停止。钩子无法辨认使用相同键的物理输入来源，因此仍需要安静的硬件验证环境。
- 观察窗口先允许有限的在途脉冲收尾，再检查输出或静默；窗口和容差在用例中明确，不在失败后自动放宽。运行期间请求 1ms 定时分辨率，正常退出恢复。
- WASD 防卡不是暂停输出：移动期间仅将开关、独立和切换模式的间隔下限提高到 35ms，按压和长按不因此停止；五种模式分别检查降频或持续输出，以及松开移动键后的恢复。
- 报告以原子替换方式保存；遇到 Windows 短暂文件占用时仅对报告写入做有界重试，不重新执行场景。原始事件和来源采样增量保存，避免完整目录运行时反复重写全部历史记录。
- 武学助手 A 系列需要现有前台窗口稳定；窗口改变时中止该轮并保留污染结果。不会新建/切换游戏窗口，也不声称游戏响应已验证。

## 扩展场景

优先在 JSON 中添加新场景，或修改矩阵生成源后重新生成。通用场景结构如下：

```json
{
  "schemaVersion": 1,
  "cases": [{
    "id": "custom-toggle-stop",
    "group": "single",
    "description": "Toggle emits and becomes quiet after stop",
    "modes": ["toggle"],
    "tags": ["single", "toggle"],
    "profile": {"bindings": [{"id": 1, "key": "K:65", "mode": "toggle", "delayMs": 90}]},
    "steps": [
      {"op": "start"},
      {"op": "observe", "ms": 260, "outputs": [{"key": "K:65", "minDown": 2}]},
      {"op": "stop"},
      {"op": "observe", "ms": 260, "outputs": [{"key": "K:65", "maxDown": 0, "held": false}]}
    ]
  }]
}
```

常用操作：`start`、`stop`、`down/up/tap`（target 为 repeat/macro/both）、`suspend`、`wait`、`mark`、`observe`、`counts`、`state`、`bindings`、`setup`、`macroStop`、`mixed`、`source`、`foreground`、`repeat`。专用验证支持 `validatePair`、`validateMacro`、`randomWalk`。

`observe` 默认为先等待 25ms 收尾，再观察 260ms；可显式指定 `settleMs`。`counts` 默认检查最近一次 mark/observe 之后的事件，`from: "start"` 检查整个当前场景。`held` 检查捕获事件的最终状态，而不是只相信引擎“正在运行”。

`bindings` 只更新当前绑定；`setup` 停止并完整切换场景配置。`foreground` 仅调用引擎的前台允许接口，不是实际窗口切换。`randomWalk` 是针对既定五模式配置的专用状态模型，不能套到任意绑定上。

新增模式时同时更新单用矩阵、15 对组合矩阵的模式集合、冲突预期、随机状态模型和文档。修复 BUG 时先加入能表达正确语义的稳定场景，不根据错误实现反推“预期”。
