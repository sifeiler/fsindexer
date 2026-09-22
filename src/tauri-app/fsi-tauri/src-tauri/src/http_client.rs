use tauri::State;
use serde_json::Value;
use crate::backend::BackendState;

#[tauri::command]
pub async fn http_post(
    state: tauri::State<'_, BackendState>,
    endpoint: String,   //everything that comes after the port
    query: serde_json::Value,   // arrives here as parsed JSON
) -> Result<serde_json::Value, String> {
    let base_url = {
        let guard = state.base_url.lock().map_err(|e| e.to_string())?;
        guard.clone().ok_or("backend isn't ready")?
    };

    let path = if endpoint.starts_with('/') {
        endpoint
    } else {
        format!("/{endpoint}")
    };
    let url = format!("{base_url}{path}");

    println!("[RUST SENDING TO C]: {}", serde_json::to_string(&query).unwrap_or_default());

    let response = state.client
        .post(&url)
        .json(&query)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    if !response.status().is_success() {
        return Err(format!("There was an error during a POST request {}", response.status()));
    }

    // 1. Await the JSON parsing result
    let json_body: serde_json::Value = response.json().await.map_err(|e| e.to_string())?;

    // 2. Print formatted/pretty JSON
    println!("[RUST RECEIVED FROM C]:\n{}", serde_json::to_string_pretty(&json_body).unwrap_or_default());

    // 3. Return the parsed json_body
    Ok(json_body)
}