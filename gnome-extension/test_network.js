const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;

// 测试网络请求功能
function testNetworkRequest() {
    const host = "hq.sinajs.cn";
    const port = 80;
    const stockCode = "sh688256";

    print("=== 测试网络请求 ===");
    print(`正在请求: ${host}/list=${stockCode}`);

    try {
        const client = new Gio.SocketClient();
        const connection = client.connect_to_host(host, port, null);

        if (!connection) {
            throw new Error("连接失败");
        }

        const request = `GET /list=${stockCode} HTTP/1.1\r\n` +
                       `Host: ${host}\r\n\r\n`;

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

        connection.close(null);

        const response = chunks.join("");
        print("接收响应成功，长度:", response.length);
        print("响应内容预览:", response.substring(0, 200));

        // 解析数据
        if (response.indexOf('"') >= 0) {
            const dataPart = response.split('"')[1].split(",");
            print("\n=== 解析股票数据 ===");
            print("股票名称:", dataPart[0]);
            print("当前价格:", dataPart[3]);
            print("昨收价格:", dataPart[2]);
            print("时间戳:", dataPart[30]);
        }

    } catch (e) {
        print(`网络请求失败: ${e.message}`);
    }
}

// 运行网络测试
try {
    testNetworkRequest();
} catch (e) {
    print(`测试失败: ${e.message}`);
}