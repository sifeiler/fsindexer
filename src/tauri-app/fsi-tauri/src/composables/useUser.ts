// src/composables/useUser.ts
import { ref, readonly } from "vue";
import { getUser } from "../service/tauri/TauriInvokes";

const username = ref<string | null>(null);   // shared
let loaded = false;

export function useUser() {
  async function load() {
    if (loaded) return;
    loaded = true;
    username.value = await getUser();
  }

  return { username: readonly(username), load };
}