const St = imports.gi.St;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;
const GLib = imports.gi.GLib;

let indicator, line1, line2, updateInterval;

// 获取price和change的函数（包含负数情况）
function getAandchange() {
    // 示例：生成可能包含负数的change值
    const price = Math.floor(Math.random() * 900) + 100; // 100-999的随机正数
    // change可能为正或负（范围：-price/2 到 price/2）
    const change = Math.floor((Math.random() - 0.5) * price); 
    
    return { price, change };
}

// 为数值添加符号（正数显式添加+号）
function addSign(value) {
    return value >= 0 ? `+${value}` : `${value}`;
}

// 计算并更新显示内容（带符号并根据正负值设置颜色）
function updateDisplay() {
    const { price, change } = getAandchange();
    // 确定颜色：change为负则绿色，为正则红色
    const textColor = change < 0 ? "#00ff00" : "#ff0000";
    
    // 第一行格式：price(±change)，显式添加符号
    const signedchange = addSign(change);
    const line1Text = `${price}(${signedchange})`;
    line1.set_text(line1Text);
    line1.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
    
    // 第二行格式：±百分比，显式添加符号
    const percentage = (change / price * 100).toFixed(1);
    const signedPercentage = addSign(percentage);
    const line2Text = `${signedPercentage}%`;
    line2.set_text(line2Text);
    line2.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
}

function init() {
}

function enable() {
    // 创建面板按钮
    indicator = new PanelMenu.Button(0.0);
    
    // 创建垂直容器（适当加宽以容纳符号）
    const container = new St.BoxLayout({
        vertical: true,
        style: "horizontal-align: center; padding: 1px 3px; min-width: 70px;"
    });
    
    // 第一行：price(±change)
    line1 = new St.Label({
        text: "0(+0)",
        style: "color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;"
    });
    
    // 第二行：±百分比
    line2 = new St.Label({
        text: "+0.0%",
        style: "color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;"
    });
    
    container.add_child(line1);
    container.add_child(line2);
    indicator.add_child(container);
    
    // 添加到状态栏右侧
    Main.panel.addToStatusArea("a-da-display", indicator, 0, "right");
    
    // 立即更新一次
    updateDisplay();
    
    // 设置定时更新（每2分钟=120秒）
    updateInterval = GLib.timeout_add_seconds(
        GLib.PRIORITY_DEFAULT,
        2,
        () => {
            updateDisplay();
            return true;
        }
    );
}

function disable() {
    // 清除定时器
    if (updateInterval) {
        GLib.source_remove(updateInterval);
        updateInterval = null;
    }
    
    // 销毁指示器
    if (indicator) {
        indicator.destroy();
        indicator = null;
    }
    line1 = null;
    line2 = null;
}

