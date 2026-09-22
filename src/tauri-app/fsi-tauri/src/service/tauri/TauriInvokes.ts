import { invoke } from "@tauri-apps/api/core";
import { SearchQuery, SearchResult } from "../../types/Search";
import { FILE_SEARCH_QUERY_POST } from "../http/Endpoints";

export async function getUser(): Promise<string> {
    return await invoke<string>("current_user");
}

export async function tauriExecuteSearch(query: SearchQuery): Promise<SearchResult> {
    return await invoke<SearchResult>("http_post", {
        endpoint: FILE_SEARCH_QUERY_POST,
        query: query
    });
}