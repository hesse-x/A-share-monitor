# Stock Monitor

一个使用 Rust、egui 和 WGPU 编写的 A 股/股指期货价差悬浮监控工具。

## 功能

- 透明、无边框、始终置顶的 GPU 渲染窗口
- 折线图和纯数据两种显示模式
- 股票超过 5 个时分页轮播，纯数据模式逐个轮播
- Tokio + reqwest 异步并发行情请求，不阻塞 UI 或渲染线程
- 支持 `sh`/`sz` 股票以及 `IH`、`IF`、`IC`、`IM` 近月/次季价差
- 右键刷新、配置、切换模式或退出
- Linux Wayland/X11 和 Windows WGPU 后端

## 运行

需要稳定版 Rust 工具链：

```shell
cargo run --release -- stock.config
```

不传配置文件时默认监控 `sh000001`，刷新间隔为 60 秒：

```shell
cargo run --release
```

## 配置

```yaml
code:
  sh601939
  sh000001
  IF-Front
  IM-Next

freq:
  60s
```

刷新间隔支持 `ms`、`s` 和 `m`。程序只在工作日的 09:30-11:30、
13:00-15:00 自动刷新；启动和右键手动刷新不受交易时间限制。

## 操作

- 左键拖动窗口
- 右键打开菜单
- 配置窗口中的修改在点击“应用”后生效
- 折线图最多同时显示 5 个标的，每 5 秒轮换下一页

Wayland 下的窗口置顶、位置和透明效果最终取决于桌面合成器策略。

## 检查

```shell
cargo fmt --all -- --check
cargo test --locked
cargo clippy --locked --all-targets -- -D warnings
```
