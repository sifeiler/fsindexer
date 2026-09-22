// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
mod backend;
mod http_client;

use backend::{spawn_backend, BackendState};

#[tauri::command]
fn current_user() -> String {
    whoami::username().unwrap_or_else(|_| "unknown".to_string())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_opener::init()) // <-- ADD THIS LINE
        .manage(BackendState::new())
        .setup(|app| {
            spawn_backend(app.handle().clone());
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            http_client::http_post,
            current_user
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}