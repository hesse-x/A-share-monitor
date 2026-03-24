// 独立的测试脚本 - 可以直接运行测试核心逻辑
const GLib = imports.gi.GLib;

// 复制核心函数到这里进行测试
function isInTradingHours() {
    const now = new Date();
    const day = now.getDay();
    const hour = now.getHours();
    const minute = now.getMinutes();
    const timeMinutes = hour * 60 + minute;

    if (day === 0 || day === 6) return false;

    const morningStart = 9 * 60 + 15;
    const morningEnd = 11 * 60 + 35;
    const afternoonStart = 13 * 60;
    const afternoonEnd = 15 * 60 + 5;

    return (timeMinutes >= morningStart && timeMinutes <= morningEnd) ||
           (timeMinutes >= afternoonStart && timeMinutes <= afternoonEnd);
}

function addSign(value) {
    return value >= 0 ? `+${value}` : `${value}`;
}

function updateStockDisplay(currentPrice, preClosePrice) {
    const change = currentPrice - preClosePrice;
    const percentage = ((change / preClosePrice) * 100).toFixed(2);
    const color = change < 0 ? "#00ff00" : "#ff0000";

    return {
        line1: `${currentPrice.toFixed(2)}(${addSign(change.toFixed(2))})`,
        line2: `${addSign(percentage)}%`,
        color: color
    };
}

// 测试函数
function runTests() {
    print("=== 开始测试 ===");
    print("当前时间:", new Date().toString());

    // 测试1: 交易时间检查
    print("\n--- 测试交易时间 ---");
    const isTrading = isInTradingHours();
    print("当前是否在交易时间内:", isTrading ? "是" : "否");

    // 测试2: 股票数据显示测试
    print("\n--- 测试股票数据显示 ---");
    const testCases = [
        {name: "上涨", current: 125.68, previous: 122.50},
        {name: "下跌", current: 120.00, previous: 122.50},
        {name: "平盘", current: 122.50, previous: 122.50},
        {name: "大涨", current: 130.00, previous: 122.50},
    ];

    testCases.forEach(testCase => {
        const result = updateStockDisplay(testCase.current, testCase.previous);
        print(`${testCase.name}: 当前=${testCase.current.toFixed(2)}, 昨收=${testCase.previous.toFixed(2)}`);
        print(`  显示内容: ${result.line1} | ${result.line2} (颜色: ${result.color})`);
    });

    print("\n=== 测试完成 ===");
}

// 运行测试
try {
    runTests();
} catch (e) {
    printError(`测试失败: ${e.message}`);
}