const St = imports.gi.St;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const ByteArray = imports.byteArray;

let indicator, line1, line2, updateInterval;
const STOCK_CODE = 'sh688256';
let cachedData = null;
let lastFetchTime = 0;
let isInTradingPeriod = false;

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

        const request = `GET /list=${stockCode} HTTP/1.1\r\n` +
                       `Host: ${host}\r\n` +
                       `User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36\r\n` +
                       `Referer: https://finance.sina.com.cn/\r\n` +
                       `Connection: close\r\n\r\n`;

        log(`[GJS] Sending request: ${request.substring(0, Math.min(request.length, 50))}...`);
        connection.get_output_stream().write_bytes(GLib.Bytes.new(request), null);

        const inputStream = connection.get_input_stream();
        const bufferSize = 512;
        const chunks = [];

        while (true) {
            let bytesRead = inputStream.read_bytes(bufferSize, null);
            if (!bytesRead || bytesRead.get_size() === 0) break;

            chunks.push(GLib.convert(bytesRead.get_data(), "UTF8", "GBK"));
            if (bytesRead.get_size() < bufferSize) break;
        }

        return chunks.join("").split("\"")[1].split(",");
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

    const currentPrice = parseFloat(data[3]);
    const preClosePrice = parseFloat(data[2]);
    const change = currentPrice - preClosePrice;
    const percentage = ((change / preClosePrice) * 100).toFixed(2);
    const color = change < 0 ? "#00ff00" : "#ff0000";
    const style = `color: ${color}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;

    line1.set_text(`${currentPrice.toFixed(2)}(${addSign(change.toFixed(2))})`);
    line2.set_text(`${addSign(percentage)}%`);
    line1.style = style;
    line2.style = style;
}

function fetchAndUpdateData() {
    const data = getSinaStockPrice(STOCK_CODE);
    cachedData = data;
    lastFetchTime = Date.now();
    updateDisplayWithData(data);
}

async function updateDisplay() {
    try {
        const currentTradingStatus = isInTradingHours();

        if (currentTradingStatus !== isInTradingPeriod) {
            isInTradingPeriod = currentTradingStatus;
            log(`[GJS] Trading status changed: ${isInTradingPeriod ? 'Entering trading hours' : 'Entering non-trading hours'}`);
            fetchAndUpdateData();
        } else if (currentTradingStatus) {
            fetchAndUpdateData();
        } else {
            if (cachedData) {
                updateDisplayWithData(cachedData);
            } else {
                fetchAndUpdateData();
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
