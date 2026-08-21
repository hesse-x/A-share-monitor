use std::{
    collections::HashMap,
    sync::atomic::{AtomicU64, Ordering},
    sync::mpsc::{self, Receiver},
    thread,
    time::Duration,
};

use chrono::{Datelike, Local};
use eframe::egui;
use encoding_rs::GBK;
use reqwest::Client;
use tokio::{runtime, sync::mpsc as async_mpsc, task::JoinSet};

#[derive(Debug, Clone)]
pub struct StockInfo {
    pub name: String,
    pub current_price: f64,
    pub yesterday_price: f64,
}

#[derive(Debug)]
pub struct FetchResult {
    pub code: String,
    pub result: Result<StockInfo, String>,
}

pub struct FetchWorker {
    requests: async_mpsc::UnboundedSender<String>,
    pub results: Receiver<FetchResult>,
}

impl FetchWorker {
    pub fn spawn(context: egui::Context) -> Self {
        let (request_tx, request_rx) = async_mpsc::unbounded_channel::<String>();
        let (result_tx, result_rx) = mpsc::channel();
        thread::Builder::new()
            .name("stock-async-runtime".into())
            .spawn(move || {
                let runtime = runtime::Builder::new_multi_thread()
                    .worker_threads(2)
                    .enable_all()
                    .build()
                    .expect("failed to create Tokio runtime");
                runtime.block_on(worker_loop(request_rx, result_tx, context));
            })
            .expect("failed to create async runtime thread");
        Self {
            requests: request_tx,
            results: result_rx,
        }
    }

    pub fn request(&self, code: &str) {
        let _ = self.requests.send(code.to_owned());
    }
}

async fn worker_loop(
    mut requests: async_mpsc::UnboundedReceiver<String>,
    results: mpsc::Sender<FetchResult>,
    context: egui::Context,
) {
    let client = Client::builder()
        .timeout(Duration::from_secs(10))
        .user_agent(
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 \
             (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        )
        .build()
        .expect("failed to build HTTP client");
    let mut tasks = JoinSet::new();
    while let Some(code) = requests.recv().await {
        let client = client.clone();
        let results = results.clone();
        let context = context.clone();
        tasks.spawn(async move {
            let result = fetch_code(&client, &code).await;
            if results.send(FetchResult { code, result }).is_ok() {
                context.request_repaint();
            }
        });

        while tasks.try_join_next().is_some() {}
    }
}

async fn fetch_code(client: &Client, code: &str) -> Result<StockInfo, String> {
    if code.starts_with("test") {
        static STEP: AtomicU64 = AtomicU64::new(0);
        let step = STEP.fetch_add(1, Ordering::Relaxed);
        let movement =
            ((step.wrapping_mul(1103515245).wrapping_add(12345) % 2001) as f64 - 1000.0) / 10_000.0;
        return Ok(StockInfo {
            name: "random".into(),
            current_price: 800.0 * (1.0 + movement / 100.0),
            yesterday_price: 800.0,
        });
    }
    if code.starts_with("sh") || code.starts_with("sz") {
        return fetch_sina(client, code, "", false).await;
    }

    let (future_name, kind) = code
        .split_once('-')
        .ok_or_else(|| format!("invalid future code: {code}"))?;
    let spot_codes = HashMap::from([
        ("IH", "sh000922"),
        ("IF", "sh000300"),
        ("IC", "sh000905"),
        ("IM", "sh000852"),
    ]);
    let spot_code = spot_codes
        .get(future_name)
        .ok_or_else(|| format!("unsupported future: {future_name}"))?;
    let contract = contract_code(future_name, kind == "Next");
    let (spot, future) = tokio::try_join!(
        fetch_sina(client, spot_code, "", false),
        fetch_sina(client, &contract, "nf_", true),
    )?;
    Ok(StockInfo {
        name: contract,
        current_price: future.current_price,
        yesterday_price: spot.current_price,
    })
}

async fn fetch_sina(
    client: &Client,
    code: &str,
    prefix: &str,
    future: bool,
) -> Result<StockInfo, String> {
    let url = format!("http://hq.sinajs.cn/list={prefix}{code}");
    let response = client
        .get(url)
        .header("Referer", "https://finance.sina.com.cn/")
        .send()
        .await
        .and_then(reqwest::Response::error_for_status)
        .map_err(|error| format!("request failed: {error}"))?;
    let bytes = response.bytes().await.map_err(|error| error.to_string())?;
    let (decoded, _, _) = GBK.decode(&bytes);
    parse_sina(&decoded, future)
}

fn parse_sina(response: &str, future: bool) -> Result<StockInfo, String> {
    let start = response.find('"').ok_or("missing opening quote")? + 1;
    let end = response[start..]
        .find('"')
        .map(|offset| start + offset)
        .ok_or("missing closing quote")?;
    let fields: Vec<_> = response[start..end].split(',').collect();
    if fields.len() < 4 {
        return Err("response contains too few fields".into());
    }
    let number = |index: usize| {
        fields[index]
            .parse::<f64>()
            .map_err(|error| format!("invalid number `{}`: {error}", fields[index]))
    };
    if future {
        Ok(StockInfo {
            name: fields.last().unwrap_or(&"").to_string(),
            current_price: number(3)?,
            yesterday_price: number(0)?,
        })
    } else {
        Ok(StockInfo {
            name: fields[0].to_string(),
            current_price: number(3)?,
            yesterday_price: number(2)?,
        })
    }
}

fn contract_code(name: &str, next: bool) -> String {
    let now = Local::now();
    let mut year = now.year() % 100;
    let month = now.month() as i32;
    let mut quarter_month = month + (3 - month % 3) % 3 + if next { 3 } else { 0 };
    if quarter_month > 12 {
        quarter_month -= 12;
        year += 1;
    }
    format!("{name}{year:02}{quarter_month:02}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_stock_response() {
        let parsed = parse_sina("var hq_str_sh000001=\"Index,10,9,11,0\";", false).unwrap();
        assert_eq!(parsed.name, "Index");
        assert_eq!(parsed.current_price, 11.0);
        assert_eq!(parsed.yesterday_price, 9.0);
    }

    #[test]
    fn rejects_empty_response() {
        assert!(parse_sina("var x=\"\";", false).is_err());
    }

    #[tokio::test]
    async fn random_fetcher_is_async_and_changes_value() {
        let client = Client::new();
        let first = fetch_code(&client, "test-one").await.unwrap();
        let second = fetch_code(&client, "test-one").await.unwrap();
        assert_ne!(first.current_price, second.current_price);
        assert_eq!(first.yesterday_price, 800.0);
    }
}
