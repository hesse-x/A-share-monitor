use std::{
    collections::{BTreeMap, BTreeSet},
    fs,
    path::Path,
    time::{Duration, Instant},
};

use chrono::{Datelike, Local, Timelike, Weekday};
use eframe::egui::{
    self, Color32, FontData, FontDefinitions, FontFamily, Pos2, Rect, Sense, Stroke, Vec2,
};

use crate::{
    config::{Config, validate_code},
    fetcher::FetchWorker,
    model::Stock,
};

const RED: Color32 = Color32::from_rgba_premultiplied(240, 58, 45, 220);
const GREEN: Color32 = Color32::from_rgba_premultiplied(18, 184, 116, 220);
const MUTED: Color32 = Color32::from_rgba_premultiplied(188, 193, 202, 210);
const BACKGROUND: Color32 = Color32::TRANSPARENT;
const ROLL_INTERVAL: Duration = Duration::from_secs(5);

#[derive(Clone, Copy, PartialEq, Eq)]
enum DisplayMode {
    Chart,
    DataOnly,
}

pub struct StockApp {
    stocks: BTreeMap<String, Stock>,
    worker: FetchWorker,
    frequency: Duration,
    last_fetch: Instant,
    last_roll: Instant,
    roll_index: usize,
    mode: DisplayMode,
    config_open: bool,
    draft_codes: BTreeSet<String>,
    new_code: String,
    config_error: Option<String>,
    size_key: (DisplayMode, usize, bool),
}

impl StockApp {
    pub fn new(cc: &eframe::CreationContext<'_>, config: Config) -> Self {
        install_cjk_font(&cc.egui_ctx);
        let worker = FetchWorker::spawn(cc.egui_ctx.clone());
        let mut stocks = BTreeMap::new();
        for code in config.codes {
            let mut stock = Stock::new(code.clone());
            stock.loading = true;
            worker.request(&code);
            stocks.insert(code, stock);
        }
        Self {
            stocks,
            worker,
            frequency: config.frequency,
            last_fetch: Instant::now(),
            last_roll: Instant::now(),
            roll_index: 0,
            mode: DisplayMode::Chart,
            config_open: false,
            draft_codes: BTreeSet::new(),
            new_code: String::new(),
            config_error: None,
            size_key: (DisplayMode::DataOnly, usize::MAX, false),
        }
    }

    fn receive_results(&mut self) {
        while let Ok(message) = self.worker.results.try_recv() {
            let Some(stock) = self.stocks.get_mut(&message.code) else {
                continue;
            };
            match message.result {
                Ok(info) => stock.apply(info),
                Err(error) => stock.fail(error),
            }
        }
    }

    fn schedule_fetches(&mut self, force: bool) {
        if !force && (self.last_fetch.elapsed() < self.frequency || !is_trading_time()) {
            return;
        }
        for stock in self.stocks.values_mut() {
            if !stock.loading {
                stock.loading = true;
                self.worker.request(&stock.code);
            }
        }
        self.last_fetch = Instant::now();
    }

    fn roll_if_needed(&mut self) {
        let page_size = self.page_size();
        if self.stocks.len() > page_size && self.last_roll.elapsed() >= ROLL_INTERVAL {
            self.roll_index = (self.roll_index + page_size) % self.stocks.len();
            self.last_roll = Instant::now();
        }
    }

    fn page_size(&self) -> usize {
        match self.mode {
            DisplayMode::Chart => 5,
            DisplayMode::DataOnly => 1,
        }
    }

    fn visible_codes(&self) -> Vec<String> {
        let codes: Vec<_> = self.stocks.keys().cloned().collect();
        if codes.is_empty() {
            return Vec::new();
        }
        (0..self.page_size().min(codes.len()))
            .map(|offset| codes[(self.roll_index + offset) % codes.len()].clone())
            .collect()
    }

