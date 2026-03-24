// 如何测试你的GNOME扩展函数

// 功能1: 测试交易时间判断
const Lib = imports.gi.GLib;

// 你可以直接创建测试文件测试这个函数
// 保存为 test_trading_hours.js

function testTradingHours() {
    // 模拟不同时间
    const testCases = [
        "2024-03-19T09:30:00", // 交易时间内（上午）
        "2024-03-19T11:40:00", // 交易时间外（上午）
        "2024-03-19T14:00:00", // 交易时间内（下午）
        "2024-03-19T15:10:00", // 交易时间外（下午）
        "2024-03-17T10:00:00", // 周末
    ];

    for (const timeStr of testCases) {
        // 这里需要模拟时间，实际运行时系统时间
        const date = new Date(timeStr);
        print(`测试时间: ${timeStr}`);
        // 调用 isInTradingHours() 函数
    }
}

// 功能2: 测试数据解析
function testStockDataParsing() {
    // 新浪API返回的数据格式示例
    const sampleData = "var hq_str_sh688256=\"寒武纪,123.45,122.50,124.68,125.00,122.30,124.68,124.69,125.00,129768450,16217380000.00,0.00,0.00,0.00,0.00,122.51,123.45,122.50,123.00,123.50,122.80,124.00,0.00,0.00,0.00,0.00,-1,0,0,0,0,2024-03-19,15:00:00,00\";";

    // 提取数据部分
    const dataPart = sampleData.split('"')[1].split(",");
    print("解析后的数据数组长度:", dataPart.length);
    print("当前价格 (index 3):", dataPart[3]);
    print("昨收价格 (index 2):", dataPart[2]);
}

print("创建测试环境成功！");