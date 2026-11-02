# sqlitebrowser

SQLite 数据库浏览器（etapps 插件应用），参照 DB Browser for SQLite (DB4S) 功能集移植，Qt3/Qt4 双构建。

## 功能

1. **库文件**：打开 / 新建（按钮版） / 内存库 / 关闭 / VACUUM / 只读模式 / 最近文件菜单
2. **结构 tab**：tables / indexes / views 位列示，DDL 查看；建表 / 改表（改名、加列、删列、改列） / 删表 / 建索引 / 删索引
3. **浏览数据 tab**：表/视图数据 50 行分页、点击列头排序、过滤框（expr LIKE '%s%' 或 WHERE）、单元格双击编辑（文本 / 数字 / NULL / BLOB hex `X'..'`）、追加行 / 删除行
4. **执行 SQL tab**：多 SQL 编辑页（+ / 删除页），单页即时执行，结果表格 + 执行信息（耗时、行数），错误提示；行右键跳转浏览数据（暂用双击列单元格定位更直接）
5. **CSV 导入导出**：字段含逗号/引号/换行自动加引号，BLOB 用 `X'..'` 字面量往返；导入节流失败回滚（事务）
6. **SQL dump 导入导出**：.sql 文本导出为 CREATE + INSERT 序列；导入按分号切分逐条执行（跳过注释 BEGIN/COMMIT/PRAGMA 行）
7. **Pragma tab**：page_size / journal_mode / auto_vacuum / foreign_keys / user_version 查看+应用，quick_check / integrity_check 一键核验
8. **SQL 日志 tab**：所有 SQL 变更语句回显（只读记录），可清空

BLOB 支持十六进制查看与编辑（编辑格输入 `X'0A0B..'` 或空串清空）。

## 构建

```sh
bash buildqt3.sh   # Qt3（/opt/qt338sh）: qltox/plugins/uiapps/libsqlitebrowser_ui.so + build-exe/sqlitebrowser_ui
bash buildqt4.sh   # Qt4（/opt/qt487sh）: qltox/plugins4/uiapps/libsqlitebrowser_ui.so + build-exe4/sqlitebrowser_ui
```

不要并行编译（buildqt3 先、buildqt4 后；同一 .o 目录约定）。

## 运行

- 服务内：把 `libsqlitebrowser_ui.so` 放入 `qltox/plugins/uiapps/`（Qt3）或 `qltox/plugins4/uiapps/`（Qt4），由插件管理器加载；编辑框直接出现。
- 独立 exe：`./build-exe/sqlitebrowser_ui` / `./build-exe4/sqlitebrowser_ui`，可选 argv[1] 为 .db 路径直接打开。

## 文件

| 文件 | 说明 |
|------|------|
| dblogic.h | SQLite 数据层 DbSession + Qt3/Qt4 兼容 typedef 与小工具声明 + SqlBrowserLogic（Q_OBJECT） |
| dblogic.cpp | 数据层实现（sqlite3 C API 直连）+ 兼容小工具实现 |
| sbui.cpp | SqlBrowserLogic 槽实现 + 全部对话框（选表/编辑格/建表/改表/建索引）+ 工厂与 exe 直开辅助 |
| mainwin.cpp | 主窗工厂（五 tab UI 布局、信号连接、最近文件、状态持久化恢复） |
| main.cpp | 插件入口（plugin_api.h 五函数） |
| exe.cpp | 独立 exe 入口（appsetup 初始化） |
| plugin.pro / exe.pro | 插件 / 独立工程文件（Qt3/Qt4 自动区分） |

## 设计要点

- 数据层与 UI 分离：`DbSession`（纯 C++，无 QWidget 依赖）+ `SqlBrowserLogic`（QObject + 槽）+ `mainwin.cpp`（工厂式 UI），UI 只通过逻辑对象交互。
- 状态持久化：`~/.config/sqlitebrowser/state.json`（cJSON）——最近文件列表、SQL 页文本、浏览表、浏览过滤条件。
- Qt3/Qt4 差异集中在 `compat34.h` 宏与 `sb*()` 小工具（QTable/QTableWidget、QListBox/QListWidget、QTabWidget、QTextEdit、QFileDialog、QChar 签名、QMessageBox 枚举签名等），逻辑层不直接触碰平台差异点位。
- 浏览数据分页支持 rowid 有无两种模式（WITHOUT ROWID/Old SQLite 视图只读 + 代替定位）；视图页面只读浏览。
- 不实现远程数据库（DB4S DBHub）与图表 — 项目不需要。