    fn update_window_size(&mut self, context: &egui::Context) {
        let key = (self.mode, self.stocks.len(), self.config_open);
        if key == self.size_key {
            return;
        }
        self.size_key = key;
        let size = if self.config_open {
            Vec2::new(380.0, 340.0)
        } else {
            match self.mode {
                DisplayMode::Chart => Vec2::new(280.0, 92.0 * self.stocks.len().clamp(1, 5) as f32),
                DisplayMode::DataOnly => Vec2::new(96.0, 92.0),
            }
        };
        context.send_viewport_cmd(egui::ViewportCommand::InnerSize(size));
    }

    fn show_context_menu(&mut self, ui: &mut egui::Ui) {
        if ui
            .selectable_label(self.mode == DisplayMode::Chart, "折线图")
            .clicked()
        {
            self.mode = DisplayMode::Chart;
            self.roll_index = 0;
            ui.close();
        }
        if ui
            .selectable_label(self.mode == DisplayMode::DataOnly, "仅数据")
            .clicked()
        {
            self.mode = DisplayMode::DataOnly;
            self.roll_index = 0;
            ui.close();
        }
        ui.separator();
        if ui.button("刷新").clicked() {
            self.schedule_fetches(true);
            ui.close();
        }
        if ui.button("配置").clicked() {
            self.open_config();
            ui.close();
        }
        if ui.button("退出").clicked() {
            ui.ctx().send_viewport_cmd(egui::ViewportCommand::Close);
        }
    }

    fn open_config(&mut self) {
        self.draft_codes = self.stocks.keys().cloned().collect();
        self.new_code.clear();
        self.config_error = None;
        self.config_open = true;
    }

    fn apply_config(&mut self) {
        self.stocks
            .retain(|code, _| self.draft_codes.contains(code));
        for code in &self.draft_codes {
            if !self.stocks.contains_key(code) {
                let mut stock = Stock::new(code.clone());
                stock.loading = true;
                self.worker.request(code);
                self.stocks.insert(code.clone(), stock);
            }
        }
        self.roll_index = 0;
        self.config_open = false;
    }

    fn show_config(&mut self, context: &egui::Context) {
        let mut open = self.config_open;
        egui::Window::new("监控配置")
            .open(&mut open)
            .resizable(false)
            .collapsible(false)
            .show(context, |ui| {
                ui.label("股票代码或期指价差");
                ui.horizontal(|ui| {
                    let edit = ui.text_edit_singleline(&mut self.new_code);
                    let submit = ui.button("添加").clicked()
                        || (edit.lost_focus()
                            && ui.input(|input| input.key_pressed(egui::Key::Enter)));
                    if submit {
                        let code = self.new_code.trim().to_owned();
                        match validate_code(&code) {
                            Ok(()) if !self.draft_codes.contains(&code) => {
                                self.draft_codes.insert(code);
                                self.new_code.clear();
                                self.config_error = None;
                            }
                            Ok(()) => self.config_error = Some("该代码已存在".into()),
                            Err(error) => self.config_error = Some(error),
                        }
                    }
                });
                if let Some(error) = &self.config_error {
                    ui.colored_label(RED, error);
                }
                ui.add_space(6.0);
                let mut remove = None;
                for code in &self.draft_codes {
                    ui.horizontal(|ui| {
                        ui.monospace(code);
                        let name = self
                            .stocks
                            .get(code)
                            .map(|stock| stock.name.as_str())
                            .unwrap_or("--");
                        ui.label(name);
                        if ui.small_button("删除").clicked() {
                            remove = Some(code.clone());
                        }
                    });
                }
                if let Some(code) = remove {
                    self.draft_codes.remove(&code);
                }
                ui.separator();
                ui.horizontal(|ui| {
                    if ui.button("应用").clicked() {
                        self.apply_config();
                    }
                    if ui.button("重置").clicked() {
                        self.draft_codes = self.stocks.keys().cloned().collect();
                        self.config_error = None;
                    }
                    if ui.button("取消").clicked() {
                        self.config_open = false;
                    }
                });
            });
        if !open {
            self.config_open = false;
        }
    }

