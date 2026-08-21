use std::collections::VecDeque;

use crate::fetcher::StockInfo;

const HISTORY_CAPACITY: usize = 240;

#[derive(Debug)]
pub struct Stock {
    pub code: String,
    pub name: String,
    pub base_price: f64,
    pub history: VecDeque<f64>,
    pub loading: bool,
    pub error: Option<String>,
}

impl Stock {
    pub fn new(code: String) -> Self {
        Self {
            name: code.clone(),
            code,
            base_price: 0.0,
            history: VecDeque::with_capacity(HISTORY_CAPACITY),
            loading: false,
            error: None,
        }
    }

    pub fn apply(&mut self, info: StockInfo) {
        self.name = info.name;
        self.base_price = info.yesterday_price;
        if self.history.len() == HISTORY_CAPACITY {
            self.history.pop_front();
        }
        self.history.push_back(info.current_price);
        self.loading = false;
        self.error = None;
    }

    pub fn fail(&mut self, error: String) {
        self.loading = false;
        self.error = Some(error);
    }

    pub fn current_price(&self) -> Option<f64> {
        self.history.back().copied()
    }

    pub fn difference(&self) -> Option<f64> {
        Some(self.current_price()? - self.base_price)
    }

    pub fn percentage(&self) -> Option<f64> {
        (self.base_price != 0.0)
            .then(|| self.difference().unwrap_or_default() / self.base_price * 100.0)
    }
}
