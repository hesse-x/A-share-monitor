import St from 'gi://St';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

// ByteArray bridges Uint8Array (what read_bytes_async yields in GJS 46+) and the
// byte-string type GLib.convert expects. Without it, get_data() gets toString()'d
// into "1,2,3,..." and the GBK→UTF8 decode silently produces garbage.
const ByteArray = imports.byteArray;

const STOCK_CODE = 'sh688256';
const CONNECT_TIMEOUT_SEC = 5;
const REQUEST_TIMEOUT_SEC = 8;

// Wrap blocking Gio async methods into GJS Promises so we can `await` them
// on the main loop without freezing the Shell while the socket I/O is in flight.
Gio._promisify(Gio.SocketClient.prototype, 'connect_to_host_async', 'connect_to_host_finish');
Gio._promisify(Gio.OutputStream.prototype, 'write_bytes_async', 'write_bytes_finish');
Gio._promisify(Gio.InputStream.prototype, 'read_bytes_async', 'read_bytes_finish');

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

// Fully async fetch: never blocks the GNOME Shell main loop.
// Resolves to the parsed Sina quote fields, or rejects on error/timeout.
async function getSinaStockPrice(stockCode) {
    const host = 'hq.sinajs.cn';
    const port = 80;
    const client = new Gio.SocketClient();
    client.set_timeout(CONNECT_TIMEOUT_SEC);

    // Cancellable lets us abort if the read phase drags on too long.
    const cancellable = new Gio.Cancellable();
    let connection = null;
    let timeoutId = 0;

    try {
        log(`[GJS] Connecting to ${host}:${port}...`);
        connection = await client.connect_to_host_async(host, port, cancellable);
        if (!connection)
            throw new Error('Failed to establish connection');

        // Arm a watchdog so a slow/dead server can't hold the loop open.
        timeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, REQUEST_TIMEOUT_SEC, () => {
            log(`[GJS] Request exceeded ${REQUEST_TIMEOUT_SEC}s, cancelling`);
            cancellable.cancel();
            return GLib.SOURCE_REMOVE;
        });

        const request = `GET /list=${stockCode} HTTP/1.1\r\n` +
                        `Host: ${host}\r\n` +
                        `User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36\r\n` +
                        `Referer: https://finance.sina.com.cn/\r\n` +
                        `Connection: close\r\n\r\n`;

        log(`[GJS] Sending request: ${request.substring(0, Math.min(request.length, 50))}...`);
        await connection.get_output_stream().write_bytes_async(
            GLib.Bytes.new(request), GLib.PRIORITY_DEFAULT, cancellable);

        const inputStream = connection.get_input_stream();
        const bufferSize = 512;
        const chunks = [];

        while (true) {
            const bytesRead = await inputStream.read_bytes_async(
                bufferSize, GLib.PRIORITY_DEFAULT, cancellable);
            if (!bytesRead || bytesRead.get_size() === 0)
                break;

            chunks.push(GLib.convert(ByteArray.fromUint8Array(bytesRead.get_data()), 'UTF8', 'GBK'));
            if (bytesRead.get_size() < bufferSize)
                break;
        }

        return chunks.join('').split('"')[1].split(',');
    } catch (e) {
        log(`[GJS Error] ${e.message}`);
        throw e;
    } finally {
        if (timeoutId)
            GLib.source_remove(timeoutId);
        if (connection) {
            try {
                connection.close(null);
                log('[GJS] Connection closed.');
            } catch (e) {
                // Closing an already-closed/cancelled connection is harmless.
            }
        }
    }
}

// Add sign to numerical value
function addSign(value) {
    return value >= 0 ? `+${value}` : `${value}`;
}

export default class StockTickerExtension extends Extension {
    constructor(metadata) {
        super(metadata);
        this._indicator = null;
        this._line1 = null;
        this._line2 = null;
        this._updateInterval = null;
        this._cachedData = null;
        this._lastFetchTime = 0;
        this._isInTradingPeriod = false;
        this._inFlight = false;
    }

    // Parse and update display
    _updateDisplayWithData(data) {
        if (!data || data.length < 4) {
            this._line1.set_text('Data error');
            this._line2.set_text('---');
            return;
        }

        const currentPrice = parseFloat(data[3]);
        const preClosePrice = parseFloat(data[2]);
        const change = currentPrice - preClosePrice;
        const percentage = ((change / preClosePrice) * 100).toFixed(2);
        const color = change < 0 ? '#00ff00' : '#ff0000';
        const style = `color: ${color}; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;`;

        this._line1.set_text(`${currentPrice.toFixed(2)}(${addSign(change.toFixed(2))})`);
        this._line2.set_text(`${addSign(percentage)}%`);
        this._line1.style = style;
        this._line2.style = style;
    }

    async _fetchAndUpdateData() {
        // Guard against overlapping requests if a tick fires while one is in flight.
        if (this._inFlight)
            return;
        this._inFlight = true;
        try {
            const data = await getSinaStockPrice(STOCK_CODE);
            this._cachedData = data;
            this._lastFetchTime = Date.now();
            this._updateDisplayWithData(data);
        } catch (e) {
            log(`Error: ${e.message}`);
            this._line1.set_text('Fetch failed');
            this._line2.set_text('---');
        } finally {
            this._inFlight = false;
        }
    }

    _updateDisplay() {
        try {
            const currentTradingStatus = isInTradingHours();

            if (currentTradingStatus !== this._isInTradingPeriod) {
                this._isInTradingPeriod = currentTradingStatus;
                log(`[GJS] Trading status changed: ${this._isInTradingPeriod ? 'Entering trading hours' : 'Entering non-trading hours'}`);
                this._fetchAndUpdateData();
            } else if (currentTradingStatus) {
                this._fetchAndUpdateData();
            } else {
                if (this._cachedData) {
                    this._updateDisplayWithData(this._cachedData);
                } else {
                    this._fetchAndUpdateData();
                }
                log(`[GJS] Non-trading hours, using cached data (last updated: ${new Date(this._lastFetchTime).toLocaleTimeString()})`);
            }
        } catch (e) {
            log(`Error: ${e.message}`);
            this._line1.set_text('Fetch failed');
            this._line2.set_text('---');
        }
    }

    enable() {
        this._indicator = new PanelMenu.Button(0.0);

        const container = new St.BoxLayout({
            vertical: true,
            style: 'horizontal-align: center; padding: 1px 3px; min-width: 80px;'
        });

        this._line1 = new St.Label({ text: '0.00(+0.00)', style: 'color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;' });
        this._line2 = new St.Label({ text: '+0.00%', style: 'color: #ff0000; text-align: center; font-size: 12px; font-weight: bold; padding: 0; line-height: 1.0;' });

        container.add_child(this._line1);
        container.add_child(this._line2);
        this._indicator.add_child(container);

        Main.panel.addToStatusArea('stock-ticker', this._indicator, 0, 'right');

        // Initialize status check
        this._isInTradingPeriod = isInTradingHours();
        this._updateDisplay();

        // Check every 30 seconds (updates data during trading hours, only checks status during non-trading hours)
        this._updateInterval = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            30,
            () => { this._updateDisplay(); return true; }
        );
    }

    disable() {
        if (this._updateInterval) {
            GLib.source_remove(this._updateInterval);
            this._updateInterval = null;
        }
        if (this._indicator) {
            this._indicator.destroy();
            this._indicator = null;
        }
        this._line1 = null;
        this._line2 = null;
        this._cachedData = null;
    }
}