    fn show_stock(&self, painter: &egui::Painter, rect: Rect, stock: &Stock) {
        let difference = stock.difference().unwrap_or_default();
        let color = if difference < 0.0 { GREEN } else { RED };
        match self.mode {
            DisplayMode::Chart => paint_chart_stock(painter, rect, stock, color),
            DisplayMode::DataOnly => paint_data_stock(painter, rect, stock, color),
        }
    }
}

impl eframe::App for StockApp {
    fn update(&mut self, context: &egui::Context, _frame: &mut eframe::Frame) {
        self.receive_results();
        self.schedule_fetches(false);
        self.roll_if_needed();
        self.update_window_size(context);

        let panel = egui::CentralPanel::default()
            .frame(egui::Frame::NONE.fill(BACKGROUND))
            .show(context, |ui| {
                let response = ui.interact(
                    ui.max_rect(),
                    ui.id().with("drag-surface"),
                    Sense::click_and_drag(),
                );
                let start_drag = response.hovered()
                    && ui.input(|input| input.pointer.button_pressed(egui::PointerButton::Primary));
                if start_drag {
                    context.send_viewport_cmd(egui::ViewportCommand::StartDrag);
                }
                let visible = self.visible_codes();
                if visible.is_empty() {
                    ui.centered_and_justified(|ui| ui.colored_label(MUTED, "右键添加股票"));
                } else {
                    let canvas = ui.max_rect();
                    let painter = ui.painter_at(canvas);
                    let visible_len = visible.len();
                    let item_height = canvas.height() / visible_len as f32;
                    for (index, code) in visible.into_iter().enumerate() {
                        if let Some(stock) = self.stocks.get(&code) {
                            let top = canvas.top() + item_height * index as f32;
                            let bottom = if index + 1 == visible_len {
                                canvas.bottom()
                            } else {
                                top + item_height
                            };
                            self.show_stock(
                                &painter,
                                Rect::from_min_max(
                                    Pos2::new(canvas.left(), top),
                                    Pos2::new(canvas.right(), bottom),
                                ),
                                stock,
                            );
                        }
                    }
                }
                response
            });
        panel.inner.context_menu(|ui| self.show_context_menu(ui));

        if self.config_open {
            self.show_config(context);
        }
        context.request_repaint_after(Duration::from_millis(250));
    }
}

