const St = imports.gi.St;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const ByteArray = imports.byteArray;

let indicator, line1, line2, updateInterval;
const STOCK_CODE = 'sh000001'; // 股票代码
let cachedData = null; // 缓存股票数据
let lastFetchTime = 0; // 上次获取数据的时间戳
let isInTradingPeriod = false; // 记录当前是否处于交易时间段

// 判断当前时间是否在交易时间段内
function isInTradingHours() {
    const now = new Date();
    const hour = now.getHours();
    const minute = now.getMinutes();
    const day = now.getDay(); // 0是周日，6是周六
    
    // 非工作日不交易
    if (day === 0 || day === 6) {
        return false;
    }
    
    // 上午交易时间: 9:15-11:35
    const isMorningSession = (hour === 9 && minute >= 15) || 
                           (hour > 9 && hour < 11) || 
                           (hour === 11 && minute <= 35);
    
    // 下午交易时间: 13:00-15:05
    const isAfternoonSession = (hour === 13) || 
                             (hour > 13 && hour < 15) || 
                             (hour === 15 && minute <= 5);
    
    return isMorningSession || isAfternoonSession;
}

// 使用Gio 2.0兼容的SocketClient实现
function getSinaStockPrice(stockCode) {
    const host = "hq.sinajs.cn";
    const port = 80;
    const client = new Gio.SocketClient();
    let connection = null;

    try {
        log(`[GJS] 连接到 ${host}:${port}...`);
        connection = client.connect_to_host(host, port, null);

        if (!connection) {
            throw new Error("无法建立连接");
        }

        const path = `/list=${stockCode}`;
        const request = [
            `GET ${path} HTTP/1.1`,
            `Host: ${host}`,
            'User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36',
            'Referer: https://finance.sina.com.cn/',
            'Connection: close',
            '',
            ''
        ].join('\r\n');

        log(`[GJS] 发送请求: ${request.substring(0, Math.min(request.length, 50))}...`);
        const outputStream = connection.get_output_stream();

        const requestBytes = GLib.Bytes.new(request);
        let writeResult = outputStream.write_bytes(requestBytes, null);
        
        if (writeResult <= 0) {
            throw new Error("请求发送失败");
        }

        const inputStream = connection.get_input_stream();
        const bufferSize = 512; 
        const space = [];
        
        // 循环读取所有响应字节
        while (true) {
            let bytesReadChunk = inputStream.read_bytes(bufferSize, null);
            if (!bytesReadChunk || bytesReadChunk.get_size() === 0) {
                break; // 读取完毕
            }

            let buffer = bytesReadChunk.get_data();
            space.push(GLib.convert(buffer, "UTF8", "GBK"));
            if (bytesReadChunk.get_size() < bufferSize) {
                break; // 读取完毕
            }
        }
        
        let resultStr = space.toString();
        return resultStr.toString().split("\"")[1].split(","); 
    } catch (e) {
        log(`[GJS 错误] ${e.message}`);
        throw e;
    } finally {
        if (connection) {
            connection.close(null);
            log("[GJS] 连接已关闭。");
        }
    }
}

// 为数值添加符号
function addSign(value) {
    return value >= 0 ? `+${value}` : `${value}`;
}

// 解析并更新显示
function updateDisplayWithData(data) {
    if (!data || data.length < 4) {
        line1.set_text('数据错误');
        line2.set_text('---');
        return;
    }
    
    // 解析股票数据
    const currentPrice = parseFloat(data[3]);
    const preClosePrice = parseFloat(data[2]);
    const change = currentPrice - preClosePrice;
    const percentage = ((change / preClosePrice) * 100).toFixed(2);
    const textColor = change < 0 ? "#00ff00" : "#ff0000";
    
    // 更新UI
    line1.set_text(`${currentPrice.toFixed(2)}(${addSign(change.toFixed(2))})`);
    line2.set_text(`${addSign(percentage)}%`);
    line1.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
    line2.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
}

// 更新显示逻辑
async function updateDisplay() {
    try {
        const currentTradingStatus = isInTradingHours();
        
        // 状态变化时（从非交易到交易，或从交易到非交易）
        if (currentTradingStatus !== isInTradingPeriod) {
            isInTradingPeriod = currentTradingStatus;
            log(`[GJS] 交易状态变化: ${isInTradingPeriod ? '进入交易时间' : '进入非交易时间'}`);
            
            // 状态变化时强制获取一次数据
            const freshData = getSinaStockPrice(STOCK_CODE);
            cachedData = freshData;
            lastFetchTime = Date.now();
            updateDisplayWithData(freshData);
        } 
        // 交易时间：定期更新数据
        else if (currentTradingStatus) {
            const freshData = getSinaStockPrice(STOCK_CODE);
            cachedData = freshData;
            lastFetchTime = Date.now();
            updateDisplayWithData(freshData);
        } 
        // 非交易时间：使用缓存数据，不重新获取
        else {
            if (cachedData) {
                updateDisplayWithData(cachedData);
            } else {
                // 如果还没有缓存数据，获取一次
                const freshData = getSinaStockPrice(STOCK_CODE);
                cachedData = freshData;
                lastFetchTime = Date.now();
                updateDisplayWithData(freshData);
            }
            log(`[GJS] 非交易时间，使用缓存数据 (上次更新于: ${new Date(lastFetchTime).toLocaleTimeString()})`);
        }
    } catch (e) {
        log(`错误: ${e.message}`);
        line1.set_text('获取失败');
        line2.set_text('---');
    }
}

function init() {}

function enable() {
    indicator = new PanelMenu.Button(0.0);
    
    const container = new St.BoxLayout({
        vertical: true,
        style: "horizontal-align: center; padding: 1px 3px; min-width: 80px;"
    });
    
    line1 = new St.Label({ text: "0.00(+0.00)", style: "color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;" });
    line2 = new St.Label({ text: "+0.00%", style: "color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;" });
    
    container.add_child(line1);
    container.add_child(line2);
    indicator.add_child(container);
    
    Main.panel.addToStatusArea("stock-ticker", indicator, 0, "right");
    
    // 初始化状态判断
    isInTradingPeriod = isInTradingHours();
    updateDisplay();
    
    // 30秒检查一次（交易时间更新数据，非交易时间只检查状态不更新数据）
    updateInterval = GLib.timeout_add_seconds(
        GLib.PRIORITY_DEFAULT,
        30,
        () => { updateDisplay(); return true; }
    );
}

function disable() {
    if (updateInterval) GLib.source_remove(updateInterval);
    if (indicator) indicator.destroy();
    indicator = line1 = line2 = updateInterval = cachedData = null;
}

