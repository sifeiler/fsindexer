<template>
    <div class="search-view-wrapper vertical-container">
        <h1 v-if="username">Hello {{username}}, is there anything I can find for you?</h1>
        <div class="horizontal-container input-group">
            <input v-model="state.query" class="input-field" placeholder="type here..." type="text"/>
            <button @click="onSearch" type="submit" name="search-submit">Search</button>
        </div>
        <p v-if="state.loading">Loading...</p>
        <div v-if="state.currentSearchResult" class="results-container">
            <ul class="results-list">
                <li 
                    v-for="r in state.currentSearchResult.results" 
                    :key="r.path"
                    class="result-item"
                    @click="openFile(r.path)"
                >
                    <span class="file-path">{{ r.path }}</span>
                </li>
            </ul>
        </div>
    </div>
</template>

<script setup lang="ts">
import { useUser } from "../composables/useUser";
import { onMounted, reactive } from "vue";
import { executeWideSearch } from "../service/SearchService";
import { SearchResult } from "../types/Search";
import { openPath } from "@tauri-apps/plugin-opener";

const { username, load } = useUser();

interface SearchViewState {
    query :string | null;
    currentSearchResult :SearchResult | null;
    loading: boolean
}

const state :SearchViewState = reactive<SearchViewState>({
    query: null,
    currentSearchResult: null,
    loading: false
});

function onSearch() {
    if (!state.query || !state.query.trim()) return;
    
    state.loading = true;
    state.currentSearchResult = null;
    //build complex query with logical ORs
    executeWideSearch(state.query).then((result :SearchResult) => {
        state.currentSearchResult = result;
    })
    .finally(() => state.loading = false);
}

function openFile(path :string) {
    openPath(path);
}

onMounted(() => load());
</script>

<style scoped>
.search-view-wrapper {
    width: 100%;
    min-height: 100vh;
    min-height: 100dvh;
}

.vertical-container {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
}

.horizontal-container {
    display: flex;
    flex-direction: row;
    align-items: stretch;
    justify-content: center;
    gap: 0.5rem;
}

.input-group input,
.input-group button {
  box-sizing: border-box;
  font-size: 1rem;
  line-height: 1.5;
  padding-block: 0.625rem; /* Same vertical padding for both */
  border-radius: 9999px;
  padding: 1em;
}

.input-field {
    width: 100vh;
}

.results-list {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 8px; /* Space between each item */
}

.result-item {
  background-color: #2a2a2a; /* Dark gray (adjust to #f3f4f6 if using light mode) */
  color: #e0e0e0;
  padding: 10px 14px;
  border-radius: 6px;
  cursor: pointer;
  border: 1px solid #3a3a3a;
  transition: background-color 0.15s ease, transform 0.1s ease;
  user-select: none;
  
  /* Ensure the full row is clickable and truncates long paths nicely */
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Hover & Active Feedback */
.result-item:hover {
  background-color: #383838; /* Slightly lighter gray on hover */
  border-color: #4a4a4a;
}

.result-item:active {
  background-color: #1f1f1f;
  transform: scale(0.99); /* Gentle press effect */
}

</style>