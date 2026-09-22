// src-tauri/src/backend.rs
// Claude.ai helped a lot here...

use std::sync::Mutex;
use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_shell::{process::CommandEvent, ShellExt};

pub struct BackendState {
    pub base_url: Mutex<Option<String>>,
    pub client: reqwest::Client,
}

impl BackendState {
    pub fn new() -> Self {
        Self {
            base_url: Mutex::new(None),
            client: reqwest::Client::new(),
        }
    }
}

fn pick_free_port() -> std::io::Result<u16> {
    let listener = std::net::TcpListener::bind("127.0.0.1:0")?;
    Ok(listener.local_addr()?.port())
}

pub fn spawn_backend(app: AppHandle) {
    let (mut rx, _child) = app
        .shell()
        .sidecar("indexer")
        .expect("failed to create sidecar command")
        .spawn()
        .expect("failed to spawn indexer");

    let app_handle = app.clone();

    tauri::async_runtime::spawn(async move {
        while let Some(event) = rx.recv().await {
            match event {
                CommandEvent::Stdout(line_bytes) => {
                    let line = String::from_utf8_lossy(&line_bytes);
                    println!("[C STDOUT]: {}", line); // Print stdout to Rust console
                    
                    // Check if C process printed port line
                    if let Some(port) = parse_ready_line(&line) {
                        let state = app_handle.state::<BackendState>();
                        
                        // Update app state with assigned port
                        *state.base_url.lock().unwrap() =
                            Some(format!("http://127.0.0.1:{port}"));
                        
                        // Emit event to frontend
                        let _ = app_handle.emit("indexer-up", ());
                    }
                }
                CommandEvent::Stderr(line_bytes) => {
                    eprintln!("[indexer] {}", String::from_utf8_lossy(&line_bytes));
                }
                CommandEvent::Terminated(payload) => {
                    let state = app_handle.state::<BackendState>();
                    *state.base_url.lock().unwrap() = None;
                    
                    let _ = app_handle.emit("indexer-down", payload.code);
                    break;
                }
                _ => {}
            }
        }
    });
}

fn parse_ready_line(line: &str) -> Option<u16> {
    // Matches output like "listening 127.0.0.1:51234"
    line.lines().find_map(|l| {
        l.trim()
            .strip_prefix("PORT:")
            .and_then(|p| p.parse::<u16>().ok())
    })
}