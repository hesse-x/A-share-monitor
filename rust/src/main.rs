#![cfg_attr(target_os = "windows", windows_subsystem = "windows")]

mod app;
mod config;
mod fetcher;
mod model;

use std::{env, fs::File, process::ExitCode};

use app::StockApp;
use config::Config;
use eframe::egui;

fn load_config() -> Result<Config, String> {
    let Some(path) = env::args().nth(1) else {
        return Ok(Config::default());
    };
    let file = File::open(&path).map_err(|error| format!("cannot open {path}: {error}"))?;
    Config::parse(file).map_err(|error| format!("invalid config {path}: {error}"))
}

fn main() -> ExitCode {
    let config = match load_config() {
        Ok(config) => config,
        Err(error) => {
            eprintln!("{error}");
            return ExitCode::FAILURE;
        }
    };
    let initial_height = 92.0 * config.codes.len().clamp(1, 5) as f32;

    let options = eframe::NativeOptions {
        renderer: eframe::Renderer::Wgpu,
        viewport: egui::ViewportBuilder::default()
            .with_title("Stock Monitor")
            .with_inner_size([280.0, initial_height])
            .with_min_inner_size([86.0, 76.0])
            .with_decorations(false)
            .with_transparent(true)
            .with_taskbar(false)
            .with_always_on_top(),
        ..Default::default()
    };

    match eframe::run_native(
        "Stock Monitor",
        options,
        Box::new(move |cc| Ok(Box::new(StockApp::new(cc, config)))),
    ) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("failed to start Stock Monitor: {error}");
            ExitCode::FAILURE
        }
    }
}
