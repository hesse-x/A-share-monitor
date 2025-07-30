const St = imports.gi.St;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;
const Me = ExtensionUtils.getCurrentExtension();

let indicator;

function init() {
}

function enable() {
  // 创建面板按钮
  indicator = new PanelMenu.Button(0.0);
  
  // 容器样式：紧凑宽度，调整整体边距
  const container = new St.BoxLayout({
    vertical: true,
    style: "horizontal-align: center; padding: 1px 2px; min-width: 32px;"
  });
  
  // 第一行：800（字体稍大，行间距缩小）
  const line1 = new St.Label({
    text: "800",
    // 字体增大到12px，通过line-height缩小行间距
    style: "color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.1;"
  });
  
  // 第二行：100%（字体稍大，行间距缩小）
  const line2 = new St.Label({
    text: "100%",
    // 字体增大到12px，通过line-height缩小行间距
    style: "color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.1;"
  });
  
  container.add_child(line1);
  container.add_child(line2);
  indicator.add_child(container);
  
  // 添加到状态栏右侧
  Main.panel.addToStatusArea("fixed-numbers", indicator, 0, "right");
}

function disable() {
  if (indicator) {
    indicator.destroy();
    indicator = null;
  }
}

