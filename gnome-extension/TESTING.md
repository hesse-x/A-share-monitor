# GNOME Shell 扩展测试工具

虽然 GNOME Shell 扩展最终需要在 GNOME 环境中测试，但你可以使用以下工具进行本地测试：

## 测试工具

### 1. **gjs 直接测试** (推荐用于基本逻辑测试)

```bash
# 测试核心函数逻辑
gjs test_basic.js

# 测试网络请求 (注意：可能会遇到API限制)
gjs test_network.js
```

### 2. **Looking Glass 调试器** (GNOME Shell 内置)

- **如何打开**: 按 `Alt + F2`，输入 `lg` 并按回车
- **用途**: 在运行中的GNOME会话中调试扩展
- **功能**: 查看日志、错误、执行脚本

### 3. **GNOME Shell 重新加载**

```bash
# 查看当前GNOME版本
gnome-shell --version

# 重新加载GNOME Shell (Alt+F2，输入 'r')
# 注意：这会重新启动GNOME Shell，可能会关闭一些应用
```

### 4. **扩展开发工具**

```bash
# 安装扩展工具 (如果还没有)
sudo apt install gnome-shell-extension-tool

# 查看已安装的扩展
gnome-shell-extension-tool --installed

# 重新加载特定扩展 (需要扩展UUID)
gnome-shell-extension-tool --enable <extension-uuid>
```

## 测试文件说明

- **test_basic.js**: 测试核心函数逻辑（交易时间判断、数据显示等）
- **test_network.js**: 测试网络请求功能
- **test_functions.js**: 测试示例和用例

## 测试最佳实践

1. **先测试逻辑函数**: 使用 `gjs test_basic.js` 测试核心逻辑
2. **再测试网络功能**: 使用 `gjs test_network.js` 测试API请求
3. **最后在GNOME环境测试**: 在实际GNOME会话中加载扩展
4. **使用Looking Glass**: 实时查看日志和调试信息

## 注意事项

- 网络API可能会有反爬虫措施，测试时可能会遇到限制
- 某些GNOME API需要在实际GNOME环境中才能使用
- 扩展的完整功能需要在GNOME Shell环境中测试

## 常见问题

**Q: 网络测试返回403 Forbidden？**
A: 新浪财经API可能有反爬虫措施，这在实际GNOME环境中使用时通常可以正常工作。

**Q: 如何查看扩展的运行日志？**
A: 使用Looking Glass查看，或者检查 `journalctl`：
```bash
journalctl /usr/bin/gnome-shell -f
```

**Q: 如何调试扩展加载问题？**
A: 在Looking Glass的Extensions标签页中查看扩展状态和错误信息。