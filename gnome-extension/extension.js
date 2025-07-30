const St = imports.gi.St;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const ByteArray = imports.byteArray;

let indicator, line1, line2, updateInterval;
const STOCK_CODE = 'sh000001'; // Stock code
let cachedData = null; // Cached stock data
let lastFetchTime = 0; // Timestamp of last data fetch
let isInTradingPeriod = false; // Records whether current time is within trading hours

// Determine if current time is within trading hours
function isInTradingHours() {
    const now = new Date();
    const hour = now.getHours();
    const minute = now.getMinutes();
    const day = now.getDay(); // 0 is Sunday, 6 is Saturday
    
    // No trading on weekends
    if (day === 0 || day === 6) {
        return false;
    }
    
    // Morning trading session: 9:15-11:35
    const isMorningSession = (hour === 9 && minute >= 15) || 
                           (hour > 9 && hour < 11) || 
                           (hour === 11 && minute <= 35);
    
    // Afternoon trading session: 13:00-15:05
    const isAfternoonSession = (hour === 13) || 
                             (hour > 13 && hour < 15) || 
                             (hour === 15 && minute <= 5);
    
    return isMorningSession || isAfternoonSession;
}

// Implementation using Gio 2.0 compatible SocketClient
function getSinaStockPrice(stockCode) {
    const host = "hq.sinajs.cn";
    const port = 80;
    const client = new Gio.SocketClient();
    let connection = null;

    try {
        log(`[GJS] Connecting to ${host}:${port}...`);
        connection = client.connect_to_host(host, port, null);

        if (!connection) {
            throw new Error("Failed to establish connection");
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

        log(`[GJS] Sending request: ${request.substring(0, Math.min(request.length, 50))}...`);
        const outputStream = connection.get_output_stream();

        const requestBytes = GLib.Bytes.new(request);
        let writeResult = outputStream.write_bytes(requestBytes, null);
        
        if (writeResult <= 0) {
            throw new Error("Failed to send request");
        }

        const inputStream = connection.get_input_stream();
        const bufferSize = 512; 
        const space = [];
        
        // Read all response bytes in loop
        while (true) {
            let bytesReadChunk = inputStream.read_bytes(bufferSize, null);
            if (!bytesReadChunk || bytesReadChunk.get_size() === 0) {
                break; // Reading complete
            }

            let buffer = bytesReadChunk.get_data();
            space.push(GLib.convert(buffer, "UTF8", "GBK"));
            if (bytesReadChunk.get_size() < bufferSize) {
                break; // Reading complete
            }
        }
        
        let resultStr = space.toString();
        return resultStr.toString().split("\"")[1].split(","); 
    } catch (e) {
        log(`[GJS Error] ${e.message}`);
        throw e;
    } finally {
        if (connection) {
            connection.close(null);
            log("[GJS] Connection closed.");
        }
    }
}

// Add sign to numerical value
function addSign(value) {
    return value >= 0 ? `+${value}` : `${value}`;
}

// Parse and update display
function updateDisplayWithData(data) {
    if (!data || data.length < 4) {
        line1.set_text('Data error');
        line2.set_text('---');
        return;
    }
    
    // Parse stock data
    const currentPrice = parseFloat(data[3]);
    const preClosePrice = parseFloat(data[2]);
    const change = currentPrice - preClosePrice;
    const percentage = ((change / preClosePrice) * 100).toFixed(2);
    const textColor = change < 0 ? "#00ff00" : "#ff0000";
    
    // Update UI
    line1.set_text(`${currentPrice.toFixed(2)}(${addSign(change.toFixed(2))})`);
    line2.set_text(`${addSign(percentage)}%`);
    line1.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
    line2.style = `color: ${textColor}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;
}

// Update display logic
async function updateDisplay() {
    try {
        const currentTradingStatus = isInTradingHours();
        
        // When status changes (from non-trading to trading, or vice versa)
        if (currentTradingStatus !== isInTradingPeriod) {
            isInTradingPeriod = currentTradingStatus;
            log(`[GJS] Trading status changed: ${isInTradingPeriod ? 'Entering trading hours' : 'Entering non-trading hours'}`);
            
            // Force data fetch when status changes
            const freshData = getSinaStockPrice(STOCK_CODE);
            cachedData = freshData;
            lastFetchTime = Date.now();
            updateDisplayWithData(freshData);
        } 
        // Trading hours: update data regularly
        else if (currentTradingStatus) {
            const freshData = getSinaStockPrice(STOCK_CODE);
            cachedData = freshData;
            lastFetchTime = Date.now();
            updateDisplayWithData(freshData);
        } 
        // Non-trading hours: use cached data, no new fetch
        else {
            if (cachedData) {
                updateDisplayWithData(cachedData);
            } else {
                // If no cached data yet, fetch once
                const freshData = getSinaStockPrice(STOCK_CODE);
                cachedData = freshData;
                lastFetchTime = Date.now();
                updateDisplayWithData(freshData);
            }
            log(`[GJS] Non-trading hours, using cached data (last updated: ${new Date(lastFetchTime).toLocaleTimeString()})`);
        }
    } catch (e) {
        log(`Error: ${e.message}`);
        line1.set_text('Fetch failed');
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
    
    // Initialize status check
    isInTradingPeriod = isInTradingHours();
    updateDisplay();
    
    // Check every 30 seconds (updates data during trading hours, only checks status during non-trading hours)
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