fn paint_chart_stock(painter: &egui::Painter, rect: Rect, stock: &Stock, color: Color32) {
    let text_width = (rect.width() * 0.36).max(104.0);
    let text_rect =
        Rect::from_min_max(rect.min, Pos2::new(rect.left() + text_width, rect.bottom()));
    let chart_rect = Rect::from_min_max(
        Pos2::new(text_rect.right() + 4.0, rect.top() + 4.0),
        Pos2::new(rect.right() - 2.0, rect.bottom() - 4.0),
    );
    paint_text(painter, text_rect, stock, color);

    if stock.history.is_empty() {
        if let Some(error) = &stock.error {
            painter.text(
                chart_rect.center(),
                egui::Align2::CENTER_CENTER,
                error,
                egui::FontId::proportional(10.0),
                MUTED,
            );
        }
        return;
    }
    let (mut low, mut high) = stock.history.iter().fold(
        (stock.base_price, stock.base_price),
        |(low, high), value| (low.min(*value), high.max(*value)),
    );
    if (high - low).abs() < f64::EPSILON {
        let padding = high.abs().max(1.0) * 0.01;
        low -= padding;
        high += padding;
    }
    let count = stock.history.len();
    let points: Vec<_> = stock
        .history
        .iter()
        .enumerate()
        .map(|(index, price)| {
            let t = if count == 1 {
                1.0
            } else {
                index as f32 / (count - 1) as f32
            };
            let normalized = ((*price - low) / (high - low)) as f32;
            Pos2::new(
                egui::lerp(chart_rect.x_range(), t),
                egui::lerp(chart_rect.y_range(), 1.0 - normalized),
            )
        })
        .collect();

    let base_y = egui::lerp(
        chart_rect.y_range(),
        1.0 - ((stock.base_price - low) / (high - low)) as f32,
    );
    painter.line_segment(
        [
            Pos2::new(chart_rect.left(), base_y),
            Pos2::new(chart_rect.right(), base_y),
        ],
        Stroke::new(0.6, Color32::from_white_alpha(55)),
    );
    if points.len() >= 2 {
        let mut mesh = egui::Mesh::default();
        let transparent = Color32::from_rgba_premultiplied(color.r(), color.g(), color.b(), 0);
        for pair in points.windows(2) {
            let base = mesh.vertices.len() as u32;
            mesh.colored_vertex(
                pair[0],
                Color32::from_rgba_premultiplied(color.r(), color.g(), color.b(), 72),
            );
            mesh.colored_vertex(
                pair[1],
                Color32::from_rgba_premultiplied(color.r(), color.g(), color.b(), 72),
            );
            mesh.colored_vertex(Pos2::new(pair[0].x, chart_rect.bottom()), transparent);
            mesh.colored_vertex(Pos2::new(pair[1].x, chart_rect.bottom()), transparent);
            mesh.add_triangle(base, base + 2, base + 1);
            mesh.add_triangle(base + 1, base + 2, base + 3);
        }
        painter.add(egui::Shape::mesh(mesh));
    }
    painter.add(egui::Shape::line(points, Stroke::new(1.4, color)));
}

fn paint_data_stock(painter: &egui::Painter, rect: Rect, stock: &Stock, color: Color32) {
    paint_text(painter, rect, stock, color);
}

fn paint_text(painter: &egui::Painter, rect: Rect, stock: &Stock, color: Color32) {
    let price = stock.current_price();
    let lines = [
        stock.name.clone(),
        price
            .map(|value| format!("{value:.2}"))
            .unwrap_or_else(|| "--".into()),
        stock
            .difference()
            .map(signed_number)
            .unwrap_or_else(|| "--".into()),
        stock
            .percentage()
            .map(|value| format!("{value:+.2}%"))
            .unwrap_or_else(|| "--".into()),
    ];
    let line_height = (rect.height() / 4.4).clamp(13.0, 20.0);
    let top = rect.center().y - line_height * 2.0;
    for (index, line) in lines.iter().enumerate() {
        painter.text(
            Pos2::new(rect.center().x, top + line_height * (index as f32 + 0.5)),
            egui::Align2::CENTER_CENTER,
            line,
            egui::FontId::proportional(if index == 1 { 14.0 } else { 12.0 }),
            color,
        );
    }
}

fn signed_number(value: f64) -> String {
    format!("{value:+.2}")
}

fn is_trading_time() -> bool {
    let now = Local::now();
    if matches!(now.weekday(), Weekday::Sat | Weekday::Sun) {
        return false;
    }
    let minute = now.hour() * 60 + now.minute();
    (570..=690).contains(&minute) || (780..=900).contains(&minute)
}

fn install_cjk_font(context: &egui::Context) {
    let candidates = [
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "C:\\Windows\\Fonts\\msyh.ttc",
    ];
    let Some((path, data)) = candidates
        .iter()
        .find_map(|path| fs::read(path).ok().map(|data| (*path, data)))
    else {
        return;
    };
    let mut fonts = FontDefinitions::default();
    fonts
        .font_data
        .insert("cjk".into(), FontData::from_owned(data).into());
    for family in [FontFamily::Proportional, FontFamily::Monospace] {
        fonts.families.entry(family).or_default().push("cjk".into());
    }
    context.set_fonts(fonts);
    eprintln!("loaded CJK font from {}", Path::new(path).display());
}
