const St = imports.gi.St;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const ByteArray = imports.byteArray;

// 引入 GJS 的 Promise 工具函数，用于将 GIO 异步模式转为 Promise
// 在 GNOME Shell 扩展中，这个工具通常位于 misc.extensionUtils 或直接使用 Gio.promisify
// 这里手动封装，以确保兼容性。
const { promisify } = imports.misc.extensionUtils;

let indicator, line1, line2, updateInterval;
const STOCK_CODE = 'sh688256'; // 股票代码
let cachedData = null; // 缓存股票数据
let lastFetchTime = 0; // 上次获取数据的时间戳
let isInTradingPeriod = false; // 记录当前是否处于交易时间段

// ******************************************************
// ** 异步 Promise 封装函数 **
// ******************************************************

/**
 * 将 GIO 的 _async/_finish 模式的函数封装成返回 Promise 的函数。
 * 这是一个通用的工具函数，但在这里为了清晰，我们只封装 SocketClient.connect_to_host。
 *
 * @param {object} object - 包含方法的对象（如 Gio.SocketClient.prototype）
 * @param {string} async_method_name - 异步方法名（如 'connect_to_host_async'）
 * @returns {function} 一个返回 Promise 的函数
 */
function promisifyAsync(object, async_method_name) {
    const finish_method_name = async_method_name.replace('_async', '_finish');
    
    return function (...args) {
        return new Promise((resolve, reject) => {
            const callback = (source, res) => {
                try {
                    // 调用对应的 finish 方法
                    const result = source[finish_method_name](res);
                    resolve(result);
                } catch (e) {
                    reject(e);
                }
            };
            
            // 确保 callback 和 user_data 被传入
            args.push(callback);
            args.push(null); // user_data
            
            // 调用 async 方法
            object[async_method_name].apply(this, args);
        });
    };
}

// 封装 Gio.SocketClient.connect_to_host_async
const connectToHostPromise = promisifyAsync(
    Gio.SocketClient.prototype, 
    'connect_to_host_async'
);

// 封装 Gio.OutputStream.write_bytes_async
const writeBytesPromise = promisifyAsync(
    Gio.OutputStream.prototype,
    'write_bytes_async'
);

// 封装 Gio.InputStream.read_bytes_async
const readBytesPromise = promisifyAsync(
    Gio.InputStream.prototype,
    'read_bytes_async'
);


// ******************************************************
// ** 业务逻辑函数 **
// ******************************************************

// 判断当前时间是否在交易时间段内
function isInTradingHours() {
    // ... 保持不变
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


/**
 * 异步获取新浪股票价格数据。
 * @param {string} stockCode - 股票代码
 * @returns {Promise<string[]>} 包含股票数据的数组 Promise。
 */
async function getSinaStockPrice(stockCode) {
    const host = "hq.sinajs.cn";
    const port = 80;
    const client = new Gio.SocketClient();
    let connection = null;

    try {
        log(`[GJS] 异步连接到 ${host}:${port}...`);
        
        // 异步连接
        connection = await connectToHostPromise.call(client, host, port, null, null);

        if (!connection) {
            throw new Error("无法建立连接");
        }

        const path = `/list=${stockCode}`;
        const request = [
            `GET ${path} HTTP/1.1`,
            `Host: ${host}`,
            'User-Agent: GJS Stock Plugin', // 简化 User-Agent
            'Referer: https://finance.sina.com.cn/',
            'Connection: close',
            '',
            ''
        ].join('\r\n');

        log(`[GJS] 异步发送请求: ${request.substring(0, Math.min(request.length, 50))}...`);
        const outputStream = connection.get_output_stream();
        const requestBytes = GLib.Bytes.new(request);
        
        // 异步写入请求
        let writeResult = await writeBytesPromise.call(outputStream, requestBytes, null, null);
        
        if (writeResult <= 0) {
            throw new Error("请求发送失败");
        }

        const inputStream = connection.get_input_stream();
        const bufferSize = 4096; // 增大缓冲区
        const space = [];
        
        // 循环异步读取所有响应字节
        while (true) {
            // 异步读取
            let bytesReadChunk = await readBytesPromise.call(inputStream, bufferSize, null, null);
            
            if (!bytesReadChunk || bytesReadChunk.get_size() === 0) {
                break; // 读取完毕
            }

            let buffer = bytesReadChunk.get_data();
            
            // 原始数据是 GBK 编码，需转换。
            // 注意：GLib.convert 默认处理的是 Byte Array，而不是 GLib.Bytes
            // 这里使用 ByteArray.toString() 配合 GLib.get_charset() 的 GBK 处理
            
            let dataArray = ByteArray.fromgbytes(bytesReadChunk);
            let utf8String = GLib.convert(dataArray, 'UTF-8', 'GBK');
            
            space.push(utf8String[0].toString()); // [0]是转换后的字符串

            if (bytesReadChunk.get_size() < bufferSize) {
                break; // 读取完毕
            }
        }
        
        let resultStr = space.join('');
        
        // 查找 HTTP 响应体
        const bodyStart = resultStr.indexOf('\r\n\r\n');
        if (bodyStart === -1) {
             throw new Error("未找到 HTTP 响应体");
        }
        
        const body = resultStr.substring(bodyStart + 4);
        
        // 解析股票数据，格式: var hq_str_sh688256="名称,开盘,昨收,现价,...";
        return body.split("\"")[1].split(","); 
        
    } catch (e) {
        log(`[GJS 异步错误] ${e.message}`);
        throw e;
    } finally {
        if (connection) {
            // connection.close() 是同步的，在 finally 中运行问题不大
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
    
    // 解析股票数据 (名称, 开盘, 昨收, 现价, 最高, 最低, ... )
    const currentPrice = parseFloat(data[3]);
    const preClosePrice = parseFloat(data[2]);
    
    // 检查数值有效性
    if (isNaN(currentPrice) || isNaN(preClosePrice) || preClosePrice === 0) {
        line1.set_text('数据解析失败');
        line2.set_text('---');
        return;
    }

    const change = currentPrice - preClosePrice;
    const percentage = ((change / preClosePrice) * 100).toFixed(2);
    const textColor = change < 0 ? "#00ff00" : "#ff0000";
    
    // 更新UI
    line1.set_text(`${currentPrice.toFixed(2)}(${addSign(change.toFixed(2))})`);
    line2.set_text(`${addSign(percentage)}%`);
    line1.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
    line2.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
}

// 异步更新显示逻辑
async function updateDisplay() {
    try {
        const currentTradingStatus = isInTradingHours();
        let shouldFetch = false;
        
        // 状态变化时（强制获取一次）
        if (currentTradingStatus !== isInTradingPeriod) {
            isInTradingPeriod = currentTradingStatus;
            log(`[GJS] 交易状态变化: ${isInTradingPeriod ? '进入交易时间' : '进入非交易时间'}`);
            shouldFetch = true;
        } 
        // 交易时间：定期更新数据
        else if (currentTradingStatus) {
            shouldFetch = true;
        } 
        // 非交易时间：使用缓存数据
        else {
            if (!cachedData) {
                 // 尚未缓存，获取一次
                shouldFetch = true;
            } else {
                updateDisplayWithData(cachedData);
                log(`[GJS] 非交易时间，使用缓存数据 (上次更新于: ${new Date(lastFetchTime).toLocaleTimeString()})`);
                return; // 不进行网络请求
            }
        }
        
        if (shouldFetch) {
            // 调用异步函数并等待结果
            const freshData = await getSinaStockPrice(STOCK_CODE);
            cachedData = freshData;
            lastFetchTime = Date.now();
            updateDisplayWithData(freshData);
        }

    } catch (e) {
        log(`异步更新错误: ${e.message}`);
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
    
    // 初始化状态判断并首次更新
    isInTradingPeriod = isInTradingHours();
    updateDisplay();
    
    // 30秒检查一次
    updateInterval = GLib.timeout_add_seconds(
        GLib.PRIORITY_DEFAULT,
        30,
        () => { 
            // 异步函数在 GLib.timeout_add 中调用，不需要特殊的处理
            updateDisplay(); 
            return true; 
        }
    );
}

function disable() {
    if (updateInterval) GLib.source_remove(updateInterval);
    if (indicator) indicator.destroy();
    indicator = line1 = line2 = updateInterval = cachedData = null;
}
